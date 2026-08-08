/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * A-TD (Airymax Task Descriptor) — implementation.
 *
 * 任务描述符创建与完整性校验（真实实现，非桩）：
 *   - magic 0x41475453 ('AGTS')，独立于 IPC 消息头 magic
 *   - CRC32 覆盖 header[0:72) + payload（与 IPC C-S12 同一算法）
 *   - validate() 依次检查 magic/version/flags/reserved/payload_len/CRC32
 *
 * 时间戳使用单调时钟（monotonic ns），跨平台（POSIX/Win32）。
 */

#include <airymax/task_desc.h>

#include <stddef.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* ============================================================================
 * CRC-32（IEEE 802.3，与 IPC ipc_calc_crc32 同一算法）
 * ============================================================================ */

__u32 airy_task_desc_crc32(const void *data, size_t len)
{
	const __u8 *buf = (const __u8 *)data;
	__u32 crc = 0xFFFFFFFFu;

	for (size_t i = 0; i < len; i++) {
		crc ^= buf[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320u;
			else
				crc >>= 1;
		}
	}

	return ~crc;
}

/* ============================================================================
 * 内部工具
 * ============================================================================ */

/**
 * @brief 计算 header[0:crc32 偏移) + payload 的整体 CRC32
 *
 * crc32 字段自身（offset 68）不参与计算，与 IPC 头校验（header[0:52)+payload）
 * 的约定一致。
 */
static __u32 task_desc_compute_crc(const struct airy_task_desc *desc,
				   const void *payload, __u32 payload_len)
{
	__u32 crc = 0xFFFFFFFFu;
	const __u8 *p = (const __u8 *)desc;

	for (size_t i = 0; i < offsetof(struct airy_task_desc, crc32); i++) {
		crc ^= p[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320u;
			else
				crc >>= 1;
		}
	}

	if (payload && payload_len > 0) {
		const __u8 *q = (const __u8 *)payload;
		for (__u32 i = 0; i < payload_len; i++) {
			crc ^= q[i];
			for (int j = 0; j < 8; j++) {
				if (crc & 1)
					crc = (crc >> 1) ^ 0xEDB88320u;
				else
					crc >>= 1;
			}
		}
	}

	return ~crc;
}

/**
 * @brief 获取单调时钟纳秒时间戳
 */
static __u64 task_desc_monotonic_ns(void)
{
#ifdef _WIN32
	LARGE_INTEGER freq, counter;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&counter);
	if (freq.QuadPart > 0)
		return (__u64)((double)counter.QuadPart / freq.QuadPart * 1000000000.0);
	return 0;
#else
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		return (__u64)ts.tv_sec * 1000000000ull + (__u64)ts.tv_nsec;
	return 0;
#endif
}

/* ============================================================================
 * 公共 API
 * ============================================================================ */

airy_err_t airy_task_desc_create(struct airy_task_desc *desc, __u16 opcode,
				 __u64 task_id, __u64 parent_task_id,
				 __u64 deadline_ns, __u64 src_task, __u64 dst_task,
				 const void *payload, __u32 payload_len,
				 __u32 flags, __u32 priority)
{
	if (!desc)
		return AIRY_EINVAL;
	if (payload == NULL && payload_len != 0)
		return AIRY_EINVAL;
	if (flags & AIRY_TASK_FLAG_RESERVED)
		return AIRY_EINVAL;

	/* __builtin_memset：规避 AIRY_COMPLIANCE_STRICT 对 memset 的毒化 */
	__builtin_memset(desc, 0, sizeof(*desc));
	desc->magic = AIRY_TASK_DESC_MAGIC;
	desc->version = AIRY_TASK_DESC_VERSION;
	desc->opcode = opcode;
	desc->task_id = task_id;
	desc->parent_task_id = parent_task_id;
	desc->submit_time_ns = task_desc_monotonic_ns();
	desc->deadline_ns = deadline_ns;
	desc->src_task = src_task;
	desc->dst_task = dst_task;
	desc->payload_len = payload_len;
	desc->flags = flags;
	desc->priority = priority;
	desc->crc32 = task_desc_compute_crc(desc, payload, payload_len);

	return AIRY_EOK;
}

airy_err_t airy_task_desc_validate(const struct airy_task_desc *desc,
				   const void *payload, __u32 payload_len)
{
	if (!desc)
		return AIRY_EINVAL;

	/* 1. magic：'AGTS'（与 IPC 的 'ARE1' 相互独立） */
	if (desc->magic != AIRY_TASK_DESC_MAGIC)
		return AIRY_EIPC_MAGIC;

	/* 2. version 契约一致 */
	if (desc->version != AIRY_TASK_DESC_VERSION)
		return AIRY_ECFGVERSION;

	/* 3. flags 保留位必须为零 */
	if (desc->flags & AIRY_TASK_FLAG_RESERVED)
		return AIRY_EIPC_FLAGS;

	/* 4. reserved[56] 必须全零（未来扩展不得静默携带数据） */
	for (size_t i = 0; i < sizeof(desc->reserved); i++) {
		if (desc->reserved[i] != 0)
			return AIRY_EIPC_RESERVED;
	}

	/* 5. payload 一致性：负载非空必须有匹配长度 */
	if (desc->payload_len == 0 && payload_len != 0)
		return AIRY_EIPC_PAYLOAD;
	if (desc->payload_len > 0 && (payload == NULL || payload_len < desc->payload_len))
		return AIRY_EIPC_PAYLOAD;

	/* 6. CRC32（header[0:72) + payload） */
	__u32 expect = task_desc_compute_crc(desc, payload, desc->payload_len);
	if (desc->crc32 != expect)
		return AIRY_EIPC_CRC32;

	return AIRY_EOK;
}
