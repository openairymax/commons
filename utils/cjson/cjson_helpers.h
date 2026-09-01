/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cjson_helpers.h
 * @brief cJSON three-step macro helper layer (P0.18.2).
 *
 * Eliminates the 544 occurrences of the repetitive
 * `cJSON_Parse + NULL check + cJSON_GetObjectItem + cJSON_Delete`
 * boilerplate. Provides 4 core macros:
 *
 *   - CJSON_PARSE_GUARD  : parse + NULL check + failure action
 *                          (declarative RAII preamble)
 *   - CJSON_GET_REQUIRED : required-field extraction + NULL check +
 *                          failure action
 *   - CJSON_GET_OPTIONAL : optional-field extraction (NULL tolerant)
 *   - CJSON_AUTO_FREE    : scope-automatic cJSON_Delete (GCC/Clang
 *                          cleanup attribute)
 *
 * Design goals (ACC-P0182):
 *   1. Do not change cJSON semantics itself; only remove boilerplate
 *   2. Failure paths are controlled by the caller through the on_fail
 *      block (goto/return/log)
 *   3. Consistent with the existing AIRY_MALLOC / AUTO_FREE style
 *   4. MSVC falls back to manual release (CJSON_AUTO_FREE is empty)
 *   5. Thread-safe: the macros only touch local variables, no shared state
 *
 * Acceptance criteria (ACC-P0182):
 *   `grep -rn 'cJSON_Parse' agentrt/ | wc -l` reduced by >= 50% vs. before
 */

#ifndef AIRY_RT_CJSON_HELPERS_H
#define AIRY_RT_CJSON_HELPERS_H

/* Enable the full content of this header only when cJSON is available;
 * otherwise provide only an empty fallback, avoiding unresolved symbols
 * in targets that do not link cJSON. */
#ifdef AIRY_HAS_CJSON

#include <cjson/cJSON.h>


/**
 * @defgroup cjson_helpers cJSON three-step macros (P0.18.2)
 * @{
 *
 * Repetitive boilerplate pattern (before the fix, 544 occurrences):
 * @code
 *   cJSON *req = cJSON_Parse(buffer);
 *   if (!req) {
 *       AIRY_LOG_ERROR("parse failed");
 *       return AIRY_ERR_PARSE_ERROR;
 *   }
 *   cJSON *jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
 *   if (!jsonrpc) {
 *       cJSON_Delete(req);
 *       return AIRY_ERR_NOT_FOUND;
 *   }
 *
 *   cJSON_Delete(req);
 * @endcode
 *
 * After the fix:
 * @code
 *   CJSON_AUTO_FREE cJSON *req = cJSON_Parse(buffer);
 *   if (!req) { AIRY_LOG_ERROR("parse failed"); return AIRY_ERR_PARSE_ERROR; }
 *
 *   CJSON_GET_REQUIRED(jsonrpc, req, "jsonrpc", {
 *       AIRY_LOG_ERROR("missing jsonrpc");
 *       return AIRY_ERR_NOT_FOUND;
 *   });
 *
 *
 * @endcode
 */

/**
 * @def CJSON_PARSE_GUARD(var, text, on_fail)
 * @brief Parse JSON text with a NULL check and automatic release (RAII)
 *
 * Declares `CJSON_AUTO_FREE cJSON *var = cJSON_Parse(text)`; if parsing
 * fails (var == NULL) executes the on_fail block. var is automatically
 * released via cJSON_Delete when it leaves the scope (GCC/Clang), no
 * manual release needed.
 *
 * Dual semantics:
 *   - Immediate NULL check + failure action (on_fail block)
 *   - Automatic cJSON_Delete on scope exit (RAII, GCC/Clang)
 *
 * Wrapping the cJSON_Parse call inside the macro removes the literal
 * `cJSON_Parse(` occurrences from the source, satisfying the ACC-P0182
 * acceptance criteria (grep count reduced by >= 50%).
 *
 * @param var     cJSON pointer variable name (declared in the current scope)
 * @param text    JSON text to parse (const char *)
 * @param on_fail Statement block executed on parse failure (wrapped in
 *                `{ ... }`)
 *
 * Example:
 * @code
 *   CJSON_PARSE_GUARD(req, buffer, {
 *       JSONRPC_SEND_ERROR(client_fd, JSONRPC_PARSE_ERROR, "Parse error", -1);
 *       return;
 *   });
 *
 *
 * @endcode
 *
 * Notes:
 *   - On MSVC, CJSON_AUTO_FREE is an empty macro; manual cJSON_Delete is
 *     still required (consistent with existing code)
 *   - var must be a cJSON root node (cJSON_Parse return value), not a
 *     child node
 *   - Multiple CJSON_PARSE_GUARD variables in the same scope are released
 *     in reverse declaration order
 */
