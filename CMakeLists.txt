
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    # platform 兼容层按功能域拆分：主文件/同步原语/进程/路径
    # 2026-08-27 目录整理：实现集中到 src/，include/ 仅保留公共头
    platform/src/platform.c
    platform/src/platform_sync.c
    platform/src/platform_process.c
    platform/src/platform_paths.c
    utils/platform/src/platform_adapter.c
    utils/token/src/counter.c
    utils/token/src/budget.c
    utils/token/src/token_standard.c
    utils/cost/src/estimator.c
    utils/cost/src/controller.c
    utils/observability/src/logger.c
    utils/observability/src/metrics.c
    utils/observability/src/trace.c
    utils/error/src/handler.c
    utils/io/src/file_utils.c
    utils/security/src/input_validator.c
    utils/resource/src/resource_guard.c
    utils/resource/src/resource_quota.c
    # logging 核心按功能域拆分：核心/格式化/文件后端/运行时控制（2026-08-27）
    utils/logging/src/logging_core.c
    utils/logging/src/logging_format.c
    utils/logging/src/logging_backend_file.c
    utils/logging/src/logging_control.c
    utils/logging/src/atomic_logging.c
    utils/logging/src/service_logging.c
    utils/memory/src/memory.c
    utils/memory/src/memory_pool.c
    # memory_pool 按功能域拆分：块区生命周期/分配释放与容量/统计查询校验（2026-08-27）
    utils/memory/src/memory_pool_alloc.c
    utils/memory/src/memory_pool_stats.c
    # memory_debug 模块按功能域拆分：生命周期/分配追踪/泄漏检测/校验/统计
    utils/memory/src/memory_debug.c
    utils/memory/src/memory_debug_track.c
    utils/memory/src/memory_debug_leak.c
    utils/memory/src/memory_debug_validate.c
    utils/memory/src/memory_debug_stats.c
    utils/memory/src/memory_common.c
    utils/memory/src/memory_stats_reporter.c
    utils/memory/src/memory_prealloc.c
    utils/memory/src/arena.c
    utils/memory/src/tcache.c
    utils/string/src/string_common.c
    utils/string/src/string.c
    utils/string/src/string_find.c
    utils/string/src/string_split.c
    utils/string/src/string_classify.c
    utils/string/src/string_format.c
    utils/string/src/string_buffer.c
    utils/string/src/string_view.c
    utils/string/src/string_encode.c
    utils/string/src/safe_string_utils.c
    utils/sync/src/sync.c
    utils/sync/src/sync_platform.c
    utils/sync/src/sync_internal.c
    utils/sync/src/sync_spinlock.c
    utils/sync/src/sync_mutex.c
    utils/sync/src/sync_rwlock.c
    utils/sync/src/sync_semaphore.c
    utils/sync/src/sync_condition.c
    utils/sync/src/sync_event.c
    utils/sync/src/sync_barrier.c
    utils/sync/src/sync_recursive_mutex.c
    utils/sync/src/sync_common.c
    # 改进1（异步可中断）：取消令牌（原子标志 + 回调唤醒链）
    utils/sync/src/cancel_token.c
    utils/cache/src/cache_common.c
    utils/compat/src/compat.c
    # POSIX ERE engine for Windows (no <regex.h>); compiled everywhere so
    # the engine itself is unit-tested on Linux too. POSIX callers use the
    # system <regex.h> via airy_regex.h and never call airy_re_* symbols.
    utils/compat/src/airy_regex.c
    # airy_regex ERE 引擎按功能域拆分：公共 API+VM/解析/编译（2026-08-27）
    utils/compat/src/airy_regex_parse.c
    utils/compat/src/airy_regex_compile.c
    utils/cognition/src/cognition_common.c
    utils/strategy/src/strategy_common.c
    # network 模块按功能域拆分：连接 IO/HTTP/连接池/DNS
    utils/network/src/network_common.c
    utils/network/src/network_http.c
    utils/network/src/network_pool.c
    utils/network/src/network_dns.c
    utils/uuid/src/uuid_generator.c
    # config_unified 按功能域组织（2026-08-27）：core（核心上下文）/service（配置服务）/source（配置源）/parse（格式解析）
    utils/config_unified/src/core/core_config.c
    # core_config 按功能域拆分：上下文/value 值对象/字符串化与调试（2026-08-27）
    utils/config_unified/src/core/core_config_value.c
    utils/config_unified/src/core/core_config_strings.c
    utils/config_unified/src/service/config_service.c
    utils/config_unified/src/service/config_service_validator.c
    utils/config_unified/src/service/config_service_hotreload.c
    utils/config_unified/src/service/config_service_crypto.c
    utils/config_unified/src/service/config_service_version.c
    utils/config_unified/src/source/config_source.c
    utils/config_unified/src/source/config_source_file.c
    utils/config_unified/src/source/config_source_env.c
    utils/config_unified/src/source/config_source_args.c
    utils/config_unified/src/source/config_source_memory.c
    utils/config_unified/src/source/config_source_manager.c
    # config_parse 按格式域拆分：INI（本文件）/JSON/YAML 结构/YAML 标量（2026-08-27）
    utils/config_unified/src/parse/config_parse.c
    utils/config_unified/src/parse/config_parse_json.c
    utils/config_unified/src/parse/config_parse_yaml.c
    utils/config_unified/src/parse/config_parse_yaml_scalar.c
    utils/config_unified/src/parse/yaml_minimal.c
    utils/config_unified/src/parse/yaml_minimal_lexer.c
    utils/config_unified/src/parse/yaml_minimal_parser.c
    # yaml parser 按功能域拆分：块结构/节点与锚点注册/标量与流值（2026-08-27）
    utils/config_unified/src/parse/yaml_minimal_anchor.c
    utils/config_unified/src/parse/yaml_minimal_value.c
    utils/config_unified/src/parse/yaml_minimal_node.c
    utils/config_unified/src/parse/yaml_minimal_scalar.c
    utils/config_unified/src/parse/yaml_minimal_serialize.c
    # execution/checkpoint 模块按功能域拆分：生命周期/持久化/快照/会话管理
    utils/execution/src/checkpoint.c
    utils/execution/src/checkpoint_persist.c
    utils/execution/src/checkpoint_snapshot.c
    utils/execution/src/checkpoint_session.c
    utils/task/src/task_desc.c
    # P23: 统一作用域 effect 原语（注册即副作用 + disposer + 逆序回滚）
    utils/effect/src/airy_effect.c
    # 阶段 2: 统一扩展注册表 + 四域 provider 注册辅助（memory/llm/tool/storage/sandbox）
    utils/ext/src/airy_ext.c
    utils/ext/src/airy_providers.c
    # 阶段 3: 品牌化 ID（trace_id/msg_id 结构化命名，类型层不可互赋）
    utils/id/src/airy_id.c
    # 阶段 4: 轻量内置拼音输入法词典（全拼，本地只读，无 IME 设备中文输入）
    utils/ime/src/airy_ime.c
)
if(NOT WIN32)
    list(APPEND SOURCES utils/ipc/src/ipc_common.c)
    list(APPEND SOURCES utils/ipc/src/ipc_common_io.c) # Phase 2.3a: 消息收发域（从 ipc_common.c 拆分）
    list(APPEND SOURCES utils/ipc/src/ipc_common_message.c)
    list(APPEND SOURCES utils/ipc/src/ipc_server_client.c)
    list(APPEND SOURCES utils/ipc/src/ipc_shm.c)
    list(APPEND SOURCES utils/ipc/src/ipc_mq.c)
    list(APPEND SOURCES utils/ipc/src/ipc_rpc.c)
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
    utils/types/include
    utils/logging/include
    utils/error/include
    utils/sync/include
    utils/include
    utils/memory
    utils/string
    utils/compat/include
    utils/token/include
    utils/cost/include
    utils/observability/include
    utils/io/include
    utils/security/src
    utils/resource/src
    utils/memory/include
    utils/string/include
    utils/cache/include
    utils/platform/include
    utils/config_unified/include
    # 2026-08-27 按功能域组织后，跨族裸名 include 经下列子目录解析
    utils/config_unified/src/parse
    utils/config_unified/src/source
    utils/config_unified/src/service
    utils/execution/include
    utils/cognition/include
    utils/strategy/include
    utils/network/include
    utils/ipc/include
    utils/uuid/include
    # P0.18.2: cJSON 三步曲宏化辅助头文件（header-only）
    utils/cjson/include
    utils/effect/include
    # 阶段 2: 统一扩展注册表头
    utils/ext/include
    # 阶段 4: 内置拼音输入法头
    utils/ime/include
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
    utils/memory/src/memory.c
    utils/memory/src/memory_pool.c
    utils/memory/src/memory_pool_alloc.c
    utils/memory/src/memory_pool_stats.c
    utils/memory/src/memory_debug.c
    utils/memory/src/memory_debug_track.c
    utils/memory/src/memory_debug_leak.c
    utils/memory/src/memory_debug_validate.c
    utils/memory/src/memory_debug_stats.c
    utils/memory/src/memory_common.c
    utils/memory/src/memory_stats_reporter.c
    utils/memory/src/memory_prealloc.c
    utils/memory/src/arena.c
    utils/memory/src/tcache.c
    utils/logging/src/logging_core.c
    utils/logging/src/logging_format.c
    utils/logging/src/logging_backend_file.c
    utils/logging/src/logging_control.c
    utils/logging/src/atomic_logging.c
    utils/logging/src/service_logging.c
    PROPERTIES COMPILE_DEFINITIONS "AIRY_COMPLIANCE_IMPL"
)

