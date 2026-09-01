/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file resource_guard.h
 * @brief Resource scope guard - RAII pattern implementation.
 *
 * Provides automatic resource release so resources are correctly freed
 * when a scope ends. Custom cleanup functions are supported; suitable
 * for file handles, memory, locks, network connections, etc.
 *
 * Usage example:
 * @code
 * FILE* file = fopen("test.txt", "r");
 * AIRY_SCOPE_EXIT(file, fclose);  // auto-closed at scope end
 *
 * void* buffer = malloc(1024);
 * AIRY_SCOPE_EXIT(buffer, free);  // auto-freed at scope end
 * @endcode
 */

#ifndef AIRY_RT_RESOURCE_GUARD_H
#define AIRY_RT_RESOURCE_GUARD_H

#include <stddef.h>
#include <stdint.h>

#include "../memory/airy_memory.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Resource cleanup function type.
 * @param resource Resource pointer
 */
typedef void (*airy_resource_cleanup_t)(void *resource);

/**
 * @brief Resource guard structure.
 */
typedef struct airy_resource_guard {
    void *resource;
    airy_resource_cleanup_t cleanup;
    const char *file;
    int line;
    const char *name;
    int active;
} airy_resource_guard_t;


/**
 * @brief Initialize a resource guard.
 * @param guard [out] Guard structure pointer
 * @param resource [in] Resource pointer
 * @param cleanup [in] Cleanup function
 * @param file [in] File name
 * @param line [in] Line number
 * @param name [in] Resource name
 */
void airy_resource_guard_init(airy_resource_guard_t *guard, void *resource,
                              airy_resource_cleanup_t cleanup, const char *file, int line,
                              const char *name);

/**
 * @brief Run the resource cleanup.
 * @param guard [in] Guard structure pointer
 */
void airy_resource_guard_cleanup(airy_resource_guard_t *guard);

/**
 * @brief Cancel the resource cleanup (transfer ownership).
 * @param guard [in] Guard structure pointer
 */
void airy_resource_guard_dismiss(airy_resource_guard_t *guard);


#ifdef AIRY_RESOURCE_TRACKING

/**
 * @brief Resource tracking record.
 */
typedef struct airy_resource_record {
    void *resource;
    const char *type;
    const char *file;
    int line;
    uint64_t timestamp_ns;
    struct airy_resource_record *next;
} airy_resource_record_t;

/**
 * @brief Register a resource allocation.
 * @param resource Resource pointer
 * @param type Resource type
 * @param file File name
 * @param line Line number
 */
void airy_resource_track_alloc(void *resource, const char *type, const char *file, int line);

/**
 * @brief Unregister a resource allocation.
 * @param resource Resource pointer
 */
void airy_resource_track_free(void *resource);

/**
 * @brief Get the resource tracking report.
 * @param out_report [out] Output report string (caller must free)
 * @return Number of unreleased resources
 */
int airy_resource_track_report(char **out_report);

/**
 * @brief Clear the resource tracking records.
 */
void airy_resource_track_clear(void);

#endif /* AIRY_RESOURCE_TRACKING */

/**
 * @brief Create a scope guard (auto-generated variable name).
 * @param resource Resource pointer
 * @param cleanup Cleanup function
 */
#define AIRY_SCOPE_GUARD(resource, cleanup)                                               \
    airy_resource_guard_t AIRY_UNIQUE_NAME(_guard)                                        \
        AIRY_ATTRIBUTE((cleanup(airy_resource_guard_cleanup))) = {.resource = (resource), \
                                                                  .cleanup = (cleanup),   \
                                                                  .file = __FILE__,       \
                                                                  .line = __LINE__,       \
                                                                  .name = #resource,      \
                                                                  .active = 1}

/**
 * @brief Create a scope guard (with custom cleanup).
 * @param resource Resource pointer
 * @param cleanup Cleanup function
 */
#define AIRY_SCOPE_EXIT(resource, cleanup)                                                \
    airy_resource_guard_t AIRY_UNIQUE_NAME(_scope_exit)                                   \
        AIRY_ATTRIBUTE((cleanup(airy_resource_guard_cleanup))) = {.resource = (resource), \
                                                                  .cleanup = (cleanup),   \
                                                                  .file = __FILE__,       \
                                                                  .line = __LINE__,       \
                                                                  .name = #resource,      \
                                                                  .active = 1}

/**
 * @brief Dismiss a scope guard (transfer ownership).
 * @param resource Resource pointer
 */
#define AIRY_SCOPE_DISMISS(resource)                                 \
    do {                                                             \
        airy_resource_guard_dismiss(&AIRY_UNIQUE_NAME(_scope_exit)); \
    } while (0)

/**
 * @brief Generate a unique variable name.
 */
#define AIRY_UNIQUE_NAME(prefix) AIRY_CONCAT(prefix, __LINE__)

/* AIRY_CONCAT needs two expansion layers so __LINE__ expands before
 * concatenation. Some headers (types.h, compat.h) define a single-layer
 * a##b version, which leaves __LINE__ unexpanded. Force the two-layer
 * version here to avoid redefinition warnings and expand correctly. */
#ifdef AIRY_CONCAT
#undef AIRY_CONCAT
#endif
#define AIRY_CONCAT(a, b) AIRY_CONCAT_IMPL(a, b)

#define AIRY_CONCAT_IMPL(a, b) a##b


#ifdef AIRY_RESOURCE_TRACKING

/**
 * @brief Track a memory allocation.
 */
#define AIRY_TRACK_ALLOC(ptr, type) airy_resource_track_alloc(ptr, type, __FILE__, __LINE__)

/**
 * @brief Track a memory free.
 */
#define AIRY_TRACK_FREE(ptr) airy_resource_track_free(ptr)

/**
 * @brief Tracked memory allocation.
 */
#define AIRY_TRACKED_MALLOC(size)             \
    ({                                        \
        void *_ptr = AIRY_MALLOC(size);       \
        if (_ptr)                             \
            AIRY_TRACK_ALLOC(_ptr, "memory"); \
        _ptr;                                 \
    })

/**
 * @brief Tracked memory free.
 */
#define AIRY_TRACKED_FREE(ptr)    \
    do {                          \
        if (ptr) {                \
            AIRY_TRACK_FREE(ptr); \
            AIRY_FREE(ptr);       \
            ptr = NULL;           \
        }                         \
    } while (0)

#else

#define AIRY_TRACK_ALLOC(ptr, type) ((void)0)
#define AIRY_TRACK_FREE(ptr) ((void)0)
#define AIRY_TRACKED_MALLOC(size) AIRY_MALLOC(size)
#define AIRY_TRACKED_FREE(ptr) AIRY_FREE(ptr)

#endif /* AIRY_RESOURCE_TRACKING */
#ifdef _MSC_VER
#define AIRY_ATTRIBUTE(x)
#else
#define AIRY_ATTRIBUTE(x) __attribute__(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_RESOURCE_GUARD_H */
