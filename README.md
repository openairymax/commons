# commons — Unified Foundation Library

> The lowest layer of the Airymax runtime: every other agentrt module builds on commons.

**Language:** English | [简体中文](README_zh.md)

[![Version](https://img.shields.io/badge/version-0.1.15-5a6b7e)](https://atomgit.com/openairymax/commons)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

---

## What this is

**commons** is the foundational library of the Airymax agent runtime
([agentrt](https://atomgit.com/openairymax/agentrt)). It provides the
cross-platform, cross-module infrastructure that every upper layer — atomic
primitives ([atoms](https://atomgit.com/openairymax/atoms)), the security dome
([cupolas](https://atomgit.com/openairymax/cupolas)), storage
([heapstore](https://atomgit.com/openairymax/heapstore)), wire protocols
([protocols](https://atomgit.com/openairymax/protocols)), the gateway
([gateway](https://atomgit.com/openairymax/gateway)) and the daemon services
([daemons](https://atomgit.com/openairymax/daemons)) — is built upon.

commons depends on **no** other Airymax module: it sits at the bottom of the
dependency graph and only relies on the operating system, the C11 compiler,
and optional external libraries (pthreads, libyaml, cJSON).

It builds as a **single static library**, target `airy_common`, aggregating
32 cohesive utility modules plus a platform abstraction layer. Include paths
are exported as `PUBLIC`, so a consumer that links `airy_common` sees every
sub-module header through one link.

## Capabilities

- **Unified type & error contract** — `airy_err_t` and the branded ID types
  (`airy_trace_id_t` / `airy_msg_id_t`) in `include/airy_types.h`; the
  cross-boundary error-code contract in `include/airymax/error.h`; the
  128-byte IPC wire header `struct airy_ipc_msg_hdr` in `include/airymax/ipc.h`.
  One type system keeps every module ABI-compatible.
- **Platform abstraction** — `platform/` hides OS differences (threads,
  sync, paths, process, time, filesystem, dynamic loading) behind one API
  for Linux / Windows / macOS.
- **High-performance infrastructure** — memory pools, sync primitives and
  queues, LRU/TTL cache, event loop and timers, atomic-operation compat layer.
- **Agent-runtime utilities** — token counting and budgets, cost estimation,
  task checkpoints, cognition/strategy helpers, service discovery, IPC/RPC,
  unified configuration with a minimal built-in YAML parser, a pinyin IME
  dictionary.
- **Observability** — logging (console/file/JSON), metrics and traces with a
  Prometheus-style export surface.
- **Safety by default** — input validation, path normalization, resource
  guards and quotas, log sanitization, and an optional strict-compliance mode
  that poisons unsafe libc functions project-wide.

## Directory layout

```
commons/
├── CMakeLists.txt               # builds the single static lib airy_common
├── README.md / README_zh.md     # this file (English / Chinese)
├── LICENSE                      # dual license texts (AGPL-3.0-or-later OR Apache-2.0)
├── NOTICE                       # copyright notice
├── platform/                    # platform abstraction layer
│   ├── include/                 # public headers (platform.h + domain-split headers)
│   ├── compat/                  # compatibility headers (stdbool.h, stdint.h)
│   └── src/                     # implementation (base / paths / process / sync / time)
├── include/                     # global public headers
│   ├── airy_types.h             # unified type and error contract entry point
│   ├── airy_defaults.h          # project-wide defaults (paths, limits, tuning knobs)
│   ├── airy_run_stream.h        # run-stream contract types
│   ├── airy_tool_schema.h       # tool schema contract types
│   ├── airyrt_version.h         # version macros
│   ├── airymax/                 # cross-boundary contract headers
│   │   ├── error.h              # airy_err_t + AIRY_E* / AIRY_FAULT_* codes
│   │   ├── ipc.h                # airy_ipc_msg_hdr (128-byte wire header)
│   │   ├── task_desc.h          # task descriptor contract
│   │   ├── uapi_compat.h        # user/kernel ABI helpers
│   │   └── syscalls.h sched.h memory_types.h cognition_types.h
│   │       security_types.h lsm_types.h log_types.h bpf_struct_ops.h
│   └── third_party/             # vendored third-party headers (e.g. nghttp2)
├── utils/                       # 32 utility modules (+ utils/include shared headers)
│   ├── include/                 # cross-module shared headers
│   │   ├── atomic_compat.h      # cross-platform atomic operations (umbrella)
│   │   ├── atomic_compat_api.h  # atomic API per type
│   │   ├── atomic_compat_platform.h # atomic backend selection
│   │   ├── logging_compat.h     # logging compatibility shim
│   │   └── check.h              # generic check macros
│   ├── logging/  sync/  memory/  string/  ipc/  token/  cost/
│   ├── observability/  platform/  error/  types/  config_unified/
│   ├── execution/  io/  cache/  compat/  cognition/  strategy/
│   ├── network/  security/  resource/  uuid/  print/  compliance/
│   ├── quality/  sd/  effect/  ext/  id/  task/  cjson/  ime/
│   └── <module>/                # each module is flat: headers + sources in one dir
└── tests/                       # test suite (unit / bench / helpers)
```

Each utility module keeps its public headers and implementation files in the
module root — there are no per-module `include/` or `src/` sub-directories.
Every module directory carries its own README with the API details.

## Module list

| Module | Responsibility |
|--------|----------------|
| logging | 3-tier logging (core → atomic → service); JSON/text formats |
| sync | sync primitives (mutex, recursive mutex, rwlock, spinlock, semaphore, condition, barrier, event), cancel token, thread pool, event loop, timer |
| memory | unified allocation macros, pools, prealloc, guards, stats, debug helpers |
| string | string operations, safe string utilities |
| ipc | IPC abstraction (channel/server/client/shm/mq/rpc), JSON-RPC helpers, circuit breaker |
| token | LLM token counting and budgets; API-key standard |
| cost | cost estimation and budget controller |
| observability | metrics, traces, structured logger facade, unified metrics, alert manager |
| platform | platform adapter utilities (fs, env, paths, time, sysinfo) |
| error | error handling macros and user-space extended error codes |
| types | shared type definitions (generic types, cupolas/LLM/tool type headers) |
| config_unified | 3-tier configuration (core → source → service) with built-in minimal YAML parser |
| execution | task checkpoints (persistence / session / snapshot) |
| io | file read/write, directory helpers |
| cache | LRU / TTL cache |
| compat | cross-platform compatibility (regex, dirent, mman, netdb, unistd) |
| cognition | cognition helpers (agent info, planning, coordination) |
| strategy | weighted scoring and agent-selection strategy |
| network | HTTP client, connection pool, DNS resolution |
| security | input validation, log sanitization |
| resource | resource guards, quotas, API recovery |
| uuid | UUID generation and parsing |
| print | unified runtime print macros (airy_print_*) |
| compliance | strict-compliance unsafe-function poisoning and exemption macros |
| quality | code-quality check macros (null checks, scope guards, numeric safety) |
| sd | cross-process service discovery (shm registry, heartbeat, expiry) |
| effect | rollback effect scopes (register-now, undo-in-reverse) |
| ext | unified provider registry (LLM / tool / storage / sandbox domains) |
| id | branded ID generation (trace_id / msg_id) |
| task | task descriptor creation with CRC32 integrity |
| cjson | cJSON helper macros (parse guard, auto-free, deep copy) |
| ime | lightweight pinyin IME (binary dictionary shipped with install) |

## Usage & build

commons is normally built as part of the
[agentrt](https://atomgit.com/openairymax/agentrt) tree, which adds it via
`add_subdirectory` and exposes the `airy_common` target:

```bash
cmake -S agentrt -B agentrt/build -DCMAKE_BUILD_TYPE=Release
cmake --build agentrt/build --parallel
```

Consumers link the single target:

```cmake
target_link_libraries(my_component PRIVATE airy_common)
```

All module include directories are exported as `PUBLIC`, so linking
`airy_common` makes every sub-module header visible.

**Key build options** (defaults from the agentrt root `CMakeLists.txt`):

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | `ON` | Build unit tests and benchmarks |
| `BUILD_SHARED_LIBS` | `OFF` | commons is a static library |
| `WARNINGS_AS_ERRORS` | `OFF` | Promote warnings to errors in CI |
| `ENABLE_SANITIZERS` | `OFF` | ASan/UBSan instrumentation |
| `AIRY_COMPLIANCE_STRICT` | `ON` | Poison unsafe libc functions via `utils/compliance/banned_functions.h` (injected project-wide with `-include`) |
| `AIRY_HAS_CJSON` | auto | Set by dependency detection; gates cJSON code paths |
| `AIRY_HAS_YAML` | auto | Set by dependency detection; gates libyaml code paths |

When cJSON or libyaml is unavailable, commons degrades gracefully:
`AIRY_NO_CJSON` is defined and configuration parsing falls back to the built-in
minimal YAML parser (`utils/config_unified/yaml_minimal`).

**Install layout:** the static library plus public headers under
`include/agentrt/{platform,utils/*}`; the IME dictionary installs to
`share/agentrt/ime`.

### Quick example

```c
#include "airy_types.h"
#include "logging.h"
#include "config_unified.h"

int main(void) {
    log_config_t log_cfg = {0};
    log_init(&log_cfg);

    config_context_t *ctx = config_context_create("myapp");
    config_context_set(ctx, "server.host", CONFIG_STRING("0.0.0.0"));

    log_write(LOG_LEVEL_INFO, "demo", __LINE__, "host=%s",
              CONFIG_GET_STRING_SAFE(ctx, "server.host", "localhost"));

    config_context_destroy(ctx);
    log_cleanup();
    return 0;
}
```

## Relationship to other modules

```
┌──────────────────────────────────────────────┐
│                  applications                │
├──────────────────────────────────────────────┤
│          daemons / gateway / protocols       │
├──────────────────────────────────────────────┤
│           atoms / cupolas / heapstore        │
├──────────────────────────────────────────────┤
│                ★ commons ★                   │  ← lowest layer, zero upstream deps
├──────────────────────────────────────────────┤
│             operating system / hardware      │
└──────────────────────────────────────────────┘
```

- **atoms** — platform abstraction, type/error contract, and most utility
  modules (logging, sync, memory, error, types, config_unified, observability…).
- **cupolas** — type system, sync primitives, memory macros, security and
  resource utilities.
- **heapstore** — logging, configuration, memory pools, sync primitives.
- **protocols** — `airy_err_t`, `struct airy_ipc_msg_hdr`, sync, observability.
- **gateway** — network utilities, token management, logging, configuration.
- **daemons** — the full surface: logging, config, network, token, cost,
  observability, cognition, strategy, IPC, service discovery.

The language SDKs ultimately bind back to these same commons types and error
codes, keeping host and embedded sides of the runtime consistent.

## Documentation

Project documentation lives under the `docs/AirymaxRT/` tree of
[openairymax/docs](https://atomgit.com/openairymax/docs), including the
engineering-standards handbook that governs this repository.

## License

Copyright (c) 2025-2026 SPHARX Ltd. and contributors.

This module is dual-licensed under the terms of either:

- **GNU Affero General Public License v3.0 or later**
  ([AGPL-3.0-or-later](https://www.gnu.org/licenses/agpl-3.0.txt)), or
- **Apache License, Version 2.0**
  ([Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0.txt))

SPDX-License-Identifier: `AGPL-3.0-or-later OR Apache-2.0`

The full license texts are in the [LICENSE](LICENSE) file and the copyright
notice in [NOTICE](NOTICE). You may select either license to comply with.
The Apache-2.0 alternative is provided for downstream integration scenarios
that the AGPL does not accommodate.
