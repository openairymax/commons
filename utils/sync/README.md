# Sync — 线程同步与并发执行原语

**模块路径**: `commons/utils/sync/`
**版本**: 0.1.19

## 概述

Sync 模块提供跨平台的线程同步与并发执行基础设施，包含四个 API 域：

- **核心层 `sync.h`**：互斥锁（递归形态经 `SYNC_FLAG_RECURSIVE` 创建）、
  读写锁、条件变量共三类同步原语，统一为不透明句柄 + `sync_result_t`
  返回值，阻塞接口普遍支持毫秒超时；附原子操作（CAS/加/减/读/写）与
  线程 ID 查询；
- **事件循环 `airy_event_loop.h`**：fd 可读/可写事件分发 + 周期定时器，
  按平台由 epoll（Linux）、kqueue（macOS/BSD）、WSAEventSelect（Windows）
  三个后端实现，同一平台仅一个后端参与链接；
- **线程池 `thread_pool.h`**：固定上下限的工作者线程池，任务队列 +
  优雅关闭（消费完队列后退出）；
- **取消令牌 `cancel_token.h`**：原子取消标志 + 条件变量阻塞等待 + 唤醒回调链，
  用于异步任务的可中断执行。

平台差异统一委托 `commons/platform` 母件（`platform_sync.h`）：POSIX 分支基于
pthread，Windows 分支基于 CriticalSection/SRWLOCK/CONDITION_VARIABLE；macOS
缺失的 `pthread_mutex_timedlock`、`pthread_rwlock_timed*` 由母件
`airy_mtx_timed` / `airy_rwlock_timed` 以 try + 短眠轮询近似超时。

## 目录结构

```
sync/
├── README.md
├── sync.h / sync.c                      # 核心层 API：线程工具/原子操作
├── sync_mutex.c                         # 互斥锁（含递归形态）
├── sync_rwlock.c                        # 读写锁
├── sync_condition.c                     # 条件变量
├── sync_types.h                         # 句柄内部布局（家族头 + 母件原语，勿直接依赖）
├── sync_internal.h / sync_internal.c    # 内部助手（errno 映射/句柄分配，不安装）
├── airy_event_loop.h / airy_event_loop.c            # 事件循环门面（stop/定时器委托）
├── airy_event_loop_epoll.c                          # Linux epoll 后端
├── airy_event_loop_kqueue.c                         # macOS/BSD kqueue 后端
├── airy_event_loop_win.c                            # Windows WSAEventSelect 后端
├── airy_event_loop_posix.c                          # 两 POSIX 后端共享机制（Windows 下为空）
├── airy_event_loop_internal.h                       # 门面与后端的内部契约（不安装）
├── airy_event_timer.h / airy_event_timer.c          # 平台无关定时器管理（内部）
├── thread_pool.h / thread_pool.c        # 线程池
└── cancel_token.h / cancel_token.c      # 取消令牌
```

## 核心层（`sync.h`）

### 枚举与类型

| 类型 | 取值 |
|------|------|
| `sync_result_t` | `SYNC_SUCCESS=0`、`SYNC_ERROR_TIMEOUT`、`SYNC_ERROR_DEADLOCK`、`SYNC_ERROR_INVALID`、`SYNC_ERROR_MEMORY`、`SYNC_ERROR_PERMISSION`、`SYNC_ERROR_BUSY`、`SYNC_ERROR_UNSUPPORTED`、`SYNC_ERROR_UNKNOWN` |
| `sync_flag_t` | `SYNC_FLAG_NONE=0`、`SYNC_FLAG_RECURSIVE=1<<4` |

| 结构 | 字段/说明 |
|------|-----------|
| `sync_timeout_t` | `{ uint64_t timeout_ms; bool absolute; }`；接口传 `NULL` 表示无限等待 |
| `sync_attr_t` | 创建属性 `{ uint32_t flags; }`，当前仅识别 `SYNC_FLAG_RECURSIVE` |

三类原语各有不透明句柄类型：`sync_mutex_t`、`sync_rwlock_t`、
`sync_condition_t`。句柄由 `*_create` 分配、由对应 `*_free` 释放。

### 线程工具

| 函数 | 说明 |
|------|------|
| `sync_get_thread_id()` | 当前线程 ID（Windows `GetCurrentThreadId` / POSIX `pthread_self`） |

### 三类原语接口

| 原语 | 函数 |
|------|------|
| 互斥锁 | `sync_mutex_create(&m, attr)` / `sync_mutex_free(m)` / `sync_mutex_lock_ex(m, timeout)` / `sync_mutex_try_lock(m)` / `sync_mutex_unlock_ex(m)`；`attr->flags & SYNC_FLAG_RECURSIVE` 时创建为递归互斥（POSIX 走 `PTHREAD_MUTEX_RECURSIVE`，Windows `CRITICAL_SECTION` 天然递归） |
| 读写锁 | `sync_rwlock_create/free` + `read_lock_ex` / `try_read_lock` / `write_lock_ex` / `try_write_lock` / `unlock_ex`（读共享、写独占，均支持超时） |
| 条件变量 | `sync_condition_create/free` + `wait_ex(cond, mutex, timeout)` / `signal_ex` / `broadcast_ex`；等待必须关联核心层 `sync_mutex_t` |