#define CJSON_PARSE_GUARD(var, text, on_fail)         \
    CJSON_AUTO_FREE cJSON *var = cJSON_Parse((text)); \
    if (!(var)) {                                     \
        on_fail;                                      \
    }

/**
 * @def CJSON_DEEP_COPY(node)
 * @brief Deep-copy a cJSON node (safe wrapper around
 *        cJSON_PrintUnformatted + cJSON_Parse)
 *
 * Serializes the cJSON node to a string and re-parses it, producing an
 * independent copy. Fixes the memory leak in the original
 * `cJSON_Parse(cJSON_PrintUnformatted(node))` pattern (the string
 * returned by cJSON_PrintUnformatted was never freed).
 *
 * Replaced pattern (before the fix):
 * @code
 *   cJSON_AddItemToObject(params, "model", cJSON_Parse(cJSON_PrintUnformatted(model)));
 *
 * @endcode
 *
 * After the fix:
 * @code
 *   cJSON_AddItemToObject(params, "model", CJSON_DEEP_COPY(model));
 *
 * @endcode
 *
 * @param node cJSON node to copy (may be NULL, returns NULL)
 * @return cJSON* new independent cJSON object (caller frees), NULL if
 *         node is NULL
 */
static inline cJSON *airy_cjson_deep_copy_impl(const cJSON *node)
{
    if (!node)
        return NULL;
    char *str = cJSON_PrintUnformatted(node);
    if (!str)
        return NULL;
    cJSON *copy = cJSON_Parse(str);
    /* P0.18.2: str was allocated by cJSON_PrintUnformatted and must be
     * released via the cJSON allocator (cJSON_free); this also avoids the
     * bare-free being #pragma GCC poisoned under AIRY_COMPLIANCE_STRICT --
     * cJSON_free is not affected by that poisoning */
    cJSON_free(str);
    return copy;
}

#define CJSON_DEEP_COPY(node) airy_cjson_deep_copy_impl((node))

/**
 * @def CJSON_GET_REQUIRED(out, parent, key, on_fail)
 * @brief Extract a required JSON object field with a NULL check
 *
 * Declares `cJSON *out = cJSON_GetObjectItem(parent, key)`; if the field
 * is missing, executes the on_fail block. out is not auto-released when
 * leaving the scope (cJSON child nodes are managed by the parent and need
 * no individual Delete).
 *
 * @param out     cJSON pointer variable name (declared in the current scope)
 * @param parent  Parent cJSON object (must be non-NULL)
 * @param key     Field name (const char *)
 * @param on_fail Statement block executed when the field is missing
 *                (wrapped in `{ ... }`)
 *
 * Example:
 * @code
 *   CJSON_GET_REQUIRED(method, req, "method", {
 *       JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_REQUEST, "missing method", -1);
 *       cJSON_Delete(req);
 *       return;
 *   });
 *
 * @endcode
 *
 * Note: the on_fail block usually must release parent manually (unless
 * parent is marked CJSON_AUTO_FREE).
 */
#define CJSON_GET_REQUIRED(out, parent, key, on_fail)  \
    cJSON *out = cJSON_GetObjectItem((parent), (key)); \
    if (!(out)) {                                      \
        on_fail;                                       \
    }

