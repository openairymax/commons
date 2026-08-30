# commons 模块测试 CMakeLists.txt

# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AIRY_COMPLIANCE_IMPL)
# 包含单元测试和集成测试

enable_testing()

set(COMMONS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# 测试源文件
set(UNIT_TEST_SOURCES
    unit/test_platform.c
    # 时间服务：逻辑墙钟 / 时区 / SNTP 校对 / 周期同步（2026-08-30）
    unit/test_platform_time.c
    unit/test_error.c
    unit/test_logger.c
    # unit/test_config.c  # 禁用：config_unified模块实现与头文件不匹配
    unit/test_token.c
    unit/test_cost.c
    # unit/test_types.c  # 禁用：需要cmocka测试框架
    unit/test_ipc.c      # 已改为 cmocka_stub 适配（无真实 cmocka 依赖）
    # unit/test_network.c # 禁用：需要cmocka测试框架
    unit/test_arena_tcache.c
    unit/test_print.c
    # 改进1（异步可中断）：取消令牌（原子标志 + 回调唤醒链 + 跨线程唤醒）
    unit/test_cancel_token.c
    # P23: 统一作用域 effect 原语（注册即副作用 + disposer + 逆序回滚）
    unit/test_airy_effect.c
    # 阶段 2: 统一扩展注册表 + 四域 provider 契约
    unit/test_airy_ext.c
    # 阶段 3: 品牌化 ID（trace_id/msg_id 结构化命名）
    unit/test_airy_id.c
    # Windows POSIX ERE 引擎（Linux 上直测 airy_re_* API）
    unit/test_airy_regex.c
    # 阶段 4: 内置拼音输入法词典（全拼前缀查询 + 候选排序 + 损坏拒载）
    unit/test_ime.c
    # 文件增删改查（CRUD）跨平台往返（coding 场景基础能力）
    unit/test_io.c
)

set(INTEGRATION_TEST_SOURCES
    # integration/test_common_integration.c  # 禁用：缺少manager.h
    # integration/test_unified_modules.c    # 禁用：依赖test_common_integration
)

# 单元测试可执行文件
foreach(test_source ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    # test_ipc.c 已按功能域拆分，需一并编译各域文件
    if(test_source STREQUAL "unit/test_ipc.c")
        target_sources(${test_name} PRIVATE
            unit/test_ipc_channel.c
            unit/test_ipc_send.c
            unit/test_ipc_server.c
            unit/test_ipc_shm_mq.c
            unit/test_ipc_message.c
        )
    endif()

    # 内置拼音输入法词典路径（源码树 data/，构建树分发）
    if(test_source STREQUAL "unit/test_ime.c")
        target_compile_definitions(${test_name} PRIVATE
            AIRY_IME_TEST_DICT="${CMAKE_CURRENT_SOURCE_DIR}/../utils/ime/data/airy_ime.dat")
    endif()

    target_include_directories(${test_name} PRIVATE
        ${COMMONS_ROOT}/include
        ${COMMONS_ROOT}/platform/include
        ${COMMONS_ROOT}/utils/logging/include
        ${COMMONS_ROOT}/utils/error/include
        ${COMMONS_ROOT}/utils/string/include
        ${COMMONS_ROOT}/utils/observability/include
        ${COMMONS_ROOT}/utils/types/include
        ${COMMONS_ROOT}/utils/token/include
        ${COMMONS_ROOT}/utils/cost/include
        ${COMMONS_ROOT}/utils/ipc/include
        ${COMMONS_ROOT}/utils/network/include
        ${COMMONS_ROOT}/utils/memory/include
        ${COMMONS_ROOT}/utils/print/include
        ${COMMONS_ROOT}/utils/sync/include
        ${COMMONS_ROOT}/utils/effect/include
        ${COMMONS_ROOT}/utils/ext/include
        ${COMMONS_ROOT}/utils/ime/include
        ${COMMONS_ROOT}/utils/io/include
        ${COMMONS_ROOT}/tests/utils
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${test_name} PRIVATE
        airy_common
        airy_core
    )

    add_test(
        NAME "commons_test_${test_name}"
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# 集成测试可执行文件
foreach(test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    target_include_directories(${test_name} PRIVATE
        ${COMMONS_ROOT}/platform/include
        ${COMMONS_ROOT}/utils/logging/include
        ${COMMONS_ROOT}/utils/error/include
        ${COMMONS_ROOT}/utils/string/include
        ${COMMONS_ROOT}/utils/observability/include
        ${COMMONS_ROOT}/utils/types/include
        ${COMMONS_ROOT}/../atoms/corekern/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${test_name} PRIVATE
        airy_common
        airy_core
    )

    add_test(
        NAME "commons_itest_${test_name}"
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()
