**Language:** English | [简体中文](README_zh.md)

# Airymax Commons — Unified Foundation Library

`agentrt/commons/`

**Version:** 0.1.1
**License:** AGPL-3.0-or-later OR Apache-2.0 (dual-licensed)
**Branch:** `feature/official-hubs-01`

---

## 1. Module Positioning

Commons is the **foundational layer** of the Airymax agent runtime. It provides
the cross-platform, cross-language, cross-module infrastructure that every
upper layer — atomic primitives, daemons, the security dome, the gateway,
protocols — is built upon. Commons itself depends on **no** other Airymax
module: it is the lowest layer in the dependency graph.

As the project's authoritative source for type definitions
(`agentrt_types.h`), Commons guarantees type consistency across modules and
eliminates cross-module type conflicts. Its design goals are:

- **Zero-dependency abstraction** — platform-agnostic type system and interface
  definitions keep the kernel decoupled from peripheral code.
- **Unified error contract** — a project-wide error code system
  (`agentrt_error_t`) and propagation mechanism.
- **High-performance infrastructure** — memory pools, lock-free queues,
  zero-copy data pipelines and other low-level primitives.
- **Built-in observability** — standardized capture interfaces for logs,
  metrics and traces.
- **Safe by default** — every I/O path goes through parameter validation,
  boundary checks and resource limits.

---

## 2. Directory Structure

```
commons/
├── CMakeLists.txt               # CMake build configuration
├── README.md                    # This file (English)
├── README_zh.md                 # Chinese version
├── LICENSE                      # Dual license texts (AGPL-3.0 + Apache-2.0)
├── NOTICE                       # Copyright notice
├── platform/                    # Platform abstraction layer
│   ├── include/
│   │   ├── platform.h           # Platform detection and base definitions
│   │   └── export.h             # Symbol export control
│   ├── compat/                  # Platform compatibility headers (stdbool.h, stdint.h)
│   └── platform.c               # Platform abstraction implementation
├── include/                     # Global public headers
│   └── agentrt_types.h          # Unified type and error-code definitions
├── utils/                       # Tooling module collection (24+ modules)
│   ├── include/                 # Cross-module shared headers
│   │   ├── atomic_compat.h      # Cross-platform atomic operation compat layer
│   │   └── check.h              # Generic check macros
│   ├── logging/                 # Logging (3-tier: Core → Atomic → Service)
│   ├── sync/                    # Sync primitives (8+ locks & queues)
│   ├── memory/                  # Memory management (pools, smart ptrs, zero-copy)
│   ├── string/                  # String operations and formatting
│   ├── ipc/                     # IPC abstraction layer
│   ├── token/                   # Token management (API Key / JWT lifecycle)
│   ├── cost/                    # Cost estimation and control
│   ├── observability/           # Observability (OpenTelemetry metrics/traces)
│   ├── platform/                # Platform-specific tooling functions
│   ├── error/                   # Error handling framework
│   ├── types/                   # Generic type definitions and conversions
│   ├── config_unified/          # Unified config (3-tier: Core → Source → Service)
│   ├── execution/               # Command execution engine (validation, cross-platform)
│   ├── io/                      # File I/O utilities
│   ├── cache/                   # Cache management (LRU / TTL)
│   ├── compat/                  # Cross-version / cross-platform compatibility
│   ├── cognition/               # Cognition mgmt (agent info, scheduling, planning)
│   ├── strategy/                # Weighted scoring strategy engine
│   ├── network/                 # Network utilities (HTTP / URI / DNS)
│   ├── security/                # Input validation and security filtering
│   ├── resource/                # Resource protection and quotas
│   ├── uuid/                    # UUID generation and parsing
│   ├── print/                   # Printing / formatting helpers
│   ├── compliance/              # Compliance validation and policy enforcement
│   └── quality/                 # Code quality checks
└── tests/                       # Test suite
    ├── utils/                   # Test utility framework
    ├── unit/                    # Unit tests
    └── integration/             # Integration tests
```

### Core Components

#### Type system (`agentrt_types.h`)

The single authoritative source of type definitions for the entire project:

| Type | Description |
|------|-------------|
| `agentrt_error_t` | Unified error code type (`int32_t`; negative = error, 0 = success) |
| `agentrt_ipc_header_t` | Application-level IPC message header (magic/version/type/flags/msg_id) |
| `agentrt_ipc_message_t` | Application-level IPC message struct (header + payload) |
| `agentrt_task_id_t` | Task ID type (`uint64_t`) |
| `agentrt_message_id_t` | Message ID type (`uint64_t`) |

The unified error code system (`AGENTRT_E*`) covers 29 standard errors
including invalid argument, out of memory, permission denied, timeout,
I/O error, protocol error, quota exceeded, etc.

#### Platform abstraction layer (`platform/`)

