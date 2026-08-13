// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file string_encode.c
 * @brief Encode/decode domain: JSON escaping, encoding conversion and
 * UTF-8 utilities.
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

int string_common_json_escape(const char *src, char **out)
{
    if (!src || !out)
        return AIRY_EINVAL;

    size_t len = 0;
    const char *p = src;
    while (*p) {
        unsigned char ch = (unsigned char)*p;
        switch (ch) {
        case '"':
        case '\\':
        case '/':
            len += 2;
            break;
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            len += 2;
            break;
        default:
            if (ch < 0x20) {
                len += 6;
            } else {
                len += 1;
            }
            break;
        }
        p++;
    }

    char *escaped = (char *)AIRY_MALLOC(len + 1);
    if (!escaped)
        return AIRY_EINVAL;

    char *q = escaped;
    p = src;
    while (*p) {
        unsigned char ch = (unsigned char)*p;
        switch (ch) {
        case '"':
            *q++ = '\\';
            *q++ = '"';
            break;
        case '\\':
            *q++ = '\\';
            *q++ = '\\';
            break;
        case '/':
            *q++ = '\\';
            *q++ = '/';
            break;
        case '\b':
            *q++ = '\\';
            *q++ = 'b';
            break;
        case '\f':
            *q++ = '\\';
            *q++ = 'f';
            break;
        case '\n':
            *q++ = '\\';
            *q++ = 'n';
            break;
        case '\r':
            *q++ = '\\';
            *q++ = 'r';
            break;
        case '\t':
            *q++ = '\\';
            *q++ = 't';
            break;
        default:
            if (ch < 0x20) {
                q += snprintf(q, 7, "\\u%04x", ch);
            } else {
                *q++ = (char)ch;
            }
            break;
        }
        p++;
    }
    *q = '\0';

    *out = escaped;
    return 0;
}

size_t string_common_json_escape_buf(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0)
        return 0;

    char *q = dst;
    const char *end = dst + dst_size - 1;
    const char *p = src;

    while (*p && q < end) {
        unsigned char ch = (unsigned char)*p;
        switch (ch) {
        case '"':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = '"';
            break;
        case '\\':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = '\\';
            break;
        case '/':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = '/';
            break;
        case '\b':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = 'b';
            break;
        case '\f':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = 'f';
            break;
        case '\n':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = 'n';
            break;
        case '\r':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = 'r';
            break;
        case '\t':
            if (q + 2 > end)
                goto done;
            *q++ = '\\';
            *q++ = 't';
            break;
        default:
            if (ch < 0x20) {
                if (q + 6 > end)
                    goto done;
                q += snprintf(q, 7, "\\u%04x", ch);
            } else {
                *q++ = (char)ch;
            }
            break;
        }
        p++;
    }

done:
    *q = '\0';
    return (size_t)(q - dst);
}

static size_t utf8_encode_codepoint(uint32_t cp, char *out, size_t out_size)
{
    if (cp <= 0x7F) {
        if (out_size < 1)
            return 0;
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FF) {
        if (out_size < 2)
            return 0;
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        if (out_size < 3)
            return 0;
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        if (out_size < 4)
            return 0;
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

static size_t utf8_decode_codepoint(const char *src, size_t src_len, uint32_t *cp)
{
    if (src_len == 0)
        return 0;
    unsigned char b0 = (unsigned char)src[0];

    if (b0 <= 0x7F) {
        *cp = b0;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0) {
        if (src_len < 2)
            return 0;
        unsigned char b1 = (unsigned char)src[1];
        if ((b1 & 0xC0) != 0x80)
            return 0;
        *cp = ((uint32_t)(b0 & 0x1F) << 6) | (b1 & 0x3F);
        if (*cp < 0x80)
            return 0;
        return 2;
    }
    if ((b0 & 0xF0) == 0xE0) {
        if (src_len < 3)
            return 0;
        unsigned char b1 = (unsigned char)src[1];
        unsigned char b2 = (unsigned char)src[2];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
            return 0;
        *cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(b1 & 0x3F) << 6) | (b2 & 0x3F);
        if (*cp < 0x800)
            return 0;
        if (*cp >= 0xD800 && *cp <= 0xDFFF)
            return 0;
        return 3;
    }
    if ((b0 & 0xF8) == 0xF0) {
        if (src_len < 4)
            return 0;
        unsigned char b1 = (unsigned char)src[1];
        unsigned char b2 = (unsigned char)src[2];
        unsigned char b3 = (unsigned char)src[3];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
            return 0;
        *cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(b1 & 0x3F) << 12) |
              ((uint32_t)(b2 & 0x3F) << 6) | (b3 & 0x3F);
        if (*cp < 0x10000 || *cp > 0x10FFFF)
            return 0;
        return 4;
    }
    return 0;
}