`timeout == NULL` 或 `timeout_ms == 0` 表示无限等待；超时值为相对毫秒时长，
按 `sync_timeout_t.absolute` 区分是否为绝对时刻。

### 原子操作

| 函数 | 语义 |
|------|------|
| `sync_atomic_cas(ptr, expected, desired)` | 比较并交换，成功返回 `true` |
| `sync_atomic_add(ptr, value)` / `sync_atomic_sub(ptr, value)` | 原子加/减，返回旧值 |
| `sync_atomic_load(ptr)` / `sync_atomic_store(ptr, value)` | 原子读/写 |

参数均为 `volatile void *` + `uintptr_t`，按指针位宽工作。Windows 使用
`_Interlocked*64`/`_Interlocked*`（依 `_WIN64` 选择），POSIX 使用
`__sync` 内建。

## 平台原语委托（`commons/platform`）

核心层三类原语的平台实现统一委托母件 `platform_sync.h`（`commons/platform`）：
`airy_mtx_t`、`airy_rwlock_t`、`airy_cond_t`。`sync_types.h` 仅保留句柄
结构体的内部布局（家族公共头 `sync_lock_hdr_t`：`initialized`，后接对应
母件原语），供本模块各 `.c` 共用，不属于稳定公共 API。

| 原语 | Linux/POSIX | macOS | Windows |
|------|-------------|-------|---------|
| 互斥锁（含递归） | `pthread_mutex_t` | 同左（timed 轮询补齐） | `CRITICAL_SECTION`（天然递归） |
| 读写锁 | `pthread_rwlock_t` | 同左（timed 轮询补齐） | `SRWLOCK` + 状态字 |
| 条件变量 | `pthread_cond_t` | 同左 | `CONDITION_VARIABLE` |

## 事件循环（`airy_event_loop.h`）

单线程 I/O 多路复用 + 定时器门面。核心文件 `airy_event_loop.c` 只承担与平台
无关的部分（`stop`、定时器委托），fd 注册与分发在三个后端文件中，各以预处理器
守卫启用：`airy_event_loop_epoll.c`（`!_WIN32 && __linux__`）、
`airy_event_loop_kqueue.c`（`!_WIN32 && !__linux__`）、
`airy_event_loop_win.c`（`_WIN32`，WSAEventSelect 订阅，每个 fd 绑定一个
WSAEVENT）。epoll 后端直接以 fd 值作注册表数组索引，要求 `fd < max_events`。

常量与类型：`AIRY_EVENT_LOOP_MAX_EVENTS`（1024，单次批量获取上限）、
`AIRY_EVENT_LOOP_MAX_TIMERS`（64）、事件类型位 `AIRY_EVENT_TYPE_READ=1`、
`AIRY_EVENT_TYPE_WRITE=2`、`AIRY_EVENT_TYPE_TIMER=4`、`AIRY_EVENT_TYPE_SIGNAL=8`；
回调签名 `airy_event_callback_t`（`int (*)(int fd, uint32_t events, void *ud)`）
与 `airy_timer_callback_t`（`void (*)(airy_event_loop_t *, uint64_t timer_id, void *ud)`）。

| 函数 | 说明 |
|------|------|
| `airy_event_loop_create(max_events)` / `destroy(loop)` | 创建/销毁循环实例（堆分配） |
| `airy_event_loop_add_fd(loop, fd, events, cb, ud)` | 注册 fd；**边沿触发**（epoll `EPOLLET`、kqueue `EV_CLEAR`） |
| `airy_event_loop_add_fd_lt(...)` | 同上，电平触发 |
| `airy_event_loop_mod_fd(loop, fd, events)` / `remove_fd(loop, fd)` | 修改关注事件/注销 |
| `airy_event_loop_add_timer(loop, interval_ms, cb, ud)` | 注册**周期性**定时器，返回单调递增 `timer_id` |
| `airy_event_loop_cancel_timer(loop, timer_id)` | 取消定时器 |
| `airy_event_loop_run(loop)` | 阻塞运行直至 `stop` |
| `airy_event_loop_stop(loop)` / `airy_event_loop_stop_async(loop)` | 停止；`_async` 仅做原子置位 + 唤醒，异步信号安全（可在信号处理器中调用） |
| `airy_event_loop_get_fd_count(loop)` | 当前注册 fd 数 |
| `airy_event_loop_wakeup(loop)` | 主动唤醒 `run`（eventfd / EVFILT_USER / 事件对象，按平台） |

定时器管理为平台无关实现（`airy_event_timer.c`，内部头不安装）：后端在每轮
poll/kevent/等待返回后调用 `airy_timer_process()` 刷新单调时钟并触发到期定时器。

## 线程池（`thread_pool.h`）

