// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#ifndef AIRY_RT_COMPLIANCE_EXEMPT_H
#define AIRY_RT_COMPLIANCE_EXEMPT_H

#ifdef AIRY_COMPLIANCE_STRICT

#define AIRY_COMPLIANCE_EXEMPT_BEGIN
#define AIRY_COMPLIANCE_EXEMPT_END

#else

#define AIRY_COMPLIANCE_EXEMPT_BEGIN                                                                   \
    _Pragma("GCC diagnostic push")                                                                        \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define AIRY_COMPLIANCE_EXEMPT_END \
    _Pragma("GCC diagnostic pop")

#endif

#endif