static uint32_t win1252_special_map[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, /* 0x80-0x87 */
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, /* 0x88-0x8F */
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, /* 0x90-0x97 */
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178 /* 0x98-0x9F */
};

static int to_utf8_intermediate(const char *src, string_encoding_t src_enc, char *buf,
                                size_t buf_size)
{
    size_t di = 0;
    size_t src_len = strlen(src);

    if (src_enc == STRING_ENCODING_ASCII || src_enc == STRING_ENCODING_UTF8) {
        if (src_len >= buf_size)
            return AIRY_ERR_BUFFER_TOO_SMALL;
        __builtin_memcpy(buf, src, src_len);
        buf[src_len] = '\0';
        return (int)src_len;
    }

    if (src_enc == STRING_ENCODING_LATIN1) {
        for (size_t i = 0; i < src_len; i++) {
            unsigned char ch = (unsigned char)src[i];
            if (ch <= 0x7F) {
                if (di + 1 >= buf_size)
                    return AIRY_ERR_BUFFER_TOO_SMALL;
                buf[di++] = (char)ch;
            } else {
                if (di + 2 >= buf_size)
                    return AIRY_ERR_BUFFER_TOO_SMALL;
                buf[di++] = (char)(0xC0 | (ch >> 6));
                buf[di++] = (char)(0x80 | (ch & 0x3F));
            }
        }
        buf[di] = '\0';
        return (int)di;
    }

    if (src_enc == STRING_ENCODING_WINDOWS_1252) {
        for (size_t i = 0; i < src_len; i++) {
            unsigned char ch = (unsigned char)src[i];
            uint32_t cp = ch;
            if (ch >= 0x80 && ch <= 0x9F)
                cp = win1252_special_map[ch - 0x80];
            size_t n = utf8_encode_codepoint(cp, buf + di, buf_size - di - 1);
            if (n == 0)
                return AIRY_ERR_BUFFER_TOO_SMALL;
            di += n;
        }
        buf[di] = '\0';
        return (int)di;
    }

    if (src_enc == STRING_ENCODING_UTF16_LE || src_enc == STRING_ENCODING_UTF16_BE) {
        for (size_t i = 0; i + 1 < src_len; i += 2) {
            uint16_t unit;
            if (src_enc == STRING_ENCODING_UTF16_LE)
                unit = (uint16_t)((unsigned char)src[i] | ((unsigned char)src[i + 1] << 8));
            else
                unit = (uint16_t)(((unsigned char)src[i] << 8) | (unsigned char)src[i + 1]);

            uint32_t cp;
            if (unit >= 0xD800 && unit <= 0xDBFF) {

                if (i + 3 >= src_len)
                    return AIRY_ERR_PARSE_ERROR;
                uint16_t low;
                if (src_enc == STRING_ENCODING_UTF16_LE)
                    low = (uint16_t)((unsigned char)src[i + 2] | ((unsigned char)src[i + 3] << 8));
                else
                    low = (uint16_t)(((unsigned char)src[i + 2] << 8) | (unsigned char)src[i + 3]);
                if (low < 0xDC00 || low > 0xDFFF)
                    return AIRY_ERR_PARSE_ERROR;
                cp = 0x10000 + ((uint32_t)(unit - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                return AIRY_ERR_PARSE_ERROR;
            } else {
                cp = unit;
            }
            size_t n = utf8_encode_codepoint(cp, buf + di, buf_size - di - 1);
            if (n == 0)
                return AIRY_ERR_BUFFER_TOO_SMALL;
            di += n;
        }
        buf[di] = '\0';
        return (int)di;
    }

    if (src_enc == STRING_ENCODING_UTF32_LE || src_enc == STRING_ENCODING_UTF32_BE) {
        for (size_t i = 0; i + 3 < src_len; i += 4) {
            uint32_t cp;
            if (src_enc == STRING_ENCODING_UTF32_LE)
                cp = (uint32_t)((unsigned char)src[i]) |
                     ((uint32_t)(unsigned char)src[i + 1] << 8) |
                     ((uint32_t)(unsigned char)src[i + 2] << 16) |
                     ((uint32_t)(unsigned char)src[i + 3] << 24);
            else
                cp = ((uint32_t)(unsigned char)src[i] << 24) |
                     ((uint32_t)(unsigned char)src[i + 1] << 16) |
                     ((uint32_t)(unsigned char)src[i + 2] << 8) |
                     (uint32_t)(unsigned char)src[i + 3];
            if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                return AIRY_ERR_PARSE_ERROR;
            size_t n = utf8_encode_codepoint(cp, buf + di, buf_size - di - 1);
            if (n == 0)
                return AIRY_ERR_BUFFER_TOO_SMALL;
            di += n;
        }
        buf[di] = '\0';
        return (int)di;
    }

    return AIRY_ERR_NOT_SUPPORTED;
}

static int from_utf8_intermediate(const char *utf8, size_t utf8_len, string_encoding_t dest_enc,
                                  char *dest, size_t dest_size)
{
    size_t si = 0, di = 0;

    if (dest_enc == STRING_ENCODING_ASCII) {
        for (size_t i = 0; i < utf8_len; i++) {
            if (di + 1 >= dest_size)
                return AIRY_ERR_BUFFER_TOO_SMALL;
            unsigned char ch = (unsigned char)utf8[i];
            dest[di++] = (ch <= 0x7F) ? (char)ch : '?';
        }
        dest[di] = '\0';
        return (int)di;
    }

    if (dest_enc == STRING_ENCODING_UTF8) {
        if (utf8_len >= dest_size)
            return AIRY_ERR_BUFFER_TOO_SMALL;
        __builtin_memcpy(dest, utf8, utf8_len);
        dest[utf8_len] = '\0';
        return (int)utf8_len;
    }

    if (dest_enc == STRING_ENCODING_LATIN1) {
        while (si < utf8_len) {
            if (di + 1 >= dest_size)
                return AIRY_ERR_BUFFER_TOO_SMALL;
            uint32_t cp;
            size_t n = utf8_decode_codepoint(utf8 + si, utf8_len - si, &cp);
            if (n == 0)
                return AIRY_ERR_PARSE_ERROR;
            si += n;
            dest[di++] = (cp <= 0xFF) ? (char)cp : '?';
        }
        dest[di] = '\0';
        return (int)di;
    }

    if (dest_enc == STRING_ENCODING_WINDOWS_1252) {
        while (si < utf8_len) {
            if (di + 1 >= dest_size)
                return AIRY_ERR_BUFFER_TOO_SMALL;
            uint32_t cp;
            size_t n = utf8_decode_codepoint(utf8 + si, utf8_len - si, &cp);
            if (n == 0)
                return AIRY_ERR_PARSE_ERROR;
            si += n;
            if (cp <= 0x7F || (cp >= 0xA0 && cp <= 0xFF)) {
                dest[di++] = (char)cp;
            } else {

                int found = 0;
                for (int j = 0; j < 32; j++) {
                    if (win1252_special_map[j] == cp) {
                        dest[di++] = (char)(0x80 + j);
                        found = 1;
                        break;
                    }
                }
                if (!found)
                    dest[di++] = '?';
            }
        }
        dest[di] = '\0';
        return (int)di;
    }

    if (dest_enc == STRING_ENCODING_UTF16_LE || dest_enc == STRING_ENCODING_UTF16_BE) {
        while (si < utf8_len) {
            uint32_t cp;
            size_t n = utf8_decode_codepoint(utf8 + si, utf8_len - si, &cp);
            if (n == 0)
                return AIRY_ERR_PARSE_ERROR;
            si += n;

            if (cp <= 0xFFFF) {
                if (di + 2 >= dest_size)
                    return AIRY_ERR_BUFFER_TOO_SMALL;
                if (dest_enc == STRING_ENCODING_UTF16_LE) {
                    dest[di++] = (char)(cp & 0xFF);
                    dest[di++] = (char)((cp >> 8) & 0xFF);
                } else {
                    dest[di++] = (char)((cp >> 8) & 0xFF);
                    dest[di++] = (char)(cp & 0xFF);
                }
            } else {

                if (di + 4 >= dest_size)
                    return AIRY_ERR_BUFFER_TOO_SMALL;
                uint32_t adj = cp - 0x10000;
                uint16_t high = (uint16_t)(0xD800 + (adj >> 10));
                uint16_t low = (uint16_t)(0xDC00 + (adj & 0x3FF));
                if (dest_enc == STRING_ENCODING_UTF16_LE) {
                    dest[di++] = (char)(high & 0xFF);
                    dest[di++] = (char)((high >> 8) & 0xFF);
                    dest[di++] = (char)(low & 0xFF);
                    dest[di++] = (char)((low >> 8) & 0xFF);
                } else {
                    dest[di++] = (char)((high >> 8) & 0xFF);
                    dest[di++] = (char)(high & 0xFF);
                    dest[di++] = (char)((low >> 8) & 0xFF);
                    dest[di++] = (char)(low & 0xFF);
                }
            }
        }
        dest[di] = '\0';
        return (int)di;
    }

    if (dest_enc == STRING_ENCODING_UTF32_LE || dest_enc == STRING_ENCODING_UTF32_BE) {
        while (si < utf8_len) {
            if (di + 4 >= dest_size)
                return AIRY_ERR_BUFFER_TOO_SMALL;
            uint32_t cp;
            size_t n = utf8_decode_codepoint(utf8 + si, utf8_len - si, &cp);
            if (n == 0)
                return AIRY_ERR_PARSE_ERROR;
            si += n;
            if (dest_enc == STRING_ENCODING_UTF32_LE) {
                dest[di++] = (char)(cp & 0xFF);
                dest[di++] = (char)((cp >> 8) & 0xFF);
                dest[di++] = (char)((cp >> 16) & 0xFF);
                dest[di++] = (char)((cp >> 24) & 0xFF);
            } else {
                dest[di++] = (char)((cp >> 24) & 0xFF);
                dest[di++] = (char)((cp >> 16) & 0xFF);
                dest[di++] = (char)((cp >> 8) & 0xFF);
                dest[di++] = (char)(cp & 0xFF);
            }
        }
        dest[di] = '\0';
        return (int)di;
    }

    return AIRY_ERR_NOT_SUPPORTED;
}

int string_convert_encoding(const char *src, string_encoding_t src_encoding, char *dest,
                            size_t dest_size, string_encoding_t dest_encoding)
{
    if (src == NULL || dest == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "invalid argument");
        return AIRY_EINVAL;
    }

    if (src_encoding == dest_encoding) {
        return string_copy(dest, src, dest_size);
    }

    size_t src_len = strlen(src);
    size_t mid_size = src_len * 4 + 1;
    char stack_buf[4096];
    char *mid_buf = stack_buf;
    bool heap_allocated = false;

    if (mid_size > sizeof(stack_buf)) {
        mid_buf = (char *)AIRY_MALLOC(mid_size);
        if (!mid_buf) {
            string_set_error(STRING_ERROR_ENCODING_CONVERSION,
                             "out of memory for intermediate buffer");
            return AIRY_ENOMEM;
        }
        heap_allocated = true;
    }

    int mid_len = to_utf8_intermediate(src, src_encoding, mid_buf, mid_size);
    if (mid_len < 0) {
        if (heap_allocated)
            AIRY_FREE(mid_buf);
        string_set_error(STRING_ERROR_ENCODING_CONVERSION, "source encoding decode failed");
        return AIRY_EINVAL;
    }

    int dest_len = from_utf8_intermediate(mid_buf, (size_t)mid_len, dest_encoding, dest, dest_size);
    if (heap_allocated)
        AIRY_FREE(mid_buf);

    if (dest_len < 0) {
        string_set_error(STRING_ERROR_ENCODING_CONVERSION,
                         "target encoding encode failed or buffer too small");
        return AIRY_EINVAL;
    }

    return AIRY_OK;
}

size_t string_utf8_char_count(const char *str, size_t max_len)
{
    if (str == NULL) {
        return 0;
    }

    size_t count = 0;
    size_t i = 0;

    while (i < max_len && str[i] != '\0') {
        unsigned char ch = (unsigned char)str[i];

        if ((ch & 0xC0) != 0x80) {
            count++;
        }

        i++;
    }

    return count;
}

size_t string_utf8_next_char(const char *str, uint32_t *ch)
{
    if (str == NULL || ch == NULL) {
        return 0;
    }

    unsigned char first = (unsigned char)str[0];

    if (first == 0) {
        return 0;
    }

    if (first < 0x80) {
        *ch = first;
        return 1;
    }

    if ((first & 0xE0) == 0xC0) {
        if (str[1] == 0) {
            return 0;
        }

        unsigned char second = (unsigned char)str[1];
        if ((second & 0xC0) != 0x80) {
            return 0;
        }

        *ch = ((first & 0x1F) << 6) | (second & 0x3F);
        return 2;
    }

    if ((first & 0xF0) == 0xE0) {
        if (str[1] == 0 || str[2] == 0) {
            return 0;
        }

        unsigned char second = (unsigned char)str[1];
        unsigned char third = (unsigned char)str[2];

        if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80) {
            return 0;
        }

        *ch = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
        return 3;
    }

    if ((first & 0xF8) == 0xF0) {
        if (str[1] == 0 || str[2] == 0 || str[3] == 0) {
            return 0;
        }

        unsigned char second = (unsigned char)str[1];
        unsigned char third = (unsigned char)str[2];
        unsigned char fourth = (unsigned char)str[3];

        if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80 || (fourth & 0xC0) != 0x80) {
            return 0;
        }

        *ch = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) |
              (fourth & 0x3F);
        return 4;
    }

    return 0;
}

