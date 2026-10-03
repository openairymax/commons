# commons 模块测试 CMakeLists.txt

# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AIRY_COMPLIANCE_IMPL)

enable_testing()

set(COMMONS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# 测试源文件
set(UNIT_TEST_SOURCES
    unit/test_platform.c
    # 时间服务：逻辑墙钟 / 时区 / SNTP 校对 / 周期同步（2026-08-30）
    unit/test_platform_time.c
    unit/test_error.c
    unit/test_logger.c
    unit/test_token.c
    unit/test_cost.c
    # 统一类型定义（含 IPC 类型/错误码/宏契约）；已适配 cmocka_stub
    unit/test_types.c
    # 网络通信（连接/HTTP/连接池/DNS/工具）；network 实现全平台编译
    unit/test_network.c
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
    # 统一 LRU 缓存（B16-S6 表二 #1 下沉：实现+测试自 llm_d cache 收敛至此）
    unit/test_cache_common.c
    # UTF-8 校验（RFC 3629）与非法序列清洗（U+FFFD 替换）
    unit/test_string_utf8.c
    # 跨进程服务发现（g14 随权威实现自 daemons 迁入；自含测试框架）
    unit/test_service_discovery.c
    # 任务大厅事件磁盘格式 SSoT（§60 hall 事件写侧契约：命名/gseq 续接/prev_file/ACL）
    unit/test_hall_event.c
    # 字符串→下标哈希索引 SSoT（§130 djb2 + 墓碑删除 + 压实；含探测链回归）
    unit/test_hindex.c
    # 统一配置（§72 schema 默认值施加契约：按类型填充/幂等/既有键保持/服务接线）
    unit/test_config_unified.c
    # svc 生命周期机制族（0.1.19 S74 随权威实现自 daemons 迁入；自含测试）
    unit/test_svc_common.c
    unit/test_svc_stop.c
)

# IPC 域测试（test_ipc.c，已改 cmocka_stub 适配）：与主库条件对齐——
# commons/CMakeLists.txt 将 ipc 实现整批 if(NOT WIN32) 排除（POSIX-only），
# Windows 下注册 test_ipc 必然 LNK2001（windows-build #101 实证），
# 支持缺口解除前（Windows ipc 实现）不得在 Windows 编译。
if(NOT WIN32)
    list(APPEND UNIT_TEST_SOURCES unit/test_ipc.c)
endif()

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
            unit/test_ipc_rpc.c
        )
    endif()

    # test_service_discovery.c 已按功能域拆分，需一并编译各域文件
    if(test_source STREQUAL "unit/test_service_discovery.c")
        target_sources(${test_name} PRIVATE
            unit/test_service_discovery_lifecycle.c
            unit/test_service_discovery_discover.c
            unit/test_service_discovery_select.c
            unit/test_service_discovery_health.c
            unit/test_service_discovery_misc.c
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
        ${COMMONS_ROOT}/utils/logging
        ${COMMONS_ROOT}/utils/error
        ${COMMONS_ROOT}/utils/string
        ${COMMONS_ROOT}/utils/observability
        ${COMMONS_ROOT}/utils/types
        ${COMMONS_ROOT}/utils/token
        ${COMMONS_ROOT}/utils/cost
        ${COMMONS_ROOT}/utils/ipc
        ${COMMONS_ROOT}/utils/sd
        ${COMMONS_ROOT}/utils/svc
        ${COMMONS_ROOT}/utils/network
        ${COMMONS_ROOT}/utils/memory
        ${COMMONS_ROOT}/utils/print
        ${COMMONS_ROOT}/utils/sync
        ${COMMONS_ROOT}/utils/cache
        ${COMMONS_ROOT}/utils/effect
        ${COMMONS_ROOT}/utils/ext
        ${COMMONS_ROOT}/utils/ime
        ${COMMONS_ROOT}/utils/io
        ${COMMONS_ROOT}/utils/hall
        ${COMMONS_ROOT}/utils/ds
        ${COMMONS_ROOT}/utils/config_unified
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
