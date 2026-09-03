
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    # platform 兼容层按功能域拆分：主文件/同步原语/进程/路径
    # 2026-08-27 目录整理：实现集中到 src/，include/ 仅保留公共头
    platform/src/platform.c
    platform/src/platform_sync.c
    platform/src/platform_process.c
    platform/src/platform_paths.c
    # 时间服务：逻辑墙钟 / 时区 / SNTP 校对 / 周期同步（2026-08-30）
    platform/src/platform_time.c
    # 0c A 类直搬：平台扩展实现（airy_dl_*/airy_sock_*/进程/线程别名等，
    # 声明在 platform/include/daemon_platform_ext.h，从 daemons 迁入）
    platform/src/platform_compat.c
    utils/platform/platform_adapter.c
    utils/token/counter.c
    utils/token/budget.c
    utils/token/token_standard.c
    utils/cost/estimator.c
    utils/cost/controller.c
    utils/observability/logger.c
    utils/observability/metrics.c
    utils/observability/trace.c
    utils/error/handler.c
    utils/io/file_utils.c
    utils/security/input_validator.c
    utils/resource/resource_guard.c
    utils/resource/resource_quota.c
    # logging 核心按功能域拆分：核心/格式化/文件后端/运行时控制（2026-08-27）
    utils/logging/logging_core.c
    utils/logging/logging_format.c
    utils/logging/logging_backend_file.c
    utils/logging/logging_control.c
    utils/logging/atomic_logging.c
    utils/logging/service_logging.c
    # memory 核心层按功能域拆分（0.1.6）：状态机+分配 / 统计 / 调试核心
    utils/memory/memory_core.c
    utils/memory/memory_stats.c
    utils/memory/memory_debug_core.c
    utils/memory/memory_pool.c
    # memory_pool 按功能域拆分：块区生命周期/分配释放与容量/统计查询校验（2026-08-27）
    utils/memory/memory_pool_alloc.c
    utils/memory/memory_pool_stats.c
    # memory_debug 模块按功能域拆分：生命周期/分配追踪/泄漏检测/校验/统计
    utils/memory/memory_debug.c
    utils/memory/memory_debug_track.c
    utils/memory/memory_debug_leak.c
    utils/memory/memory_debug_validate.c
    utils/memory/memory_debug_stats.c
    utils/memory/memory_common.c
    utils/memory/memory_stats_reporter.c
    utils/memory/memory_prealloc.c
    utils/memory/arena.c
    utils/memory/tcache.c
    utils/string/string_common.c
    utils/string/string.c
    utils/string/string_find.c
    utils/string/string_split.c
    utils/string/string_classify.c
    utils/string/string_format.c
    utils/string/string_buffer.c
    utils/string/string_view.c
    utils/string/string_encode.c
    utils/string/safe_string_utils.c
    utils/sync/sync.c
    utils/sync/sync_platform.c
    utils/sync/sync_internal.c
    utils/sync/sync_spinlock.c
    utils/sync/sync_mutex.c
    utils/sync/sync_rwlock.c
    utils/sync/sync_semaphore.c
    utils/sync/sync_condition.c
    utils/sync/sync_event.c
    utils/sync/sync_barrier.c
    utils/sync/sync_recursive_mutex.c
    utils/sync/sync_common.c
    # 改进1（异步可中断）：取消令牌（原子标志 + 回调唤醒链）
    utils/sync/cancel_token.c
    utils/sync/thread_pool.c
    utils/cache/cache_common.c
    utils/compat/compat.c
    # POSIX ERE engine for Windows (no <regex.h>); compiled everywhere so
    # the engine itself is unit-tested on Linux too. POSIX callers use the
    # system <regex.h> via airy_regex.h and never call airy_re_* symbols.
    utils/compat/airy_regex.c
    # airy_regex ERE 引擎按功能域拆分：公共 API+VM/解析/编译（2026-08-27）
    utils/compat/airy_regex_parse.c
    utils/compat/airy_regex_compile.c
    utils/cognition/cognition_common.c
    utils/strategy/strategy_common.c
    # network 模块按功能域拆分：连接 IO/HTTP/连接池/DNS
    utils/network/network_common.c
    utils/network/network_http.c
    utils/network/network_pool.c
    utils/network/network_dns.c
    utils/uuid/uuid_generator.c
    # config_unified 按功能域组织（2026-08-27）：core（核心上下文）/service（配置服务）/source（配置源）/parse（格式解析）
    utils/config_unified/core_config.c
    # core_config 按功能域拆分：上下文/value 值对象/字符串化与调试（2026-08-27）
    utils/config_unified/core_config_value.c
    utils/config_unified/core_config_strings.c
    utils/config_unified/config_service.c
    utils/config_unified/config_service_validator.c
    utils/config_unified/config_service_hotreload.c
    utils/config_unified/config_service_crypto.c
    utils/config_unified/config_service_version.c
    utils/config_unified/config_source.c
    utils/config_unified/config_source_file.c
    utils/config_unified/config_source_env.c
    utils/config_unified/config_source_args.c
    utils/config_unified/config_source_memory.c
    utils/config_unified/config_source_manager.c
    # config_parse 按格式域拆分：INI（本文件）/JSON/YAML 结构/YAML 标量（2026-08-27）
    utils/config_unified/config_parse.c
    utils/config_unified/config_parse_json.c
    utils/config_unified/config_parse_yaml.c
    utils/config_unified/config_parse_yaml_scalar.c
    utils/config_unified/yaml_minimal.c
    utils/config_unified/yaml_minimal_lexer.c
    utils/config_unified/yaml_minimal_parser.c
    # yaml parser 按功能域拆分：块结构/节点与锚点注册/标量与流值（2026-08-27）
    utils/config_unified/yaml_minimal_anchor.c
    utils/config_unified/yaml_minimal_value.c
    utils/config_unified/yaml_minimal_node.c
    utils/config_unified/yaml_minimal_scalar.c
    utils/config_unified/yaml_minimal_serialize.c
    # execution/checkpoint 模块按功能域拆分：生命周期/持久化/快照/会话管理
    utils/execution/checkpoint.c
    utils/execution/checkpoint_persist.c
    utils/execution/checkpoint_snapshot.c
    utils/execution/checkpoint_session.c
    utils/task/task_desc.c
    # P23: 统一作用域 effect 原语（注册即副作用 + disposer + 逆序回滚）
    utils/effect/airy_effect.c
    # 阶段 2: 统一扩展注册表 + 四域 provider 注册辅助（memory/llm/tool/storage/sandbox）
    utils/ext/airy_ext.c
    utils/ext/airy_providers.c
    # 阶段 3: 品牌化 ID（trace_id/msg_id 结构化命名，类型层不可互赋）
    utils/id/airy_id.c
    # 阶段 4: 轻量内置拼音输入法词典（全拼，本地只读，无 IME 设备中文输入）
    utils/ime/airy_ime.c
    # 0c A 类直搬（daemons/common → commons，IRON-6 归位；原在 svc_common 全平台编译）
    utils/ipc/ipc_backpressure.c  # IPC 总线背压控制
    utils/ipc/jsonrpc_helpers.c    # JSON-RPC 2.0 辅助
    utils/ipc/method_dispatcher.c  # 方法分发器（注册表模式）
    utils/ipc/param_validator.c    # JSON-RPC 参数校验
    # 0c A 类直搬：airy_event_loop 全家族（三后端各自 #if 守卫，全平台编译）
    utils/sync/airy_event_loop.c
    utils/sync/airy_event_loop_epoll.c
    utils/sync/airy_event_loop_win.c
    utils/sync/airy_event_loop_kqueue.c
    utils/sync/airy_event_timer.c
    # 0c A 类直搬：observability 域
    utils/observability/unified_metrics.c
    utils/observability/alert_manager.c
    # 0c A 类直搬：resource 域（api_recovery）
    utils/resource/api_recovery.c
    # 0c A 类直搬：security 域（log_sanitizer）
    utils/security/log_sanitizer.c
)
if(NOT WIN32)
    list(APPEND SOURCES utils/ipc/ipc_common.c)
    list(APPEND SOURCES utils/ipc/ipc_common_io.c) # Phase 2.3a: 消息收发域（从 ipc_common.c 拆分）
    list(APPEND SOURCES utils/ipc/ipc_common_message.c)
    list(APPEND SOURCES utils/ipc/ipc_server_client.c)
    list(APPEND SOURCES utils/ipc/ipc_shm.c)
    list(APPEND SOURCES utils/ipc/ipc_mq.c)
    list(APPEND SOURCES utils/ipc/ipc_rpc.c)
    # 0c SSoT 收敛：熔断器唯一权威实现（daemons 776 行版迁入）
    list(APPEND SOURCES utils/ipc/circuit_breaker.c)
