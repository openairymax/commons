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

#include "../include/airy_memory.h"
#include "logging.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <stdint.h>
#include <sys/time.h>
#endif

/**
 * @brief Module internal state.
 */
typedef struct {
    bool initialized;
    bool debug_enabled;
    memory_options_t options;

    memory_stats_t stats;

    /* Thread synchronization */
    airy_mtx_t lock;

    struct memory_debug_info *debug_list_head;

    void (*fail_callback)(size_t size, const char *tag, void *user_data);
    void *fail_callback_user_data;
} memory_state_t;

static memory_state_t g_state = {0};

static bool memory_lock_init(void)
{
    return airy_mtx_init(&g_state.lock) == 0;
}

static void memory_lock_destroy(void)
{
    airy_mtx_destroy(&g_state.lock);
}

static void memory_lock(void)
{
    airy_mtx_lock(&g_state.lock);
}

static void memory_unlock(void)
{
    airy_mtx_unlock(&g_state.lock);
}

static uint64_t memory_get_timestamp(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ts = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ts / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void memory_handle_fail(size_t size, const char *tag)
{
    if (g_state.fail_callback != NULL) {
        g_state.fail_callback(size, tag, g_state.fail_callback_user_data);
    }

    switch (g_state.options.fail_strategy) {
    case MEMORY_FAIL_STRATEGY_ABORT:
        LOG_ERROR("内存分配失败：size=%zu, tag=%s", size, tag ? tag : "(null)");
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
    info->timestamp = memory_get_timestamp();
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

static struct memory_debug_info *memory_find_debug_info(void *addr)
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
     * On Windows use _aligned_malloc uniformly: alignment=0 falls back to
     * sizeof(void*) default alignment, so every allocation goes through
     * the _aligned_free/_aligned_realloc path, eliminating the mismatch
     * where free() releases _aligned_malloc memory (C-5). On POSIX,
     * posix_memalign-allocated memory can be freed with free(), keeping
     * the original logic.
     */
#ifdef _WIN32
    size_t effective_alignment = (alignment > 0) ? alignment : sizeof(void *);
    ptr = _aligned_malloc(size, effective_alignment);
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
#ifdef _WIN32
        memory_add_debug_info(ptr, size, effective_alignment, tag, __FILE__, __LINE__, __func__);
#else
        memory_add_debug_info(ptr, size, alignment, tag, __FILE__, __LINE__, __func__);
#endif
    }

    return ptr;
}

bool memory_init(const memory_options_t *options)
{
    if (g_state.initialized) {
        return true;
    }

    LOG_INFO("memory: memory_init (zero_memory=%s, alignment=%zu)",
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

    LOG_INFO(
        "memory: memory_cleanup (total_alloc=%zu, current=%zu, peak=%zu, allocs=%zu, frees=%zu)",
        g_state.stats.total_allocated, g_state.stats.current_allocated,
        g_state.stats.peak_allocated, g_state.stats.allocation_count, g_state.stats.free_count);

    memory_lock();

    if (g_state.debug_enabled && g_state.debug_list_head != NULL) {
        LOG_WARN("警告：内存清理时发现未释放的内存块");

        struct memory_debug_info *current = g_state.debug_list_head;
        size_t leak_count = 0;
        size_t leak_size = 0;

        while (current != NULL) {
            leak_count++;
            leak_size += current->size;
            LOG_WARN("Leak: %p (%zu bytes) - tag: %s", current->address, current->size,
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

        LOG_WARN("Total leaks: %zu blocks, %zu bytes", leak_count, leak_size);
        g_state.debug_list_head = NULL;
    }

    g_state.initialized = false;

    memory_unlock();

    memory_lock_destroy();
}

void *memory_alloc(size_t size, const char *tag)
{
    if (!g_state.initialized) {
#ifdef _WIN32
        void *ptr = _aligned_malloc(size, sizeof(void *));
#else
        void *ptr = malloc(size);
#endif
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
#ifdef _WIN32
        void *ptr = _aligned_malloc(size, sizeof(void *));
        if (ptr != NULL) {
            __builtin_memset(ptr, 0, size);
        }
        return ptr;
#else
        return calloc(1, size);
#endif
    }

    memory_lock();
    void *ptr = memory_allocate_internal(size, tag, true, 0);
    memory_unlock();

    return ptr;
}

void *memory_aligned_alloc(size_t alignment, size_t size, const char *tag)
{
    if (!g_state.initialized) {
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
#ifdef _WIN32
        return _aligned_realloc(ptr, new_size, sizeof(void *));
#else
        return realloc(ptr, new_size);
#endif
    }

    memory_lock();

    struct memory_debug_info *debug_info = memory_find_debug_info(ptr);
    size_t old_size = debug_info ? debug_info->size : 0;

    size_t saved_alignment = debug_info ? debug_info->alignment : sizeof(void *);

    void *old_ptr = ptr;
    bool debug_info_saved = false;
    char saved_tag[64] = {0};
    char saved_file[128] = {0};
    char saved_func[64] = {0};
    int saved_line = 0;
    if (debug_info && g_state.debug_enabled) {
        debug_info_saved = true;
        if (debug_info->tag)
            AIRY_STRNCPY_TERM(saved_tag, debug_info->tag, sizeof(saved_tag));
        if (debug_info->file)
            AIRY_STRNCPY_TERM(saved_file, debug_info->file, sizeof(saved_file));
        if (debug_info->function)
            AIRY_STRNCPY_TERM(saved_func, debug_info->function, sizeof(saved_func));
        saved_line = debug_info->line;
        saved_alignment = debug_info->alignment;
        memory_remove_debug_info(old_ptr);
    }

#ifdef _WIN32
    void *new_ptr = _aligned_realloc(ptr, new_size, saved_alignment);
#else
    void *new_ptr = realloc(ptr, new_size);
#endif
    if (new_ptr == NULL) {

        if (debug_info_saved && g_state.debug_enabled) {
            memory_add_debug_info(old_ptr, old_size, saved_alignment,
                                  saved_tag[0] ? saved_tag : tag,
                                  saved_file[0] ? saved_file : __FILE__,
                                  saved_line > 0 ? saved_line : __LINE__,
                                  saved_func[0] ? saved_func : __func__);
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

        if (g_state.debug_enabled) {
            if (debug_info_saved) {
                memory_add_debug_info(new_ptr, new_size, saved_alignment,
                                      saved_tag[0] ? saved_tag : tag,
                                      saved_file[0] ? saved_file : __FILE__,
                                      saved_line > 0 ? saved_line : __LINE__,
                                      saved_func[0] ? saved_func : __func__);
            } else {
                memory_add_debug_info(new_ptr, new_size, sizeof(void *), tag, __FILE__, __LINE__,
                                      __func__);
            }
        }
    } else {
        if (new_size > old_size) {
            memory_update_stats_alloc(new_size - old_size);
        } else if (new_size < old_size) {
            memory_update_stats_free(old_size - new_size);
        }

        if (g_state.debug_enabled) {
            if (debug_info_saved) {
                memory_add_debug_info(new_ptr, new_size, saved_alignment,
                                      saved_tag[0] ? saved_tag : tag,
                                      saved_file[0] ? saved_file : __FILE__,
                                      saved_line > 0 ? saved_line : __LINE__,
                                      saved_func[0] ? saved_func : __func__);
            } else {
                memory_add_debug_info(new_ptr, new_size, sizeof(void *), tag, __FILE__, __LINE__,
                                      __func__);
            }
        }
    }

    memory_unlock();

    return new_ptr;
}

void memory_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    if (!g_state.initialized) {
#ifdef _WIN32
        _aligned_free(ptr);
#else
        free(ptr);
#endif
        return;
    }

    memory_lock();

    struct memory_debug_info *debug_info = memory_find_debug_info(ptr);
    size_t size = debug_info ? debug_info->size : 0;

    /* Remove debug info first (must happen before free, else use-after-free) */
    if (g_state.debug_enabled) {
        memory_remove_debug_info(ptr);
    }

    /*
     * Free: on Windows every allocation (including alignment=0 regular
     * ones) goes through _aligned_malloc, so always use _aligned_free
     * (C-5). On POSIX, memory from posix_memalign/malloc can be freed
     * with free().
     */
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif

    if (size > 0) {
        memory_update_stats_free(size);
    }

    memory_unlock();
}

bool memory_get_stats(memory_stats_t *stats)
{
    if (stats == NULL) {
        return false;
    }

    if (!g_state.initialized) {
        __builtin_memset(stats, 0, sizeof(memory_stats_t));
        return true;
    }

    memory_lock();
    __builtin_memcpy(stats, &g_state.stats, sizeof(memory_stats_t));

    if (g_state.debug_enabled) {
        struct memory_debug_info *current = g_state.debug_list_head;
        size_t leak_count = 0;
        while (current != NULL) {
            leak_count++;
            current = current->next;
        }
        stats->leak_count = leak_count;
    }

    memory_unlock();

    return true;
}

void memory_reset_stats(void)
{
    if (!g_state.initialized) {
        return;
    }

    memory_lock();
    __builtin_memset(&g_state.stats, 0, sizeof(memory_stats_t));
    memory_unlock();
}

bool memory_debug_enable(bool enable)
{
    if (!g_state.initialized) {
        return false;
    }

    LOG_INFO("memory: memory_debug_enable (enable=%s, prev=%s)", enable ? "true" : "false",
             g_state.debug_enabled ? "true" : "false");

    memory_lock();

    if (enable == g_state.debug_enabled) {
        memory_unlock();
        return true;
    }

    g_state.debug_enabled = enable;

    if (!enable && g_state.debug_list_head != NULL) {
        struct memory_debug_info *current = g_state.debug_list_head;
        while (current != NULL) {
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
        g_state.debug_list_head = NULL;
    }

    memory_unlock();

    return true;
}

size_t memory_check_leaks(bool dump_to_stderr)
{
    if (!g_state.initialized || !g_state.debug_enabled) {
        return 0;
    }

    memory_lock();

    size_t leak_size = 0;
    size_t leak_count = 0;
    struct memory_debug_info *current = g_state.debug_list_head;

    struct memory_debug_info *tmp = current;
    while (tmp != NULL) {
        leak_count++;
        tmp = tmp->next;
    }

    if (dump_to_stderr && current != NULL) {
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(stderr, "=== Memory Leak Detection Report ===\n");
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(stderr, "Time: %llu\n", (unsigned long long)memory_get_timestamp());
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(stderr, "Current allocated: %zu bytes\n", g_state.stats.current_allocated);
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(stderr, "Leak blocks: %zu\n", leak_count);
    }

    while (current != NULL) {
        leak_size += current->size;

        if (dump_to_stderr) {
            /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
            fprintf(stderr, "  %p: %zu字节", current->address, current->size);
            if (current->tag) {
                /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                fprintf(stderr, " [%s]", current->tag);
            }
            if (current->file) {
                /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
                fprintf(stderr, " (%s:%d)", current->file, current->line);
            }
            /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
            fprintf(stderr, "\n");
        }

        current = current->next;
    }

    if (dump_to_stderr && leak_size > 0) {
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(stderr, "Total leaks: %zu bytes\n", leak_size);
        /* BAN-70 EXEMPT: diagnostic report output to configurable FILE* stream */
        fprintf(stderr, "========================\n");
    }

    memory_unlock();

    return leak_size;
}

void memory_dump_debug_info(const char *file)
{
    if (!g_state.initialized || !g_state.debug_enabled) {
        return;
    }

    memory_lock();

    FILE *output = file ? fopen(file, "w") : stderr;
    if (output == NULL) {
        memory_unlock();
        return;
    }

    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "=== Memory Debug Info Dump ===\n");
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "Timestamp: %llu\n", (unsigned long long)memory_get_timestamp());
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "Current allocation blocks:\n");

    struct memory_debug_info *current = g_state.debug_list_head;
    size_t count = 0;

    while (current != NULL) {
        count++;
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "  [#%zu]:\n", count);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "    address: %p\n", current->address);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "    size: %zu bytes\n", current->size);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "    tag: %s\n", current->tag ? current->tag : "(null)");
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "    location: %s:%d (%s)\n", current->file ? current->file : "(unknown)",
                current->line, current->function ? current->function : "(unknown)");
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "    timestamp: %llu\n", (unsigned long long)current->timestamp);
        /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
        fprintf(output, "\n");

        current = current->next;
    }

    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "Total: %zu memory blocks\n", count);
    /* BAN-70 EXEMPT: memory diagnostic report/dump output to configurable FILE* stream */
    fprintf(output, "=======================\n");

    if (file) {
        fclose(output);
    }

    memory_unlock();
}

bool memory_validate(void *ptr)
{
    if (!g_state.initialized || !g_state.debug_enabled || ptr == NULL) {
        return true;
    }

    memory_lock();

    bool valid = (memory_find_debug_info(ptr) != NULL);

    memory_unlock();

    return valid;
}

void memory_set_fail_callback(void (*callback)(size_t size, const char *tag, void *user_data),
                              void *user_data)
{

    if (!g_state.initialized) {
        return;
    }

    memory_lock();

    g_state.fail_callback = callback;
    g_state.fail_callback_user_data = user_data;

    memory_unlock();
}

size_t memory_get_current_usage(void)
{
    if (!g_state.initialized) {
        return 0;
    }

    memory_lock();
    size_t usage = g_state.stats.current_allocated;
    memory_unlock();

    return usage;
}

size_t memory_get_peak_usage(void)
{
    if (!g_state.initialized) {
        return 0;
    }

    memory_lock();
    size_t peak = g_state.stats.peak_allocated;
    memory_unlock();

    return peak;
}