bool string_utf8_validate(const char *str, size_t len)
{
    if (str == NULL) {
        return false;
    }

    size_t i = 0;
    while (i < len && str[i] != '\0') {
        unsigned char first = (unsigned char)str[i];

        if (first < 0x80) {
            i++;
            continue;
        }

        size_t char_len = 0;

        if ((first & 0xE0) == 0xC0) {
            char_len = 2;
        } else if ((first & 0xF0) == 0xE0) {
            char_len = 3;
        } else if ((first & 0xF8) == 0xF0) {
            char_len = 4;
        } else {
            return false;
        }

        if (i + char_len > len) {
            return false;
        }

        for (size_t j = 1; j < char_len; j++) {
            unsigned char next = (unsigned char)str[i + j];
            if ((next & 0xC0) != 0x80) {
                return false;
            }
        }

        if (char_len == 2 && first == 0xC0 && (unsigned char)str[i + 1] == 0x80) {
            return false;
        }

        uint32_t code_point = 0;
        if (char_len == 2) {
            code_point = ((first & 0x1F) << 6) | (str[i + 1] & 0x3F);
        } else if (char_len == 3) {
            code_point = ((first & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
        } else if (char_len == 4) {
            code_point = ((first & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) |
                         ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
        }

        if (code_point > 0x10FFFF) {
            return false;
        }

        if (code_point >= 0xD800 && code_point <= 0xDFFF) {
            return false;
        }

        i += char_len;
    }

    return true;
} // TESTMARKER
