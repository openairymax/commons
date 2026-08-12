/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file memory_debug.h
 * @brief Unified memory management module: memory debugging features.
 *
 * Provides advanced memory debugging features, including leak detection,
 * boundary checks, and usage analysis. Mainly used during development and
 * testing to find and fix memory-related errors.
 */

#ifndef AIRY_RT_MEMORY_DEBUG_H
#define AIRY_RT_MEMORY_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_debug_api Memory debugging API
 * @{
 */

/**
 * @brief Memory debugging options
 */
typedef struct {
    bool enable_leak_check;
    bool enable_boundary_check;
    bool enable_use_after_free_check;
    bool enable_double_free_check;
    bool enable_invalid_free_check;
    bool track_allocations;
    bool fill_pattern_on_alloc;
    bool fill_pattern_on_free;
    unsigned char alloc_fill_pattern;
    unsigned char free_fill_pattern;
    size_t redzone_size;
    const char *log_file;
    int verbosity_level;
} memory_debug_options_t;

/**
 * @brief Memory leak report
 */
typedef struct {
    size_t leak_count;
    size_t total_leaked_bytes;
    struct {
        void *address;
        size_t size;
        const char *tag;
        const char *file;
        int line;
        const char *function;
        uint64_t timestamp;
    } leaks[100];
} memory_leak_report_t;

/**
 * @brief Memory error types
 */
typedef enum {
    MEMORY_ERROR_NONE = 0,
    MEMORY_ERROR_OUT_OF_BOUNDS,
    MEMORY_ERROR_USE_AFTER_FREE,
    MEMORY_ERROR_DOUBLE_FREE,
    MEMORY_ERROR_INVALID_FREE,
    MEMORY_ERROR_CORRUPTION,
    MEMORY_ERROR_LEAK,
    MEMORY_ERROR_ALLOC_FAILED
} memory_error_type_t;

/**
 * @brief Memory error report
 */
typedef struct {
    memory_error_type_t type;
    void *address;
    size_t size;
    const char *description;
    const char *file;
    int line;
    const char *function;
    uint64_t timestamp;
} memory_error_report_t;

/**
 * @brief Memory debugging callback function type
 */
typedef void (*memory_debug_callback_t)(const memory_error_report_t *report, void *user_data);

/**
 * @brief Initialize memory debugging
 *
 * @param[in] options Debug options (may be NULL, uses defaults)
 * @return true on success, false on failure
 *
 * @note Memory debugging must be enabled first (memory_debug_enable) to
 *       use this feature
 */
bool memory_debug_init(const memory_debug_options_t *options);

/**
 * @brief Enable memory debugging
 *
 * @param[in] enable Whether to enable
 * @return true on success, false on failure
 *
 * @note Enabling debugging increases memory and performance overhead
 */
bool memory_debug_enable(bool enable);

/**
 * @brief Check whether memory debugging is enabled
 *
 * @return true if enabled, false if disabled
 */
bool memory_debug_is_enabled(void);

/**
 * @brief Set the memory debugging callback
 *
 * @param[in] callback Callback function
 * @param[in] user_data User data
 */
void memory_debug_set_callback(memory_debug_callback_t callback, void *user_data);

/**
 * @brief Check for memory leaks
 *
 * @param[out] report Leak report output buffer (may be NULL)
 * @param[in] dump_to_log Whether to log the leak information
 * @return Number of leaked bytes, 0 for no leaks
 */
size_t memory_debug_check_leaks(memory_leak_report_t *report, bool dump_to_log);

/**
 * @brief Validate a memory block's integrity
 *
 * @param[in] ptr Memory pointer
 * @param[out] error Error report output buffer (may be NULL)
 * @return true if intact, false if corrupted
 */
bool memory_debug_validate(void *ptr, memory_error_report_t *error);

/**
 * @brief Validate all allocated memory blocks
 *
 * @param[out] error_count Error count output
 * @param[in] dump_to_log Whether to log the error information
 * @return Number of errors found
 */
size_t memory_debug_validate_all(size_t *error_count, bool dump_to_log);

/**
 * @brief Dump memory debugging information
 *
 * @param[in] file Output file name (NULL uses the log_file set at init)
 * @param[in] include_stack_trace Whether to include stack traces
 */
void memory_debug_dump_info(const char *file, bool include_stack_trace);

/**
 * @brief Get a memory block's allocation information
 *
 * @param[in] ptr Memory pointer
 * @param[out] file Allocation file output (may be NULL)
 * @param[out] line Allocation line output (may be NULL)
 * @param[out] function Allocation function output (may be NULL)
 * @param[out] tag Allocation tag output (may be NULL)
 * @return true if found, false on failure
 */
bool memory_debug_get_allocation_info(void *ptr, const char **file, int *line,
                                      const char **function, const char **tag);

/**
 * @brief Set a memory block's tag
 *
 * @param[in] ptr Memory pointer
 * @param[in] tag New tag (may be NULL)
 * @return true on success, false on failure
 */
bool memory_debug_set_tag(void *ptr, const char *tag);

/**
 * @brief Enable or disable a specific debugging feature
 *
 * @param[in] feature Feature name ("leak_check", "boundary_check", etc.)
 * @param[in] enable Whether to enable
 * @return true on success, false on failure
 */
bool memory_debug_set_feature(const char *feature, bool enable);

/**
 * @brief Get memory debugging statistics
 *
 * @param[out] total_allocations Total allocation count output
 * @param[out] total_frees Total free count output
 * @param[out] current_allocations Current allocation count output
 * @param[out] error_count Error count output
 * @return true on success, false on failure
 */
bool memory_debug_get_stats(size_t *total_allocations, size_t *total_frees,
                            size_t *current_allocations, size_t *error_count);

/**
 * @brief Reset memory debugging statistics
 */
void memory_debug_reset_stats(void);

/**
 * @brief Enable stack tracing
 *
 * @param[in] enable Whether to enable
 * @param[in] max_depth Maximum stack depth (0 for the default)
 * @return true on success, false on failure
 *
 * @note Enabling stack tracing significantly increases memory and
 *       performance overhead
 */
bool memory_debug_enable_stack_trace(bool enable, size_t max_depth);

/**
 * @brief Get a stack trace
 *
 * @param[in] ptr Memory pointer
 * @param[out] frames Stack frame output buffer
 * @param[in] max_frames Maximum number of frames
 * @return Number of frames actually captured
 */
size_t memory_debug_get_stack_trace(void *ptr, void **frames, size_t max_frames);

/**
 * @brief Set the memory debugging log level
 *
 * @param[in] level Log level (0-3, 0=none, 1=error, 2=warning, 3=verbose)
 */
void memory_debug_set_log_level(int level);

/**
 * @brief Log a memory operation
 *
 * @param[in] operation Operation type ("alloc", "free", "realloc", etc.)
 * @param[in] ptr Memory pointer
 * @param[in] size Size
 * @param[in] file File name
 * @param[in] line Line number
 * @param[in] function Function name
 *
 * @note Mainly for internal use; can also be used to manually record
 *       custom memory operations
 */
void memory_debug_log_operation(const char *operation, void *ptr, size_t size, const char *file,
                                int line, const char *function);

/**
 * @brief Memory debugging checkpoint
 *
 * Creates a memory state checkpoint for comparing memory usage changes.
 *
 * @param[in] name Checkpoint name
 * @return Checkpoint ID, 0 on failure
 */
unsigned int memory_debug_checkpoint(const char *name);

/**
 * @brief Compare checkpoints
 *
 * @param[in] checkpoint1 First checkpoint ID
 * @param[in] checkpoint2 Second checkpoint ID
 * @param[out] diff_report Diff report output buffer (may be NULL)
 * @return Number of differences, 0 for none
 */
size_t memory_debug_compare_checkpoints(unsigned int checkpoint1, unsigned int checkpoint2,
                                        memory_leak_report_t *diff_report);

/**
 * @brief Memory debugging allocation macro
 */
#ifdef MEMORY_DEBUG_ENABLED
#define MEMORY_DEBUG_ALLOC(size, tag)                                                \
    memory_debug_log_operation("alloc", NULL, (size), __FILE__, __LINE__, __func__); \
    memory_alloc((size), (tag))

#define MEMORY_DEBUG_CALLOC(size, tag)                                                \
    memory_debug_log_operation("calloc", NULL, (size), __FILE__, __LINE__, __func__); \
    memory_calloc((size), (tag))

#define MEMORY_DEBUG_FREE(ptr)                                                  \
    memory_debug_log_operation("free", (ptr), 0, __FILE__, __LINE__, __func__); \
    memory_free((ptr))
#else
#define MEMORY_DEBUG_ALLOC(size, tag) memory_alloc((size), (tag))
#define MEMORY_DEBUG_CALLOC(size, tag) memory_calloc((size), (tag))
#define MEMORY_DEBUG_FREE(ptr) memory_free((ptr))
#endif

/** @} */ /* end of memory_debug_api */
#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_MEMORY_DEBUG_H */