| 项 | 说明 |
|----|------|
| `thread_pool_config_t` | `{ min_threads, max_threads, queue_size, idle_timeout_ms }`；默认值（`thread_pool_get_default_config`）：2 / 8 / 256 / 30000 |
| `thread_pool_create(config)` | 按 `max_threads` 创建工作者线程；参数非法或超出资源上限返回 `NULL` |
| `thread_pool_submit(pool, task, arg)` | 投递任务 `void (*)(void *)`；0 成功；`AIRY_ERR_INVALID_PARAM`、`AIRY_ERR_OVERFLOW`（队列满）、`AIRY_ERR_OUT_OF_MEMORY`、`AIRY_ERR_UNKNOWN`（未运行/正在关闭） |
| `thread_pool_active_count` / `pending_count` | 在跑/排队任务数（池为空返回 0） |
| `thread_pool_is_running` | 是否处于运行状态 |
| `thread_pool_destroy(pool)` | 置停止标志并唤醒全部工作者：**消费完任务队列后**退出并 join |

## 取消令牌（`cancel_token.h`）

| 函数 | 说明 |
|------|------|
| `airy_cancel_token_init(token)` / `destroy(token)` | 调用方持有结构体；destroy 幂等、可传 `NULL` |
| `airy_cancel_token_cancel(token)` | 置原子取消标志并触发全部回调；幂等（回调只触发一次） |
| `airy_cancel_token_is_canceled(token)` | 无锁读原子标志 |
| `airy_cancel_token_wait(token, timeout_ms)` | 条件变量阻塞等待（非忙轮询）；返回 0 已取消、1 超时、负值参数错误；`timeout_ms=0` 无限等待 |
| `airy_cancel_token_register(token, cb, ctx)` | 追加唤醒回调（用于叫醒无法直接阻塞在令牌上的 I/O 等待），链上限 `AIRY_CANCEL_TOKEN_MAX_CBS`（8），超出返回 `AIRY_ERR_OVERFLOW` |
| `airy_cancel_token_reset(token)` | 复位为活动状态以便复用（不回调撤链） |

令牌内部使用 commons 平台层的 `airy_mtx_t` / `airy_cond_t` / `airy_atomic_int_t`。

## 用法示例

```c
#include "sync.h"

sync_mutex_t m = NULL;
sync_result_t rc = sync_mutex_create(&m, NULL);
if (rc == SYNC_SUCCESS) {
    /* 最多等 200 ms；timeout_ms 为相对毫秒 */
    sync_timeout_t to = { .timeout_ms = 200, .absolute = false };
    rc = sync_mutex_lock_ex(m, &to);
    if (rc == SYNC_SUCCESS) {
        /* 临界区 ... */
        sync_mutex_unlock_ex(m);
    }
    /* rc == SYNC_ERROR_TIMEOUT：未在期限内取得锁 */
    sync_mutex_free(m);
}
```

```c
#include "airy_event_loop.h"

/* 信号处理器中只允许调用 stop_async（异步信号安全） */
static airy_event_loop_t *g_loop;
static void on_signal(int sig)
{
    (void)sig;
    airy_event_loop_stop_async(g_loop);
}

int listen_fd; /* 已建立的非阻塞监听 fd */
g_loop = airy_event_loop_create(AIRY_EVENT_LOOP_MAX_EVENTS);
if (g_loop != NULL) {
    if (airy_event_loop_add_fd(g_loop, listen_fd, AIRY_EVENT_TYPE_READ,
                               on_readable, NULL /* ud */) == 0) {
        uint64_t tid = airy_event_loop_add_timer(g_loop, 1000, on_tick, NULL);
        (void)tid; /* 每 1000 ms 触发一次，直至 cancel_timer */
        airy_event_loop_run(g_loop); /* 阻塞直至 stop/stop_async */
    }
    airy_event_loop_destroy(g_loop);
}
```

## 构建与依赖

本模块 13 个 `.c` 全部编入静态库 `airy_common`（三后端文件全平台参与构建，
由预处理器守卫决定实际生效者），头文件目录经 PUBLIC 导出；安装时排除
`sync_internal.h` 与 `airy_event_loop_internal.h`（`*_internal.h` 规则）。

| 依赖 | 来源 | 用途 |
|------|------|------|
| `airy_memory.h` | [`utils/memory`](../memory/README.md) | 句柄/任务节点的堆分配（`AIRY_CALLOC`/`AIRY_FREE`） |
| `error.h` | [`utils/error`](../error/README.md) | `AIRY_ERR_*` 返回码（线程池、取消令牌） |
| `logging.h` / `svc_logger.h` | [`utils/observability`](../observability/README.md) | 核心层与线程池日志、事件循环后端日志 |
| `platform.h` | [`commons/platform`](../../platform/README.md) | 取消令牌与线程池的线程/锁/原子原语 |
| `atomic_compat.h` | [`utils/compat`](../compat/README.md) | Windows/macOS 分支的 C11 原子支持 |

不依赖任何外部并发库；POSIX 分支链接 pthread。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
