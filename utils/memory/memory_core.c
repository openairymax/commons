// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file memory.c
 * @brief Unified memory management module - core layer implementation.
 *
 * Provides safe, efficient, unified memory management with allocation,
 * free, debug and stats support.
 *
 * @note AIRY_COMPLIANCE_IMPL: memory subsystem
 * Using libc malloc/free/realloc directly is by design: the AIRY_MALLOC/
 * AIRY_FREE macros ultimately call memory_alloc/memory_free from this
 * file, so using the macros here would create a circular dependency.
 * fprintf(stderr) is used for leak reports and debug dumps because it
 * runs during program shutdown when the logging system may already be
 * torn down; writing directly to stderr is the safest choice.
 */

#include "memory_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Module internal state (definition; type declared in memory_internal.h).
 */
memory_state_t g_state = {0};

bool memory_lock_init(void)
{
    return airy_mtx_init(&g_state.lock) == 0;
}

void memory_lock_destroy(void)
{
    airy_mtx_destroy(&g_state.lock);
}

void memory_lock(void)
{
    airy_mtx_lock(&g_state.lock);
}

void memory_unlock(void)
{
    airy_mtx_unlock(&g_state.lock);
}

static void memory_handle_fail(size_t size, const char *tag)
{
    if (g_state.fail_callback != NULL) {
        g_state.fail_callback(size, tag, g_state.fail_callback_user_data);
    }

    switch (g_state.options.fail_strategy) {
    case MEMORY_FAIL_STRATEGY_ABORT:
        AIRY_LOG_ERROR("内存分配失败：size=%zu, tag=%s", size, tag ? tag : "(null)");
        abort();
        break;

    case MEMORY_FAIL_STRATEGY_RETRY:
        break;

    case MEMORY_FAIL_STRATEGY_CALLBACK:
        break;

    case MEMORY_FAIL_STRATEGY_RETURN_NULL:
    default:
        break;
    }
}

static void memory_add_debug_info(void *addr, size_t size, size_t alignment, const char *tag,
                                  const char *file, int line, const char *function)
{
    if (!g_state.debug_enabled || addr == NULL) {
        return;
    }

    struct memory_debug_info *info = malloc(sizeof(struct memory_debug_info));
    if (info == NULL) {
        return;
    }

    info->address = addr;
    info->size = size;
    info->alignment = alignment;
    info->tag = tag ? strdup(tag) : NULL;
    info->file = file ? strdup(file) : NULL;
    info->line = line;
    info->function = function ? strdup(function) : NULL;
    info->timestamp = airy_time_wall_ms();
    info->next = g_state.debug_list_head;
    g_state.debug_list_head = info;
}

static void memory_remove_debug_info(void *addr)
{
    if (!g_state.debug_enabled || addr == NULL) {
        return;
    }

    struct memory_debug_info **prev = &g_state.debug_list_head;
    struct memory_debug_info *current = g_state.debug_list_head;

    while (current != NULL) {
        if (current->address == addr) {
            *prev = current->next;

            if (current->tag)
                free((void *)current->tag);
            if (current->file)
                free((void *)current->file);
            if (current->function)
                free((void *)current->function);
            free(current);

            return;
        }

        prev = &current->next;
        current = current->next;
    }
}

struct memory_debug_info *memory_find_debug_info(void *addr)
{
    if (!g_state.debug_enabled || addr == NULL) {
        return NULL;
    }