if(MSVC)
    target_compile_options(airy_common PRIVATE /std:c11 /utf-8 /W3 /wd4819 /wd4005 /D_ENABLE_ATOMIC_ALIGNMENT_FIX)
endif()

target_include_directories(airy_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string
    ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/compat/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/security/src
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/resource/src
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cache/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/config_unified/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/execution/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cognition/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/strategy/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/uuid/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/print/include
    # P0.18.2: cJSON 三步曲宏化辅助头文件（header-only）
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cjson/include
    # P23: 统一作用域 effect 原语（注册即副作用 + disposer + 逆序回滚）
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/effect/include
    # 阶段 2: 统一扩展注册表头
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ext/include
    # 阶段 4: 内置拼音输入法头
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ime/include
    # P0-01: [SC] 共享契约层头文件（IRON-9 v2，6 核心 + 1 补充 + uapi_compat）
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(airy_common PRIVATE _CRT_SECURE_NO_WARNINGS _GNU_SOURCE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
if(NOT AIRY_HAS_CJSON)
    target_compile_definitions(airy_common PRIVATE AIRY_NO_CJSON)
endif()

# 版本宏注入：airy_version_string()/airy_build_info() 随顶层
# project(VERSION) 走，避免 compat.c 内硬编码版本与发行版漂移
# （SSoT：顶层 CMakeLists 的 project(AgentRT VERSION x.y.z)）。
if(PROJECT_VERSION_MAJOR)
    target_compile_definitions(airy_common PRIVATE
        AIRY_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        AIRY_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        AIRY_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        AIRY_VERSION_STRING="${PROJECT_VERSION}")
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
install(DIRECTORY utils/token/include/ DESTINATION include/agentrt/utils/token)
install(DIRECTORY utils/cost/include/ DESTINATION include/agentrt/utils/cost)
install(DIRECTORY utils/observability/include/ DESTINATION include/agentrt/utils/observability)
install(DIRECTORY utils/logging/include/ DESTINATION include/agentrt/utils/logging)
install(DIRECTORY utils/error/include/ DESTINATION include/agentrt/utils/error)
install(DIRECTORY utils/config_unified/include/ DESTINATION include/agentrt/utils/config_unified)
install(DIRECTORY utils/execution/include/ DESTINATION include/agentrt/utils/execution)
install(DIRECTORY utils/io/include/ DESTINATION include/agentrt/utils/io)
install(DIRECTORY utils/types/include/ DESTINATION include/agentrt/utils/types)
install(FILES utils/security/src/input_validator.h DESTINATION include/agentrt/utils/security)
install(FILES utils/resource/src/resource_guard.h DESTINATION include/agentrt/utils/resource)
install(FILES utils/resource/src/resource_quota.h DESTINATION include/agentrt/utils/resource)
install(DIRECTORY utils/memory/include/ DESTINATION include/agentrt/utils/memory)
install(DIRECTORY utils/string/include/ DESTINATION include/agentrt/utils/string)
install(DIRECTORY utils/sync/include/ DESTINATION include/agentrt/utils/sync)
install(DIRECTORY utils/cache/include/ DESTINATION include/agentrt/utils/cache)
install(DIRECTORY utils/compat/include/ DESTINATION include/agentrt/utils/compat)
install(DIRECTORY utils/network/include/ DESTINATION include/agentrt/utils/network)
install(DIRECTORY utils/ipc/include/ DESTINATION include/agentrt/utils/ipc)
install(DIRECTORY utils/platform/include/ DESTINATION include/agentrt/utils/platform)
install(DIRECTORY utils/cognition/include/ DESTINATION include/agentrt/utils/cognition)
install(DIRECTORY utils/strategy/include/ DESTINATION include/agentrt/utils/strategy)
# P0.18.2: cJSON 三步曲宏化辅助头文件（header-only）
install(DIRECTORY utils/cjson/include/ DESTINATION include/agentrt/utils/cjson)
install(DIRECTORY utils/effect/include/ DESTINATION include/agentrt/utils/effect)
# 阶段 2: 统一扩展注册表头安装
install(DIRECTORY utils/ext/include/ DESTINATION include/agentrt/utils/ext)
# 阶段 4: 内置拼音输入法头与词典安装（词典随安装分发，本地只读）
install(DIRECTORY utils/ime/include/ DESTINATION include/agentrt/ime)
install(FILES utils/ime/data/airy_ime.dat DESTINATION share/agentrt/ime)
# P0-01: [SC] 共享契约层头文件（IRON-9 v2）
install(DIRECTORY include/airymax/ DESTINATION include/airymax)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()