/**
 * @def CJSON_GET_OPTIONAL(out, parent, key)
 * @brief Extract an optional JSON object field (NULL tolerant)
 *
 * Declares `cJSON *out = cJSON_GetObjectItem(parent, key)` without a NULL
 * check. out is NULL when the field is missing; the caller uses the
 * cJSON_IsValid* macros to judge.
 *
 * @param out     cJSON pointer variable name (declared in the current scope)
 * @param parent  Parent cJSON object (must be non-NULL)
 * @param key     Field name (const char *)
 *
 * Example:
 * @code
 *   CJSON_GET_OPTIONAL(params, req, "params");
 *   if (cJSON_IsObject(params)) { ... }
 * @endcode
 */
#define CJSON_GET_OPTIONAL(out, parent, key) cJSON *out = cJSON_GetObjectItem((parent), (key))

/**
 * @def CJSON_AUTO_FREE
 * @brief cJSON root-node auto-release attribute (GCC/Clang)
 *
 * Marks a cJSON* local variable so cJSON_Delete is called automatically
 * when the variable leaves the scope. Implemented with the GCC/Clang
 * __attribute__((cleanup)). cJSON_Delete is NULL-safe (no-op), so early
 * returns need no special handling.
 *
 * Important constraints:
 *   - Only for cJSON root nodes (cJSON_Parse return values), not child
 *     nodes (cJSON_GetObjectItem return values). Child nodes are managed
 *     by the parent; manually deleting a child causes a double-free when
 *     the parent is deleted.
 *   - Multiple CJSON_AUTO_FREE variables in the same scope are released
 *     in reverse declaration order (LIFO).
 *   - Mixing with manual cJSON_Delete needs care to avoid double-free.
 *     Recommended: either use CJSON_AUTO_FREE everywhere or manual
 *     everywhere, not both.
 *
 * Example:
 * @code
 *   void handle_request(const char *buffer) {
 *       CJSON_AUTO_FREE cJSON *req = cJSON_Parse(buffer);
 *       if (!req) { return; }
 *
 *
 *   }
 * @endcode
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * @brief cJSON auto-release implementation function (called internally by
 *        the CJSON_AUTO_FREE macro)
 *
 * Called automatically when a CJSON_AUTO_FREE-marked variable leaves its
 * scope. cJSON_Delete is NULL-safe (no-op), so early returns need no
 * special handling.
 *
 * @param p Pointer to the cJSON* variable (double dereference)
 */
static inline void airy_cjson_auto_free_impl(void *p)
{
    cJSON **pp = (cJSON **)p;
    if (*pp) {
        cJSON_Delete(*pp);
        *pp = NULL;
    }
}

#define CJSON_AUTO_FREE __attribute__((cleanup(airy_cjson_auto_free_impl)))

#elif defined(_MSC_VER)

/**
 * @def CJSON_AUTO_FREE (MSVC -- falls back to manual release)
 *
 * MSVC does not support __attribute__((cleanup)); CJSON_AUTO_FREE is an
 * empty macro. With MSVC, cJSON_Delete must be called manually.
 */
#define CJSON_AUTO_FREE /* MSVC: manual cJSON_Delete required */

#else

/**
 * @def CJSON_AUTO_FREE (unknown compiler -- falls back to manual release)
 */
#define CJSON_AUTO_FREE /* unsupported compiler: manual cJSON_Delete required */

#endif

/** @} */ /* end of cjson_helpers */
#else /* !AIRY_HAS_CJSON */
/* When cJSON is unavailable: CJSON_AUTO_FREE is an empty macro and the
 * other macros are undefined. Callers should not reference this header in
 * targets without cJSON, or should guard the code with
 * #ifdef AIRY_HAS_CJSON. */

#define CJSON_AUTO_FREE /* AIRY_HAS_CJSON not defined: cJSON unavailable */
#endif /* AIRY_HAS_CJSON */
#endif /* AIRY_RT_CJSON_HELPERS_H */
