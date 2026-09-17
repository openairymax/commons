/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file agent_vocab.h
 * @brief 执行体角色词汇表：角色名、别名与只读集合的唯一权威定义。
 *
 * 与 ecosystem/agents/registry/agents.yaml 的 role 字段一一对应；编排侧
 * 经 A-IPC 的 vocab 方法运行时取值，C 侧统一经本模块归一化。全仓 C 源码
 * 的角色字面量只允许存在于本实现内（发布门禁守护）。
 */

#ifndef AGENT_VOCAB_H
#define AGENT_VOCAB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 兜底执行体：未知角色归一化至此，保证计划总能被驱动、不因角色失配中断。 */
#define AGENT_VOCAB_FALLBACK "coding"

/* 具体角色名常量：调用点做角色判定/赋值时引用本组宏，不得再写字面量。
 * 与 agent_vocab.c 的 VOCAB_ROLES 表一一对应。 */
#define AGENT_VOCAB_ROLE_PRODUCT_MANAGER "product_manager"
#define AGENT_VOCAB_ROLE_ARCHITECT "architect"
#define AGENT_VOCAB_ROLE_BACKEND "backend"
#define AGENT_VOCAB_ROLE_FRONTEND "frontend"
#define AGENT_VOCAB_ROLE_DEVOPS "devops"
#define AGENT_VOCAB_ROLE_SECURITY "security"
#define AGENT_VOCAB_ROLE_TESTER "tester"
#define AGENT_VOCAB_ROLE_CODING "coding"
#define AGENT_VOCAB_ROLE_DATA_ENGINEER "data_engineer"
#define AGENT_VOCAB_ROLE_REVIEWER "reviewer"
#define AGENT_VOCAB_ROLE_ANALYST "analyst"

/* 严格解析：仅命中具体角色或别名时返回归一化结果，未登记角色返回 NULL。
 * 安全判定（权限授予/只读隔离）必须用本函数，不得用 canonical 的兜底语义。 */
const char *agent_vocab_resolve(const char *role);

/* 归一化：别名 → 具体角色；未登记角色 → 兜底。仅在「兜底可接受」的驱动路径使用。
 * 返回值指向静态存储，调用方无需释放；role 为空/空串时返回兜底。 */
const char *agent_vocab_canonical(const char *role);

/* 归一化后是否只读角色（先 canonical 再判定，直接接受抽象别名输入）。 */
int agent_vocab_is_readonly(const char *role);

/* 具体执行体角色遍历。 */
size_t agent_vocab_role_count(void);
const char *agent_vocab_role_at(size_t idx);

/* 别名条目遍历：*role 返回归一化目标。 */
size_t agent_vocab_alias_count(void);
const char *agent_vocab_alias_at(size_t idx, const char **role);

/* 只读角色遍历（归一化后的具体角色集合）。 */
size_t agent_vocab_readonly_count(void);
const char *agent_vocab_readonly_at(size_t idx);

#ifdef __cplusplus
}
#endif

#endif /* AGENT_VOCAB_H */
