/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file hall_event.h
 * @brief 任务大厅事件磁盘格式唯一真相源（SSoT）：写侧机制件。
 *
 * 事件文件的路径推导、命名、envelope 布局、gseq 续接、原子写与写后回读
 * 断言唯一实现在本文件。runtime（atoms/coreloopthree）、gateway、daemons
 * 三方凡是写 hall 事件一律委托本文件，不得自研轮子或复刻格式。
 *
 * 磁盘契约：
 *   root      airy_data_dir()/agentrt/hall
 *   layout    {tenant}/{task}/{category}/{tenant}.{task}.{category}.{ts}.{seq:04u}.json
 *   ts_utc    YYYYMMDDThhmmssmmm（UTC，定宽，可直接字典序比较）
 *   seq       (task, category) 目录内 max(seq)+1，跨写者进程不撞号
 *   gseq      进程内单调递增；首次写入续接到磁盘最大 gseq，跨写者全局不撞号
 *   prev_file 同 (task, category) 目录内 max(seq) 事件，决策链可仅凭磁盘重建
 *   body      {"file":{...},"access":{...},"content":{...}}
 *
 * 依据：0.1.19 架构改进方案 §4.3（机制件收敛至 commons）、L716。
 */

#ifndef AIRY_RT_HALL_EVENT_H
#define AIRY_RT_HALL_EVENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 事件根目录（相对 airy_data_dir()）；全部读写方共用此常量 */
#define HALL_EVT_ROOT_REL "agentrt/hall"
/* 事件文件 id 上限（tenant.task.category.ts.seq.json） */
#define HALL_EVT_ID_MAX   192
/* ts_utc 缓冲长度（"YYYYMMDDThhmmssmmm" + NUL） */
#define HALL_EVT_TS_LEN   24
/* envelope 头部（file+access 两段）硬上限；超限 fail-closed，不静默截断 */
#define HALL_EVT_HDR_MAX  1024
/* 路径缓冲长度 */
#define HALL_EVT_PATH_MAX 1024

/**
 * @brief 一条 hall 事件的构造输入（全部字段为借用，不接管所有权）。
 */
typedef struct {
    const char *tenant;   /* 租户；NULL/空 → "default" */
    const char *task;     /* 任务分组键（必填） */
    const char *category; /* 类别名（必填，见任务文件模型七类） */
    const char *node;     /* blueprint 节点 id；NULL → "" */
    const char *owner;    /* owner_role；NULL/空 → "cognition" */
    const char *ts_utc;   /* 时间戳（必填，hall_clock_utc() 产出） */
    const char *content;  /* content 段 JSON 对象串（必填） */
    const char *prev;     /* 决策链前驱文件 id；NULL → "" */
    unsigned seq;
    unsigned long long gseq;
} hall_evt_t;

/**
 * @brief 事件文件名分量视图：全部为指向入参 name 的借用指针，非 NUL 结尾。
 */
typedef struct {
    const char *tenant;
    size_t tenant_len;
    const char *task;
    size_t task_len;
    const char *category;
    size_t category_len;
    const char *ts_utc;
    size_t ts_utc_len;
    const char *seq;
    size_t seq_len;
} hall_evt_parts_t;

/**
 * @brief 校验事件分量（task/category）不含路径穿越与危险字符。
 * @return 1 合法，0 非法
 */
int hall_comp_valid(const char *s);

/**
 * @brief 解析事件文件名 {tenant}.{task}.{category}.{ts_utc}.{seq:04u}.json。
 *
 * 读侧（atoms 索引重建、gateway 事件流）唯一解析实现，禁止各自复刻分段
 * 规则（BAN-154：不使用 sscanf，按 '.' 手工切分）。
 *
 * @return 0 成功；-1 名字非法（段数/后缀/空段不符）
 */
int hall_evt_parse(const char *name, hall_evt_parts_t *out);

/**
 * @brief 取事件文件名的 seq 段数值（非十进制数字返回 0）。
 */
unsigned long hall_evt_seq(const char *name);

/**
 * @brief 生成 UTC 时间戳 "YYYYMMDDThhmmssmmm"（定宽 18 字符）。
 */
void hall_clock_utc(char *buf, size_t sz);

/**
 * @brief 按 category 返回 write_roles 策略串（cognition-only / 含 executor）。
 */
const char *hall_roles_of(const char *category);

/**
 * @brief 组装事件文件名 {tenant}.{task}.{category}.{ts}.{seq:04u}.json。
 * @return 0 成功；-1 参数非法或输出被截断（fail-closed）
 */
int hall_evt_name(char *out, size_t sz, const hall_evt_t *evt);

/**
 * @brief 组装完整事件 envelope（含 content 段与收尾花括号）。
 * @param out_sz 建议 >= strlen(evt->content) + HALL_EVT_HDR_MAX
 * @return 0 成功；-1 参数非法或输出被截断（fail-closed）
 */
int hall_evt_build(const hall_evt_t *evt, char *out, size_t out_sz);

/**
 * @brief 写入一条 hall 事件（完整写路径）。
 *
 * 内部完成：路径穿越校验 → gseq 续接与递增 → 目录递归创建 → seq 续接与
 * prev_file 解析 → envelope 组装 → 原子写（tmp+fsync+rename）→ debug
 * 构建下写后回读断言。
 *
 * @return 0 成功；-1 失败（best-effort 契约，调用方不因此中断自身流程）
 */
int hall_evt_write(const char *task_id, const char *category, const char *node_id,
                   const char *content_json);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_HALL_EVENT_H */