- **Platform detection** — auto-detects Linux / Windows / macOS.
- **Filesystem** — path normalization and file-operation abstraction.
- **Threads & sync** — `agentrt_thread_t`, `agentrt_mutex_t`, `agentrt_cond_t`.
- **Dynamic library loading** — cross-platform FFI support.
- **System info** — CPU core count, memory size, process ID.

#### Atomic operation compat layer (`atomic_compat.h`)

| Backend | Environment | Implementation |
|---------|-------------|----------------|
| C11 stdatomic | Linux / macOS (C11+ compiler) | `<stdatomic.h>` |
| Windows Interlocked | Windows (MSVC / MinGW) | `<intrin.h>` Interlocked API |
| POSIX fallback | Older GCC/Clang | `__atomic` builtins |

Covers 11 types including `atomic_bool`, `atomic_int`, `atomic_uint`,
`atomic_int64_t`, `atomic_uint64_t`, `atomic_size_t`.

#### Unified memory management (`memory_compat.h`)

| Macro | Replaces | Description |
|-------|----------|-------------|
| `AGENTRT_MALLOC(size)` | `malloc(size)` | Unified allocation |
| `AGENTRT_CALLOC(num, size)` | `calloc(num, size)` | Unified zero-init allocation |
| `AGENTRT_FREE(ptr)` | `free(ptr)` | Unified deallocation |

---

## 3. Upstream / Downstream Dependencies

### Upstream (Commons depends on)

| Dependency | Required | Purpose |
|------------|----------|---------|
| C11 compiler | Yes | `<stdatomic.h>` support |
| pthreads / Win32 threads | Yes | Thread and sync primitives |
| libyaml | No | Full YAML support |
| cJSON | No | JSON config parsing |

> **BAN-12**: External dependencies are detected centrally by the umbrella
> `CMakeLists.txt`; sub-modules MUST NOT call `find_package` independently.
> Detection results propagate through CMake cache variables such as
> `AGENTRT_HAS_CJSON`, `AGENTRT_HAS_YAML`.

Commons has **no** upstream Airymax-module dependency — it is the lowest
layer of the runtime.

### Downstream (consumers of Commons)

| Consumer | What it uses |
|----------|--------------|
| **atoms** | Platform abstraction, memory management, error framework, type system — every Atoms module pulls in ~18 commons sub-modules |
| **cupolas** | Type system, sync primitives, memory macros |
| **heapstore** | Logging, config_unified, memory pools, sync primitives |
| **protocols** | Type system (`agentrt_ipc_header_t`), sync, observability |
| **gateway** | Network utilities, token management, logging, config_unified |
| **daemons** | Logging, config_unified, network, token, cost, observability, cognition, strategy — full surface |

---

## 4. Build Instructions

Commons builds a single static library `agentrt_common` aggregating all
utility modules. Include paths are exported PUBLIC so consumers see every
sub-module header through a single target link.

```bash
# Standard build (from the umbrella root, or standalone)
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run the test suite
ctest --test-dir build
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | `ON` | Build unit and integration tests |
| `AGENTRT_HAS_CJSON` | auto | Auto-detected by umbrella CMake; gates cJSON-dependent code paths |
| `AGENTRT_HAS_YAML` | auto | Auto-detected by umbrella CMake; gates libyaml-dependent code paths |

When cJSON/YAML are unavailable, Commons degrades gracefully: `AGENTRT_NO_CJSON`
is defined and the JSON/YAML parsers fall back to minimal built-in implementations
(e.g. `yaml_minimal.c`).

### Build Artifacts

- `agentrt_common` — static library containing all utility modules
- Public headers installed under `include/agentrt/{platform,utils/*}`

### Installation

```bash
cmake --install build --prefix /opt/airymax
```

---

## 5. Usage Example

```c
#include "agentrt_types.h"
#include "logging.h"
#include "config_unified.h"

log_config_t log_cfg = {
    .level = LOG_LEVEL_INFO,
    .output = LOG_OUTPUT_CONSOLE,
    .format = LOG_FORMAT_JSON
};
log_init(&log_cfg);

config_context_t *ctx = config_context_create("myapp");
config_value_t *val = CONFIG_STRING("0.0.0.0");
config_context_set(ctx, "server.host", val);

LOG_INFO("system initialized, host: %s",
         CONFIG_GET_STRING_SAFE(ctx, "server.host", "localhost"));

log_cleanup();
config_context_destroy(ctx);
```

---

## 6. License

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.

This module is dual-licensed under the terms of either:

- **GNU Affero General Public License v3.0 or later**
  ([AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.txt)), or
- **Apache License, Version 2.0**
  ([Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0.txt))

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`

The full license texts are in the [LICENSE](LICENSE) file; the copyright
notice is in [NOTICE](NOTICE). You may select either license to comply with.
The AGPL-3.0-or-later terms apply by default; the Apache-2.0 alternative is
provided for downstream integration scenarios (e.g., closed-source or
proprietary distribution) that the AGPL does not accommodate.
