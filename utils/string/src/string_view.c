// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_view.c
 * @brief View and list container domain: string view compare/search and
 * string list management.
 */

#include "airy_string.h"
#include "string_internal.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "airy_memory.h"
#include "string_compat.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "error.h"

#ifdef _WIN32
#include <locale.h>
#include <windows.h>
#else
#include <locale.h>
#include <strings.h>
#endif

string_view_t string_view_create(const char *str, string_encoding_t encoding)
{
    string_view_t view = {.data = str,
                          .length = (str != NULL) ? strlen(str) : 0,
                          .encoding = encoding};

    return view;
}

string_view_t string_view_create_n(const char *str, size_t len, string_encoding_t encoding)
{
    string_view_t view = {.data = str, .length = len, .encoding = encoding};

    return view;
}

int string_view_compare(const string_view_t *view1, const string_view_t *view2, int options)
{
    if (view1 == view2) {
        return 0;
    }

    if (view1 == NULL) {
        return AIRY_EINVAL;
    }

    if (view2 == NULL) {
        return 1;
    }

    size_t min_len = (view1->length < view2->length) ? view1->length : view2->length;

    int result = 0;
    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
        for (size_t i = 0; i < min_len; i++) {
            char ch1 = (char)tolower((unsigned char)view1->data[i]);
            char ch2 = (char)tolower((unsigned char)view2->data[i]);

            if (ch1 != ch2) {
                result = (ch1 < ch2) ? -1 : 1;
                break;
            }
        }
    } else {
        result = memcmp(view1->data, view2->data, min_len);
    }

    if (result == 0 && view1->length != view2->length) {
        result = (view1->length < view2->length) ? -1 : 1;
    }

    return result;
}

ssize_t string_view_find(const string_view_t *haystack, const string_view_t *needle, int options)
{
    if (haystack == NULL || needle == NULL || needle->length == 0 ||
        needle->length > haystack->length) {
        return AIRY_EINVAL;
    }

    for (size_t i = 0; i <= haystack->length - needle->length; i++) {
        string_view_t subview = {.data = haystack->data + i,
                                 .length = needle->length,
                                 .encoding = haystack->encoding};

        if (string_view_compare(&subview, needle, options) == 0) {
            return (ssize_t)i;
        }
    }

    return AIRY_EINVAL;
}

char *string_view_to_cstr(const string_view_t *view)
{
    if (view == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    char *str = (char *)AIRY_MALLOC(view->length + 1);
    if (str == NULL) {
        AIRY_ERROR_NULL(AIRY_ERR_UNKNOWN, "operation failed");
    }

    __builtin_memcpy(str, view->data, view->length);
    str[view->length] = '\0';

    return str;
}

string_list_t string_list_create(size_t initial_capacity)
{
    string_list_t list = {.items = NULL, .count = 0, .capacity = 0};

    if (initial_capacity > 0) {
        if (initial_capacity > SIZE_MAX / sizeof(string_view_t)) {
            return list;
        }
        list.items = (string_view_t *)AIRY_MALLOC(initial_capacity * sizeof(string_view_t));
        if (list.items != NULL) {
            list.capacity = initial_capacity;
        }
    }

    return list;
}

void string_list_destroy(string_list_t *list)
{
    if (list == NULL) {
        return;
    }

    if (list->items != NULL) {
        AIRY_FREE(list->items);
        list->items = NULL;
    }

    list->count = 0;
    list->capacity = 0;
}

bool string_list_add(string_list_t *list, const string_view_t *item)
{
    if (list == NULL || item == NULL) {
        return false;
    }

    if (list->count >= list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        string_view_t *new_items =
            (string_view_t *)AIRY_REALLOC(list->items, new_capacity * sizeof(string_view_t));
        if (new_items == NULL) {
            return false;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count] = *item;
    list->count++;

    return true;
}

bool string_list_add_cstr(string_list_t *list, const char *str)
{
    if (list == NULL || str == NULL) {
        return false;
    }

    string_view_t view = string_view_create(str, STRING_ENCODING_UTF8);
    return string_list_add(list, &view);
}

void string_list_clear(string_list_t *list)
{
    if (list == NULL) {
        return;
    }

    list->count = 0;
}

size_t string_list_size(const string_list_t *list)
{
    if (list == NULL) {
        return 0;
    }

    return list->count;
}

string_view_t string_list_get(const string_list_t *list, size_t index)
{
    static const string_view_t EMPTY_VIEW = {NULL, 0, STRING_ENCODING_ASCII};

    if (list == NULL || index >= list->count) {
        return EMPTY_VIEW;
    }

    return list->items[index];
}