    struct memory_debug_info *current = g_state.debug_list_head;
    while (current != NULL) {
        if (current->address == addr) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

static void memory_update_stats_alloc(size_t size)
{
    g_state.stats.total_allocated += size;
    g_state.stats.current_allocated += size;
    g_state.stats.allocation_count++;

    if (g_state.stats.current_allocated > g_state.stats.peak_allocated) {
        g_state.stats.peak_allocated = g_state.stats.current_allocated;
    }
}

static void memory_update_stats_free(size_t size)
{
    g_state.stats.total_freed += size;
    g_state.stats.current_allocated -= size;
    g_state.stats.free_count++;
}

static void *memory_allocate_internal(size_t size, const char *tag, bool zero, size_t alignment)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = NULL;

    /*
     * Windows 普通通道一律 CRT malloc（x64 已保证 16 字节对齐），仅显式
     * 对齐需求走 _aligned_malloc，其产物必须以 memory_aligned_free 释放。
     * 历史方案曾统一走 _aligned_malloc/_aligned_free，但 CRT 跨界指针
     * （strdup 等）经 memory_free 释放时读不到对齐头，free 出错误基址，
     * 触发 0xc0000374 堆损坏（run 36381608450 windows cdb 栈实证）。
     * 现与 corekern airy_mem_aligned_free 配对范式对齐。
     */
#ifdef _WIN32
    ptr = (alignment > 0) ? _aligned_malloc(size, alignment) : malloc(size);
#else
    if (alignment > 0) {
        if (posix_memalign(&ptr, alignment, size) != 0) {
            ptr = NULL;
        }
    } else {
        ptr = malloc(size);
    }
#endif

    if (ptr == NULL) {
        memory_handle_fail(size, tag);
        return NULL;
    }

    if (zero || g_state.options.zero_memory) {
        __builtin_memset(ptr, 0, size);
    }

    memory_update_stats_alloc(size);

    if (g_state.debug_enabled) {
        memory_add_debug_info(ptr, size, alignment, tag, __FILE__, __LINE__, __func__);
    }

    return ptr;
}

bool memory_init(const memory_options_t *options)
{
    if (g_state.initialized) {
        return true;
    }

    AIRY_LOG_INFO("memory: memory_init (zero_memory=%s, alignment=%zu)",
             options && options->zero_memory ? "true" : "false", options ? options->alignment : 0);

    if (!memory_lock_init()) {
        return false;
    }

    memory_lock();

    if (options != NULL) {
        __builtin_memcpy(&g_state.options, options, sizeof(memory_options_t));
    }

    g_state.initialized = true;
    g_state.debug_enabled = false;
    g_state.debug_list_head = NULL;
    g_state.fail_callback = NULL;
    g_state.fail_callback_user_data = NULL;

    memory_unlock();

    return true;
}

void memory_cleanup(void)
{
    if (!g_state.initialized) {
        return;
    }

    AIRY_LOG_INFO(
        "memory: memory_cleanup (total_alloc=%zu, current=%zu, peak=%zu, allocs=%zu, frees=%zu)",
        g_state.stats.total_allocated, g_state.stats.current_allocated,
        g_state.stats.peak_allocated, g_state.stats.allocation_count, g_state.stats.free_count);

    memory_lock();

    if (g_state.debug_enabled && g_state.debug_list_head != NULL) {
        AIRY_LOG_WARN("警告：内存清理时发现未释放的内存块");

        struct memory_debug_info *current = g_state.debug_list_head;
        size_t leak_count = 0;
        size_t leak_size = 0;

        while (current != NULL) {
            leak_count++;
            leak_size += current->size;
            AIRY_LOG_WARN("Leak: %p (%zu bytes) - tag: %s", current->address, current->size,
                     current->tag ? current->tag : "(null)");

            // AIRY_FREE(current->address);

            struct memory_debug_info *next = current->next;

            if (current->tag)
                free((void *)current->tag);
            if (current->file)
                free((void *)current->file);
            if (current->function)
                free((void *)current->function);
            free(current);

            current = next;
        }

        AIRY_LOG_WARN("Total leaks: %zu blocks, %zu bytes", leak_count, leak_size);
        g_state.debug_list_head = NULL;
    }

    g_state.initialized = false;

    memory_unlock();

    memory_lock_destroy();
}

void *memory_alloc(size_t size, const char *tag)
{
    if (!g_state.initialized) {
        void *ptr = malloc(size);
        if (ptr != NULL) {
            __builtin_memset(ptr, 0, size);
        }
        return ptr;
    }

    memory_lock();
    void *ptr = memory_allocate_internal(size, tag, false, 0);
    memory_unlock();

    return ptr;
}

void *memory_calloc(size_t size, const char *tag)
{
    if (!g_state.initialized) {
        void *ptr = malloc(size);
        if (ptr != NULL) {
            __builtin_memset(ptr, 0, size);
        }
        return ptr;
    }

    memory_lock();
    void *ptr = memory_allocate_internal(size, tag, true, 0);
    memory_unlock();

    return ptr;
}

void *memory_aligned_alloc(size_t alignment, size_t size, const char *tag)
{
    if (!g_state.initialized) {
        /* 对齐通道：产物必须以 memory_aligned_free 释放 */
#ifdef _WIN32
        void *ptr = _aligned_malloc(size, alignment);
        if (ptr != NULL) {
            __builtin_memset(ptr, 0, size);
        }
        return ptr;
#else
        void *ptr = NULL;
        if (posix_memalign(&ptr, alignment, size) != 0) {
            return NULL;
        }
        if (ptr != NULL) {
            __builtin_memset(ptr, 0, size);
        }
        return ptr;
#endif
    }

    memory_lock();
    void *ptr = memory_allocate_internal(size, tag, g_state.options.zero_memory, alignment);
    memory_unlock();

    return ptr;
}

/* realloc 期间用于续接调试记录的本地快照：必须在摘除 debug 条目之前复制字段。 */
typedef struct {
    size_t alignment;
    int line;
    char tag[64];
    char file[128];
    char func[64];
} memory_trace_t;

/* 重新登记 realloc 后指针的调试信息；saved 为 NULL 时按调用点信息新建。 */
static void memory_retrack(void *ptr, size_t size, const char *tag, const memory_trace_t *saved)
{
    if (!g_state.debug_enabled) {
        return;
    }

    if (saved != NULL) {
        memory_add_debug_info(ptr, size, saved->alignment,
                              saved->tag[0] ? saved->tag : tag,
                              saved->file[0] ? saved->file : __FILE__,
                              saved->line > 0 ? saved->line : __LINE__,
                              saved->func[0] ? saved->func : __func__);
        return;
    }

    memory_add_debug_info(ptr, size, sizeof(void *), tag, __FILE__, __LINE__, __func__);
}

void *memory_realloc(void *ptr, size_t new_size, const char *tag)
{
    if (ptr == NULL) {
        return memory_alloc(new_size, tag);
    }

    if (new_size == 0) {
        memory_free(ptr);
        return NULL;
    }

    if (!g_state.initialized) {
        return realloc(ptr, new_size);
    }

    memory_lock();

    struct memory_debug_info *debug_info = memory_find_debug_info(ptr);
    size_t old_size = debug_info ? debug_info->size : 0;

    void *old_ptr = ptr;
    memory_trace_t saved = {0};
    bool debug_info_saved = false;
    if (debug_info && g_state.debug_enabled) {
        debug_info_saved = true;
        saved.alignment = debug_info->alignment;
        saved.line = debug_info->line;
        if (debug_info->tag)
            AIRY_STRNCPY_TERM(saved.tag, debug_info->tag, sizeof(saved.tag));
        if (debug_info->file)
            AIRY_STRNCPY_TERM(saved.file, debug_info->file, sizeof(saved.file));
        if (debug_info->function)
            AIRY_STRNCPY_TERM(saved.func, debug_info->function, sizeof(saved.func));
        memory_remove_debug_info(old_ptr);
    }

    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL) {
        if (debug_info_saved) {
            memory_retrack(old_ptr, old_size, tag, &saved);
        }
        memory_handle_fail(new_size, tag);
        memory_unlock();
        return NULL;
    }

