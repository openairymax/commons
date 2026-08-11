/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 *
 * @file network_common.h
 * @brief 网络通信模块 - 跨平台网络抽象层
 *
 * @details
 * 本模块提供跨平台的网络通信功能，包括：
 * - TCP/UDP Socket 连接
 * - HTTP/HTTPS 客户端
 * - WebSocket 支持
 * - 连接池管理
 * - 超时与重试机制
 *
 * 支持平台：
 * - Windows (Winsock2)
 * - Linux (POSIX Socket)
 * - macOS (POSIX Socket)
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-04-02
 * @version 0.1.0
 *
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性原则
 */

#ifndef AIRY_RT_NETWORK_COMMON_H
#define AIRY_RT_NETWORK_COMMON_H

#include <error.h>
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 常量定义
 * ============================================================================ */


#define NETWORK_DEFAULT_TIMEOUT_MS 30000


#define NETWORK_DEFAULT_MAX_RETRIES 3


#define NETWORK_DEFAULT_RETRY_INTERVAL 1000


#define NETWORK_DEFAULT_BUFFER_SIZE 8192


#define NETWORK_MAX_POOL_SIZE 32


#define NETWORK_MAGIC 0x4E455457 /* "NETW" */
/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief 网络连接状态枚举
 */
typedef enum {
    NETWORK_STATUS_DISCONNECTED = 0,
    NETWORK_STATUS_CONNECTING = 1,
    NETWORK_STATUS_CONNECTED = 2,
    NETWORK_STATUS_DISCONNECTING = 3,
    NETWORK_STATUS_ERROR = 4
} network_status_t;

/**
 * @brief Socket 类型枚举
 */
typedef enum {
    NETWORK_SOCK_STREAM = 1,
    NETWORK_SOCK_DGRAM = 2,
    NETWORK_SOCK_RAW = 3
} network_sock_type_t;

/**
 * @brief 地址族枚举
 */
typedef enum {
    NETWORK_AF_UNSPEC = 0,
    NETWORK_AF_INET = 2, /**< IPv4 */
    NETWORK_AF_INET6 = 10 /**< IPv6 */
} network_af_t;

/**
 * @brief SSL/TLS 验证模式
 */
typedef enum {
    NETWORK_SSL_VERIFY_NONE = 0,
    NETWORK_SSL_VERIFY_PEER = 1,
    NETWORK_SSL_VERIFY_FAIL_IF_NO_PEER_CERT = 2,
    NETWORK_SSL_VERIFY_CLIENT_ONCE = 4
} network_ssl_verify_t;

/**
 * @brief 网络配置结构体
 */
typedef struct {
    const char *host;
    int port;
    int timeout_ms;
    int read_timeout_ms;
    int write_timeout_ms;
    int max_retries;
    int retry_interval_ms;
    network_sock_type_t sock_type;
    network_af_t af;
    bool keepalive;
    bool nonblocking;
    bool ssl_enable;
    network_ssl_verify_t ssl_verify;
    const char *ssl_cert_path;
    const char *ssl_key_path;
    const char *ssl_ca_path;
} network_config_t;

/**
 * @brief 网络统计信息结构体
 */
typedef struct {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t connect_count;
    uint64_t error_count;
    uint64_t retry_count;
    uint64_t avg_latency_us;
} network_stats_t;

/**
 * @brief 网络连接句柄（不透明指针）
 */
typedef struct network_connection network_connection_t;

/**
 * @brief 连接池句柄（不透明指针）
 */
typedef struct network_pool network_pool_t;

/**
 * @brief HTTP 请求结构体
 */
typedef struct {
    const char *method;
    const char *path;
    const char *content_type; /**< Content-Type */
    const void *body;
    size_t body_len;
    const char **headers;
    size_t header_count;
    int timeout_ms;
    bool follow_redirects;
    int max_redirects;
} network_http_request_t;

/**
 * @brief HTTP 响应结构体
 */
typedef struct {
    int status_code;
    char *status_text;
    char **headers;
    size_t header_count;
    void *body;
    size_t body_len;
    airy_err_t error;
    char *error_message;
    uint64_t latency_us;
} network_http_response_t;

/**
 * @brief 网络事件回调类型
 */