endif()

add_library(airy_common STATIC ${SOURCES})

# 验证源文件存在性
airy_print_info("[COMMONS] Verifying source files...")
set(_missing_count 0)
foreach(_src ${SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        airy_print_warn("[COMMONS]   [MISSING] ${_src}")
        math(EXPR _missing_count "${_missing_count} + 1")
    endif()
endforeach()
if(_missing_count EQUAL 0)
    airy_print_info("[COMMONS]   All source files verified OK")
else()
    airy_print_warn("[COMMONS]   ${_missing_count} source file(s) MISSING!")
endif()

# 验证 include 目录存在性
airy_print_info("[COMMONS] Verifying include directories...")
set(_inc_dirs
    platform/include
    utils/types
    utils/logging
    utils/error
    utils/sync
    utils/include
    utils/memory
    utils/string
    utils/compat
    utils/token
    utils/cost
    utils/observability
    utils/io
    utils/security
    utils/resource
    utils/cache
    utils/platform
    utils/config_unified
    utils/execution
    utils/cognition
    utils/strategy
    utils/network
    utils/ipc
    utils/uuid
    # P0.18.2: cJSON 三步曲宏化辅助头文件（header-only）
    utils/cjson
    utils/effect
    # 阶段 2: 统一扩展注册表头
    utils/ext
    # 阶段 4: 内置拼音输入法头
    utils/ime
    # P0-01: [SC] 共享契约层头文件（IRON-9 v2）
    include
)
set(_inc_missing 0)
foreach(_inc ${_inc_dirs})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_inc}")
        airy_print_ok("[COMMONS]   [OK] ${_inc}")
    else()
        airy_print_warn("[COMMONS]   [MISSING] ${_inc}")
        math(EXPR _inc_missing "${_inc_missing} + 1")
    endif()
endforeach()
if(_inc_missing GREATER 0)
    airy_print_warn("[COMMONS]   ${_inc_missing} include directory(ies) MISSING!")
endif()

set_source_files_properties(
    # memory 核心层按功能域拆分（0.1.6）：状态机+分配 / 统计 / 调试核心
    utils/memory/memory_core.c
    utils/memory/memory_stats.c
    utils/memory/memory_debug_core.c
    utils/memory/memory_pool.c
    utils/memory/memory_pool_alloc.c
    utils/memory/memory_pool_stats.c
    utils/memory/memory_debug.c
    utils/memory/memory_debug_track.c
    utils/memory/memory_debug_leak.c
    utils/memory/memory_debug_validate.c
    utils/memory/memory_debug_stats.c
    utils/memory/memory_common.c
    utils/memory/memory_stats_reporter.c
    utils/memory/memory_prealloc.c
    utils/memory/arena.c
    utils/memory/tcache.c
    utils/logging/logging_core.c
    utils/logging/logging_format.c
    utils/logging/logging_backend_file.c
    utils/logging/logging_control.c
    utils/logging/atomic_logging.c
    utils/logging/service_logging.c
    PROPERTIES COMPILE_DEFINITIONS "AIRY_COMPLIANCE_IMPL"
)

if(MSVC)
    target_compile_options(airy_common PRIVATE /std:c11 /utf-8 /W3 /wd4819 /wd4005 /D_ENABLE_ATOMIC_ALIGNMENT_FIX)
endif()

target_include_directories(airy_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/types
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/logging
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/sync
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/token
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cost
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/observability
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/io
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/security
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/resource
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cache
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/config_unified
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/execution
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/network
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/uuid
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/print
    # P0.18.2: cJSON 三步曲宏化辅助头文件（header-only）
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cjson
    # P23: 统一作用域 effect 原语（注册即副作用 + disposer + 逆序回滚）
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/effect
    # 阶段 2: 统一扩展注册表头
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ext
    # 阶段 4: 内置拼音输入法头
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ime
    # P0-01: [SC] 共享契约层头文件（IRON-9 v2，6 核心 + 1 补充 + uapi_compat）
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(APPLE)
    # macOS：特性宏由上层（cmake/platform.cmake 的 APPLE 分支
    # _POSIX_C_SOURCE=200112L + _DARWIN_C_SOURCE）统一决定；target 级
    # 重定义 200809L 会把 SDK 收紧成 glibc 语义，隐藏 Darwin 平台声明。
    target_compile_definitions(airy_common PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_definitions(airy_common PRIVATE _CRT_SECURE_NO_WARNINGS _GNU_SOURCE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
endif()
if(NOT AIRY_HAS_CJSON)
    target_compile_definitions(airy_common PRIVATE AIRY_NO_CJSON)
endif()

# 版本宏注入：airy_version_string()/airy_build_info() 随顶层
# project(VERSION) 走，避免 compat.c 内硬编码版本与发行版漂移
# （SSoT：顶层 CMakeLists 的 project(AgentRT VERSION x.y.z)）。
# 显示版本字符串以 VERSION 文件为权威（支持 0.1.5a 后缀；CMake 不接受
# 字母后缀，PROJECT_VERSION 仅作数字基线供构建系统比较）。
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" AIRY_RELEASE_VERSION LIMIT_COUNT 1)
string(STRIP "${AIRY_RELEASE_VERSION}" AIRY_RELEASE_VERSION)
if(NOT AIRY_RELEASE_VERSION)
    set(AIRY_RELEASE_VERSION "${PROJECT_VERSION}")
endif()
if(PROJECT_VERSION_MAJOR)
    target_compile_definitions(airy_common PRIVATE
        AIRY_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        AIRY_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        AIRY_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        AIRY_VERSION_STRING="${AIRY_RELEASE_VERSION}")
endif()

target_link_libraries(airy_common
    PUBLIC Threads::Threads
    PUBLIC airy_compile_defs
)

if(AIRY_HAS_YAML AND YAML_FOUND)
    target_link_libraries(airy_common PRIVATE ${YAML_LIBRARIES})
    target_include_directories(airy_common PRIVATE ${YAML_INCLUDE_DIRS})
endif()
if(AIRY_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(airy_common PRIVATE cjson)
        if(cJSON_INCLUDE_DIRS)
            target_include_directories(airy_common PRIVATE ${cJSON_INCLUDE_DIRS})
        endif()
    elseif(CJSON_FOUND)
        target_link_libraries(airy_common PRIVATE ${CJSON_LIBRARIES})
        if(CJSON_INCLUDE_DIRS)
            target_include_directories(airy_common PRIVATE ${CJSON_INCLUDE_DIRS})
        endif()
    endif()
endif()

install(TARGETS airy_common DESTINATION lib)
install(DIRECTORY platform/include/ DESTINATION include/agentrt/platform)
# 0.1.9 0d 扁平化：utils/<X>/ 平铺后仅安装公共头（*.h），排除 _internal.h
install(DIRECTORY utils/token/ DESTINATION include/agentrt/utils/token
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/cost/ DESTINATION include/agentrt/utils/cost
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/observability/ DESTINATION include/agentrt/utils/observability
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/logging/ DESTINATION include/agentrt/utils/logging
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/error/ DESTINATION include/agentrt/utils/error
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/config_unified/ DESTINATION include/agentrt/utils/config_unified
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/execution/ DESTINATION include/agentrt/utils/execution
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/io/ DESTINATION include/agentrt/utils/io
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/types/ DESTINATION include/agentrt/utils/types
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/security/ DESTINATION include/agentrt/utils/security
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/resource/ DESTINATION include/agentrt/utils/resource
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/memory/ DESTINATION include/agentrt/utils/memory
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/string/ DESTINATION include/agentrt/utils/string
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/sync/ DESTINATION include/agentrt/utils/sync
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/cache/ DESTINATION include/agentrt/utils/cache
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/compat/ DESTINATION include/agentrt/utils/compat
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/network/ DESTINATION include/agentrt/utils/network
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/ipc/ DESTINATION include/agentrt/utils/ipc
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/platform/ DESTINATION include/agentrt/utils/platform
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/cognition/ DESTINATION include/agentrt/utils/cognition
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/strategy/ DESTINATION include/agentrt/utils/strategy
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/cjson/ DESTINATION include/agentrt/utils/cjson
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/effect/ DESTINATION include/agentrt/utils/effect
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(DIRECTORY utils/ext/ DESTINATION include/agentrt/utils/ext
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
# 阶段 4: 内置拼音输入法头与词典安装（词典随安装分发，本地只读）
install(DIRECTORY utils/ime/ DESTINATION include/agentrt/ime
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(FILES utils/ime/data/airy_ime.dat DESTINATION share/agentrt/ime)
# P0-01: [SC] 共享契约层头文件（IRON-9 v2）
install(DIRECTORY include/airymax/ DESTINATION include/airymax)
# M1-1a: 工具目录 OpenAI tools schema 单一权威（agent_d/gateway/tool_d 共引）
install(FILES include/airy_tool_schema.h DESTINATION include/agentrt)
# M1-1d: agent.run_stream v1 事件帧协议 schema 单一权威（agent_d/gateway/sdk-rust 共引）
install(FILES include/airy_run_stream.h DESTINATION include/agentrt)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()