    if (new_ptr != ptr) {
        if (old_size > 0) {
            memory_update_stats_free(old_size);
        }
        memory_update_stats_alloc(new_size);
    } else {
        if (new_size > old_size) {
            memory_update_stats_alloc(new_size - old_size);
        } else if (new_size < old_size) {
            memory_update_stats_free(old_size - new_size);
        }
    }

    memory_retrack(new_ptr, new_size, tag, debug_info_saved ? &saved : NULL);

    memory_unlock();

    return new_ptr;
}

static void memory_free_raw(void *ptr, bool aligned)
{
#ifdef _WIN32
    if (aligned) {
        _aligned_free(ptr);
    } else {
        free(ptr);
    }
#else
    (void)aligned;
    free(ptr);
#endif
}

static void memory_release(void *ptr, bool aligned)
{
    if (ptr == NULL) {
        return;
    }

    if (!g_state.initialized) {
        memory_free_raw(ptr, aligned);
        return;
    }

    memory_lock();

    struct memory_debug_info *debug_info = memory_find_debug_info(ptr);
    size_t size = debug_info ? debug_info->size : 0;

    /* Remove debug info first (must happen before free, else use-after-free) */
    if (g_state.debug_enabled) {
        memory_remove_debug_info(ptr);
    }

    memory_free_raw(ptr, aligned);

    if (size > 0) {
        memory_update_stats_free(size);
    }

    memory_unlock();
}

void memory_free(void *ptr)
{
    memory_release(ptr, false);
}

void memory_aligned_free(void *ptr)
{
    memory_release(ptr, true);
}