typedef enum {
    NETWORK_EVENT_CONNECTED = 1,
    NETWORK_EVENT_DISCONNECTED = 2,
    NETWORK_EVENT_DATA_RECEIVED = 3,
    NETWORK_EVENT_DATA_SENT = 4,
    NETWORK_EVENT_ERROR = 5,
    NETWORK_EVENT_TIMEOUT = 6
} network_event_t;

/**
 * @brief 网络事件回调函数类型
 * @param connection 连接句柄
 * @param event 事件类型
 * @param data 事件数据
 * @param data_len 数据长度
 * @param user_data 用户数据
 */
typedef void (*network_event_callback_t)(network_connection_t *connection, network_event_t event,
                                         const void *data, size_t data_len, void *user_data);

/* ============================================================================
 * 基础连接 API
 * ============================================================================ */

/**
 * @brief 创建默认网络配置
 * @return 默认网络配置结构体
 */
network_config_t network_create_default_config(void);

/**
 * @brief 创建网络连接
 * @param config 网络配置
 * @return 连接句柄，失败返回 NULL
 * @ownership 调用者负责调用 network_connection_destroy 释放
 */
network_connection_t *network_connection_create(const network_config_t *config);

/**
 * @brief 销毁网络连接
 * @param connection 连接句柄
 */
void network_connection_destroy(network_connection_t *connection);

/**
 * @brief 建立网络连接
 * @param connection 连接句柄
 * @return 错误码
 */
airy_err_t network_connect(network_connection_t *connection);

/**
 * @brief 断开网络连接
 * @param connection 连接句柄
 * @return 错误码
 */
airy_err_t network_disconnect(network_connection_t *connection);

/**
 * @brief 发送数据
 * @param connection 连接句柄
 * @param data 数据缓冲区
 * @param length 数据长度
 * @param sent [out] 实际发送的字节数（可选）
 * @return 错误码
 */
airy_err_t network_send(network_connection_t *connection, const void *data, size_t length,
                        size_t *sent);

/**
 * @brief 接收数据
 * @param connection 连接句柄
 * @param buffer 接收缓冲区
 * @param length 缓冲区长度
 * @param received [out] 实际接收的字节数（可选）
 * @return 错误码
 */
airy_err_t network_receive(network_connection_t *connection, void *buffer, size_t length,
                           size_t *received);

/**
 * @brief 发送全部数据（循环发送直到完成）
 * @param connection 连接句柄
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 错误码
 */
airy_err_t network_send_all(network_connection_t *connection, const void *data, size_t length);

/**
 * @brief 接收指定长度的数据
 * @param connection 连接句柄
 * @param buffer 接收缓冲区
 * @param length 期望接收的长度
 * @return 错误码
 */
airy_err_t network_receive_exact(network_connection_t *connection, void *buffer, size_t length);

/**
 * @brief 获取连接状态
 * @param connection 连接句柄
 * @return 连接状态
 */
network_status_t network_get_status(const network_connection_t *connection);

/**
 * @brief 设置连接超时
 * @param connection 连接句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
airy_err_t network_set_timeout(network_connection_t *connection, int timeout_ms);

/**
 * @brief 设置读写超时
 * @param connection 连接句柄
 * @param read_timeout_ms 读取超时（毫秒）
 * @param write_timeout_ms 写入超时（毫秒）
 * @return 错误码
 */
airy_err_t network_set_rw_timeout(network_connection_t *connection, int read_timeout_ms,
                                  int write_timeout_ms);

/**
 * @brief 获取统计信息
 * @param connection 连接句柄
 * @param stats [out] 统计信息
 * @return 错误码
 */
airy_err_t network_get_stats(const network_connection_t *connection, network_stats_t *stats);

/**
 * @brief 重置统计信息
 * @param connection 连接句柄
 * @return 错误码
 */
airy_err_t network_reset_stats(network_connection_t *connection);

/**
 * @brief 设置事件回调
 * @param connection 连接句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
airy_err_t network_set_event_callback(network_connection_t *connection,
                                      network_event_callback_t callback, void *user_data);

/**
 * @brief 获取错误消息
 * @param connection 连接句柄
 * @return 错误消息字符串
 */
const char *network_get_error_message(const network_connection_t *connection);

/* ============================================================================
 * HTTP 客户端 API
 * ============================================================================ */

/**
 * @brief 执行 HTTP 请求
 * @param connection 连接句柄（必须已连接）
 * @param request HTTP 请求配置
 * @param response [out] HTTP 响应（调用者需调用 network_http_response_free 释放）
 * @return 错误码
 */
