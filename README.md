# commons — Unified Foundation Library

> The lowest layer of the Airymax runtime: every other agentrt module builds on commons.
> Leaf repository under the [agentrt](../) management repo.

**Language:** English | [简体中文](README_zh.md)

[![Version](https://img.shields.io/badge/version-0.1.1-5a6b7e)](https://atomgit.com/openairymax/commons)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

- **Repository:** `git@atomgit.com:openairymax/commons.git`
- **Branch:** `feature/official-hubs-01`
- **Version:** 0.1.1 (Airymax foundational release)

---

## Overview

**commons** is the **foundational layer** of the Airymax agent runtime. It provides the cross-platform, cross-language, cross-module infrastructure that every upper layer — atomic primitives (`atoms`), daemons, the security dome (`cupolas`), the gateway, protocols, and storage — is built upon. commons itself depends on **no** other Airymax module: it is the lowest layer in the dependency graph and the canonical substrate to which the SDK layer ultimately binds back, closing the Airymax cyclic architecture.

As the project's authoritative source for type definitions (`airy_types.h`) and the unified error-code contract (`airy_err_t`), commons guarantees type consistency across modules and eliminates cross-module type conflicts. Its design goals are: zero-dependency abstraction (platform-agnostic type system and interface definitions keep the kernel decoupled from peripheral code), a unified error contract, high-performance infrastructure (memory pools, lock-free queues, zero-copy pipelines), built-in observability (standardized capture interfaces for logs/metrics/traces), and safe-by-default I/O paths (parameter validation, boundary checks, resource limits).

commons builds a single static library `airy_common` aggregating 32 utility modules; include paths are exported PUBLIC so consumers see every sub-module header through a single target link. Within the Airymax 0.1.1 release, the workspace is partitioned into **38 repositories** (1 umbrella + 5 management + 29 leaf + 3 top-level); `commons` is one of the 7 leaf repositories aggregated by the [agentrt](../) management repo, and is the **single point of foundation** shared by the other 6 leaf repos.

## Module Classification

**Class A — Foundational / Atomic.**

commons is the absolute lowest leaf repository in agentrt. It has zero intra-agentrt upstream dependencies (it depends only on the OS / compiler / optional external libs) and is consumed by every other leaf repo (atoms, cupolas, heapstore, protocols, gateway, daemons). As a Class-A module, commons guarantees ABI stability within a major version and provides the type system + error contract that bind the entire runtime together.

## Directory Structure

```
commons/
├── CMakeLists.txt               # CMake build configuration (single static lib airy_common)
├── README.md                    # This file (English)
├── README_zh.md                 # Chinese version
├── LICENSE                      # Dual license texts (AGPL-3.0 + Apache-2.0)
├── NOTICE                       # Copyright notice
├── platform/                    # Platform abstraction layer
│   ├── include/
│   │   ├── platform.h           # Platform detection and base definitions
│   │   └── export.h             # Symbol export control
│   ├── compat/                  # Platform compatibility headers (stdbool.h, stdint.h)
│   ├── platform.c               # Base tools: network/atomic/time/random/fs/string/sysinfo/file lock
│   ├── platform_paths.c         # AIRY_HOME path system (bin/lib/run/logs/config/data/tmp/cache/workspace)
│   ├── platform_process.c       # Process domain (start/wait/kill/run_capture)
│   ├── platform_sync.c          # Sync domain (threads, mutex, cond, thread naming)
│   └── platform_internal.h      # Cross-domain shared header (domain split)
├── include/                     # Global public headers
│   ├── airy_types.h          # Unified type and error-code definitions (authoritative)
│   ├── airy_defaults.h       # Project-wide defaults (paths, limits, tuning knobs)
│   └── airymax/              # Unified Airymax type contracts (task_desc/uapi/syscalls/ipc/sched/…)
├── third_party/                 # Vendored third-party headers (e.g. nghttp2)
├── utils/                       # Tooling module collection (32 modules)
│   ├── include/                 # Cross-module shared headers
│   │   ├── atomic_compat.h      # Cross-platform atomic operation compat layer
│   │   └── check.h              # Generic check macros
│   ├── logging/                 # Logging (3-tier: Core → Atomic → Service)
│   ├── sync/                    # Sync primitives (8+ locks & queues)
│   ├── memory/                  # Memory management (pools, smart ptrs, zero-copy, arena, tcache)
│   ├── string/                  # String operations and safe formatting
│   ├── ipc/                     # IPC abstraction layer
│   ├── token/                   # Token management (API Key / JWT lifecycle, counter, budget)
│   ├── cost/                    # Cost estimation and control
│   ├── observability/           # Observability (OpenTelemetry metrics/traces, logger)
│   ├── platform/                # Platform-specific tooling functions (platform_adapter)
│   ├── error/                   # Error handling framework
│   ├── types/                   # Generic type definitions and conversions
│   ├── config_unified/          # Unified config (3-tier: Core → Source → Service; yaml_minimal)
│   ├── execution/               # Task checkpoint (persistence / session / snapshot)
│   ├── io/                      # File I/O utilities
│   ├── cache/                   # Cache management (LRU / TTL)
│   ├── compat/                  # Cross-version / cross-platform compatibility
│   ├── cognition/               # Cognition mgmt (agent info, scheduling, planning)
│   ├── strategy/                # Weighted scoring strategy engine
│   ├── network/                 # Network utilities (HTTP / URI / DNS)
│   ├── security/                # Input validation and security filtering
│   ├── resource/                # Resource protection and quotas (guard, quota)
│   ├── uuid/                    # UUID generation and parsing
│   ├── print/                   # Unified runtime print macros (airy_print_*)
│   ├── compliance/              # Compliance validation and policy enforcement
│   ├── quality/                 # Code quality checks
│   ├── sd/                      # Cross-process service discovery (shm registry)
│   ├── effect/                  # Rollback effect (airy_effect: register-now, undo-in-reverse)
│   ├── ext/                     # Unified extension registry (LLM/tool/storage/sandbox/memory)
│   ├── id/                      # Branded ID generation (trace_id / msg_id)
│   ├── task/                    # A-TD task descriptor (create + CRC32 integrity)
│   ├── cjson/                   # cJSON macro helper layer (CJSON_PARSE_GUARD etc.)
│   └── ime/                     # Lightweight built-in IME (pinyin dict, pure C)
└── tests/                       # Test suite
    ├── utils/                   # Test utility framework
    ├── unit/                    # Unit tests
    └── integration/             # Integration tests
```

## Core Components

### Type system (`airy_types.h`)

The single authoritative source of type definitions for the entire project:

| Type | Description |
|------|-------------|
| `airy_err_t` | Unified error code type (`int32_t`; negative = error, 0 = success) |
| `airy_ipc_hdr_t` | Application-level IPC message header (magic/version/type/flags/msg_id) |
| `airy_ipc_msg_t` | Application-level IPC message struct (header + payload) |
| `airy_task_id_t` | Task ID type (`uint64_t`) |
| `airy_message_id_t` | Message ID type (`uint64_t`) |

The unified error code system (`AIRY_E*`) covers 29 standard errors including invalid argument, out of memory, permission denied, timeout, I/O error, protocol error, quota exceeded, etc.

### Utility modules (32)

| Module | Path | Responsibility |
|--------|------|----------------|
| logging | `utils/logging/` | 3-tier logging (Core → Atomic → Service); JSON/text formats; atomic logging |
| sync | `utils/sync/` | 8+ sync primitives: spinlock, mutex, rwlock, semaphore, condition, event, barrier, recursive mutex |
| memory | `utils/memory/` | Memory pools, smart pointers, zero-copy, arena, tcache, prealloc, debug, stats reporter |
| string | `utils/string/` | String operations and safe string utilities (replaces unsafe `strcpy` family under strict compliance) |
| ipc | `utils/ipc/` | IPC abstraction layer (ipc_common; POSIX-only) |
| token | `utils/token/` | Token management: API Key / JWT lifecycle, counter, budget |
| cost | `utils/cost/` | Cost estimation and controller |
| observability | `utils/observability/` | OpenTelemetry-aligned metrics/traces; logger |
| platform | `utils/platform/` | Platform-specific tooling (platform_adapter) |
| error | `utils/error/` | Error handling framework (handler) |
| types | `utils/types/` | Generic type definitions and conversions |
| config_unified | `utils/config_unified/` | 3-tier config (Core → Source → Service); yaml_minimal fallback |
| execution | `utils/execution/` | Task checkpoint (persistence / session / snapshot / stats) |
| io | `utils/io/` | File I/O utilities (file_utils) |
| cache | `utils/cache/` | Cache management (LRU / TTL); cache_common |
| compat | `utils/compat/` | Cross-version / cross-platform compatibility (compat, airy_regex) |
| cognition | `utils/cognition/` | Cognition management (agent info, scheduling, planning) |
| strategy | `utils/strategy/` | Weighted scoring strategy engine |
| network | `utils/network/` | Network utilities (HTTP / URI / DNS) |
| security | `utils/security/` | Input validation and security filtering (input_validator) |
| resource | `utils/resource/` | Resource protection and quotas (resource_guard, resource_quota) |
| uuid | `utils/uuid/` | UUID generation and parsing (uuid_generator) |
| print | `utils/print/` | Unified runtime print macros (airy_print_*, delegating to log_write) |
| compliance | `utils/compliance/` | Compliance validation and policy enforcement |
| quality | `utils/quality/` | Code quality checks |
| sd | `utils/sd/` | Cross-process service discovery (shm registry, heartbeat/expiry/load-balancing) |
| effect | `utils/effect/` | Rollback effect (airy_effect: register-now, undo-in-reverse) |
| ext | `utils/ext/` | Unified extension registry (LLM/tool/storage/sandbox/memory domains) |
| id | `utils/id/` | Branded ID generation (trace_id / msg_id) |
| task | `utils/task/` | A-TD task descriptor (create + CRC32 integrity validation) |
| cjson | `utils/cjson/` | cJSON macro helper layer (CJSON_PARSE_GUARD etc.) |
| ime | `utils/ime/` | Lightweight built-in IME (pinyin dictionary, pure C, shipped with install) |

### Platform abstraction layer (`platform/`)

The foundation of the whole runtime — every cross-platform capability
goes through this layer. Domain-split into base tools / paths / process /
sync (shared via `platform_internal.h`, public API in `platform.h`):

- **Platform detection** — auto-detects Linux / Windows / macOS and the
  architecture (`AIRY_PLATFORM_BITS` via `UINTPTR_MAX`, covering x86_64,
  aarch64, armv7l and riscv64).
- **System info** — `airy_get_sysinfo()` returns CPU core count, total
  memory, CPU model and process ID from `/proc` (Linux), `sysctl`
  (macOS) or the registry (Windows); `cpu_model` feeds installer
  hardware auto-configuration and capability trimming.
- **File locking** — `airy_file_lock()` / `airy_file_unlock()` via
  `fcntl` (POSIX) / `LockFileEx` (Windows) for install-time and daemon
  mutual exclusion.
- **Threads & sync** — `airy_thread_t`, `airy_mtx_t`, `airy_cond_t`,
  plus `airy_thread_set_name()` / `airy_thread_get_name()` for
  debuggable worker threads.
- **Filesystem** — AIRY_HOME path system (`airy_home_dir()`,
  `airy_runtime_dir()`, `airy_log_dir()`, …) plus path normalization and
  file-operation abstraction.
- **Dynamic library loading** — cross-platform FFI support.

### Atomic operation compat layer (`atomic_compat.h`)

| Backend | Environment | Implementation |
|---------|-------------|----------------|
| C11 stdatomic | Linux / macOS (C11+ compiler) | `<stdatomic.h>` |
| Windows Interlocked | Windows (MSVC / MinGW) | `<intrin.h>` Interlocked API |
| POSIX fallback | Older GCC/Clang | `__atomic` builtins |

Covers 11 types including `atomic_bool`, `atomic_int`, `atomic_uint`, `atomic_int64_t`, `atomic_uint64_t`, `atomic_size_t`.

### Unified memory management (`airy_memory.h`)

| Macro | Replaces | Description |
|-------|----------|-------------|
| `AIRY_MALLOC(size)` | `malloc(size)` | Unified allocation |
| `AIRY_CALLOC(num, size)` | `calloc(num, size)` | Unified zero-init allocation |
| `AIRY_FREE(ptr)` | `free(ptr)` | Unified deallocation |

## Architecture

```
┌──────────────────────────────────────────────┐
│             Applications (OpenLab)            │
├──────────────────────────────────────────────┤
│             Ecosystem (Toolkit / SDK)         │
├──────────────────────────────────────────────┤
│              Daemon Services                  │
├──────────────────────────────────────────────┤
│   protocols / gateway / heapstore / cupolas   │
├──────────────────────────────────────────────┤
│                  atoms                        │
├──────────────────────────────────────────────┤
│          ★ commons (Support Layer) ★         │  ← lowest layer, zero intra-agentrt deps
├──────────────────────────────────────────────┤
│            Operating System / Hardware        │
└──────────────────────────────────────────────┘

  airy_common (single static lib)
  ┌────────────────────────────────────────────┐
  │  airy_types.h  (authoritative types)    │
  │  platform/        (OS abstraction)         │
  │  utils/           (32 util modules)        │
  │   logging sync memory string ipc token     │
  │   cost observability platform error types  │
  │   config_unified execution io cache compat │
  │   cognition strategy network security      │
  │   resource uuid print compliance quality sd│
  │   effect ext id task cjson ime             │
  └────────────────────────────────────────────┘
        ▲     ▲     ▲     ▲     ▲     ▲
        │     │     │     │     │     │
      atoms cupolas heapstore protocols gateway daemons
```

## Upstream Dependencies

> commons is the lowest layer of the runtime — it has **no** upstream Airymax-module dependency.

| Dependency | Required | Purpose |
|------------|----------|---------|
| C11 compiler | Yes | `<stdatomic.h>` support, atomic compat layer |
| pthreads / Win32 threads | Yes | Thread and sync primitives |
| libyaml | No | Full YAML support (falls back to `yaml_minimal.c`) |
| cJSON | No | JSON config parsing (gated by `AIRY_HAS_CJSON`) |

> **BAN-12**: External dependencies are detected centrally by the umbrella `CMakeLists.txt`; sub-modules MUST NOT call `find_package` independently. Detection results propagate through CMake cache variables such as `AIRY_HAS_CJSON`, `AIRY_HAS_YAML`.

When cJSON/YAML are unavailable, commons degrades gracefully: `AIRY_NO_CJSON` is defined and the JSON/YAML parsers fall back to minimal built-in implementations.

## Downstream Consumers

> commons is the foundation for **all** agentrt modules — every other leaf repo consumes it.

| Consumer | What they use |
|----------|---------------|
| **atoms** | Platform abstraction, memory management, error framework, type system — every atoms sub-module pulls in ~18 commons util modules (logging, sync, platform, error, types, config_unified, observability, ipc, io, cache, cost, memory, string, token, network, security, resource, uuid) |
| **cupolas** | Type system (`airy_types.h`), sync primitives, memory macros, security/resource utilities for the safety dome |
| **heapstore** | Logging, config_unified, memory pools, sync primitives for heap persistence |
| **protocols** | Type system (`airy_ipc_hdr_t` / `airy_ipc_msg_t`), sync, observability, ipc for the AgentsIPC wire format |
| **gateway** | Network utilities, token management, logging, config_unified for the gateway daemon |
| **daemons** | Logging, config_unified, network, token, cost, observability, cognition, strategy — full surface consumed by all 12 daemons |
| SDK layer | The Python/Go/Rust/TypeScript SDKs ultimately bind back to commons types and error contract, closing the Airymax cyclic architecture |

## Build

commons builds a single static library `airy_common` aggregating all utility modules. Include paths are exported PUBLIC so consumers see every sub-module header through a single target link.

```bash
# Standard build (out-of-source, enforced by BAN-33)
cmake -S . -B /tmp/commons-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/commons-build --parallel $(nproc)

# Enable unit + integration tests
cmake -S . -B /tmp/commons-build -DBUILD_TESTS=ON
ctest --test-dir /tmp/commons-build --output-on-failure

# Install
cmake --install /tmp/commons-build --prefix /opt/airymax
```

**CMake options:**

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | `ON` | Build unit and integration tests |
| `AIRY_HAS_CJSON` | auto | Auto-detected by umbrella CMake; gates cJSON-dependent code paths |
| `AIRY_HAS_YAML` | auto | Auto-detected by umbrella CMake; gates libyaml-dependent code paths |

**Build artifacts:**

- `airy_common` — static library containing all utility modules
- Public headers installed under `include/agentrt/{platform,utils/*}`

## API

commons exposes its surface through the unified type header and per-module public headers. The authoritative entry points:

- `include/airy_types.h` — unified type and error-code definitions (`airy_err_t`, `airy_ipc_hdr_t`, `airy_ipc_msg_t`, `AIRY_E*` codes)
- `platform/include/platform.h` — platform detection and base definitions
- `utils/include/atomic_compat.h` — cross-platform atomic operations (11 types, 3 backends)
- `utils/include/check.h` — generic check macros
- Per-module entry headers: `utils/logging/include/logging.h`, `utils/sync/include/sync.h`, `utils/memory/include/airy_memory.h`, `utils/string/include/airy_string.h`, `utils/config_unified/include/config_unified.h`, `utils/observability/include/observability.h`, `utils/token/include/token.h`, `utils/cost/include/cost.h`, `utils/error/include/error.h`, `utils/network/include/network_common.h`, `utils/security/src/input_validator.h`, `utils/resource/src/resource_guard.h`, `utils/uuid/include/uuid_generator.h`, `utils/cache/include/cache_common.h`, `utils/io/include/io.h`, `utils/ipc/include/ipc_common.h`, `utils/execution/include/checkpoint.h`, `utils/cognition/include/cognition_common.h`, `utils/strategy/include/strategy_common.h`, `utils/types/include/types.h`, `utils/platform/include/platform_adapter.h`, `utils/compat/include/compat.h`, `utils/print/include/airy_print.h`, `utils/compliance/include/compliance_exempt.h`, `utils/quality/airy_quality.h`, `utils/sd/include/service_discovery.h`, `utils/effect/include/airy_effect.h`, `utils/ext/include/airy_ext.h`, `utils/cjson/include/cjson_helpers.h`, `utils/ime/include/airy_ime.h`

Memory macros (`AIRY_MALLOC` / `AIRY_CALLOC` / `AIRY_FREE`) and the strict-compliance unsafe-function poisoning (e.g. `strcpy` replacement via `utils/string`) are project-wide.

### Usage example

```c
#include "airy_types.h"
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

## License

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.

This module is dual-licensed under the terms of either:

- **GNU Affero General Public License v3.0 or later**
  ([AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.txt)), or
- **Apache License, Version 2.0**
  ([Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0.txt))

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`

The full license texts are in the [LICENSE](LICENSE) file; the copyright notice is in [NOTICE](NOTICE). You may select either license to comply with. The AGPL-3.0-or-later terms apply by default; the Apache-2.0 alternative is provided for downstream integration scenarios (e.g., closed-source or proprietary distribution) that the AGPL does not accommodate.