airy_err_t network_http_request(network_connection_t *connection,
                                const network_http_request_t *request,
                                network_http_response_t *response);

/**
 * @brief 执行 HTTP GET 请求
 * @param connection 连接句柄
 * @param path 请求路径
 * @param response [out] HTTP 响应
 * @return 错误码
 */
airy_err_t network_http_get(network_connection_t *connection, const char *path,
                            network_http_response_t *response);

/**
 * @brief 执行 HTTP POST 请求
 * @param connection 连接句柄
 * @param path 请求路径
 * @param content_type Content-Type
 * @param body 请求体
 * @param body_len 请求体长度
 * @param response [out] HTTP 响应
 * @return 错误码
 */
airy_err_t network_http_post(network_connection_t *connection, const char *path,
                             const char *content_type, const void *body, size_t body_len,
                             network_http_response_t *response);

/**
 * @brief 释放 HTTP 响应资源
 * @param response HTTP 响应结构体
 */
void network_http_response_free(network_http_response_t *response);

/* ============================================================================
 * 连接池 API
 * ============================================================================ */

/**
 * @brief 创建连接池
 * @param config 基础网络配置
 * @param pool_size 池大小
 * @return 连接池句柄，失败返回 NULL
 */
network_pool_t *network_pool_create(const network_config_t *config, size_t pool_size);

/**
 * @brief 销毁连接池
 * @param pool 连接池句柄
 */
void network_pool_destroy(network_pool_t *pool);

/**
 * @brief 从连接池获取连接
 * @param pool 连接池句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 连接句柄，失败返回 NULL
 * @note 获取的连接使用完毕后必须调用 network_pool_release_connection 释放
 */
network_connection_t *network_pool_acquire(network_pool_t *pool, int timeout_ms);

/**
 * @brief 释放连接回连接池
 * @param pool 连接池句柄
 * @param connection 连接句柄
 */
void network_pool_release(network_pool_t *pool, network_connection_t *connection);

/**
 * @brief 获取连接池可用连接数
 * @param pool 连接池句柄
 * @return 可用连接数
 */
size_t network_pool_available(const network_pool_t *pool);

/**
 * @brief 获取连接池总大小
 * @param pool 连接池句柄
 * @return 总大小
 */
size_t network_pool_size(const network_pool_t *pool);

/**
 * @brief 健康检查连接池
 * @param pool 连接池句柄
 * @return 健康连接数
 */
size_t network_pool_health_check(network_pool_t *pool);

/* ============================================================================
 * DNS 解析 API
 * ============================================================================ */

/**
 * @brief DNS 解析结果结构体
 */
typedef struct {
    char **addresses;
    size_t count;
    int *ports;
} network_dns_result_t;

/**
 * @brief 执行 DNS 解析
 * @param hostname 主机名
 * @param af 地址族
 * @param result [out] 解析结果
 * @return 错误码
 */
airy_err_t network_dns_resolve(const char *hostname, network_af_t af, network_dns_result_t *result);

/**
 * @brief 释放 DNS 解析结果
 * @param result 解析结果
 */
void network_dns_result_free(network_dns_result_t *result);

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief 检查主机是否可达
 * @param host 主机名或 IP
 * @param timeout_ms 超时时间（毫秒）
 * @return true 可达，false 不可达
 */
bool network_is_reachable(const char *host, int timeout_ms);

/**
 * @brief 获取本机 IP 地址
 * @param af 地址族
 * @param buffer 输出缓冲区
 * @param buffer_len 缓冲区长度
 * @return 错误码
 */
airy_err_t network_get_local_ip(network_af_t af, char *buffer, size_t buffer_len);

/**
 * @brief 将 IP 地址转换为字符串
 * @param af 地址族
 * @param addr 地址结构
 * @param buffer 输出缓冲区
 * @param buffer_len 缓冲区长度
 * @return 错误码
 */
airy_err_t network_addr_to_string(network_af_t af, const void *addr, char *buffer,
                                  size_t buffer_len);

/**
 * @brief 初始化网络子系统
 * @return 错误码
 * @note Windows 平台需要调用此函数初始化 Winsock
 */
airy_err_t network_init(void);

/**
 * @brief 清理网络子系统
 * @note Windows 平台需要调用此函数清理 Winsock
 */
void network_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_NETWORK_COMMON_H */
