/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/*
 * @file platform_gpu.h
 * @brief 加速器能力探测契约（0.1.21 B-0 三层探测的机制面）
 *
 * 机制与策略分离：本文件只回答「这台机器有没有可用加速器、哪一层走得通」，
 * 不决定「用哪个后端、怎么调度」——后者属 daemons/providers 的策略面。
 *
 * 三层探测口径（§17.2 B-0 做法段）：
 *   L1 硬件层：从 sysfs/drm 读真实 PCI 加速器，虚拟设备（vgem/vkms）不计入；
 *   L2 运行时层：已配置加速后端端点且可应答；
 *   L3 OpenAI 兼容层：端点返回 /v1/models 的 JSON 名单。
 *
 * 三值判定（§12.3）：R1/R2 为「有可用加速路径」，R3 为「无」；
 * 平台无探测后端时同样落 R3 并在 reason 中显式写明「不适用」，不得静默返回成功。
 *
 * @see platform_misc.h airy_get_gpu_info（展示用字符串，与本契约并存）
 */

#ifndef AIRY_RT_PLATFORM_GPU_H
#define AIRY_RT_PLATFORM_GPU_H

#include "platform_base.h"
#include "types.h"


#ifdef __cplusplus
extern "C" {
#endif


/* ==================== Types ==================== */

/** 加速器种类；VIRTUAL 表示非真实加速器（vgem/vkms/virtio 等），必须排除在能力之外。 */
typedef enum {
    AIRY_ACCEL_NONE = 0,
    AIRY_ACCEL_NVIDIA,
    AIRY_ACCEL_AMD,
    AIRY_ACCEL_INTEL,
    AIRY_ACCEL_OTHER,
    AIRY_ACCEL_VIRTUAL
} airy_accel_kind_t;

/** 探测结论：R1 原生 GPU 路径 / R2 兼容加速路径 / R3 无可用加速器。 */
typedef enum {
    AIRY_GPU_R_NONE = 0,
    AIRY_GPU_R1,
    AIRY_GPU_R2,
    AIRY_GPU_R3
} airy_gpu_verdict_t;

/** 已打通的探测层位标（bitmask，供上层判断可宣称到哪一步）。 */
#define AIRY_GPU_L1_HW      0x1u
#define AIRY_GPU_L2_RUNTIME 0x2u
#define AIRY_GPU_L3_OPENAI  0x4u

#define AIRY_GPU_MAX_DEVS 8u

/** 单块加速器。model 为空时上层按 kind+driver 拼展示名。 */
typedef struct {
    airy_accel_kind_t kind;
    uint16_t vendor_id;
    uint16_t device_id;
    char driver[32];
    char model[128];
} airy_gpu_dev_t;

/** 整机探测结果。reason 在 R3 时必填，供 doctor 与发行说明直接引用。 */
typedef struct {
    uint32_t dev_count;
    airy_gpu_dev_t devs[AIRY_GPU_MAX_DEVS];
    uint32_t layers;
    airy_gpu_verdict_t verdict;
    char backend[64];
    char reason[192];
} airy_gpu_cap_t;


/* ==================== API ==================== */

/**
 * @brief 按 PCI 厂商号与内核驱动名归类加速器。
 *
 * vgem/vkms/virtio_gpu 一律归 VIRTUAL（虚拟设备不得计入算力）；
 * 已知厂商号：0x10de NVIDIA / 0x1002 AMD / 0x8086 Intel，其余 OTHER。
 *
 * @param vendor [in] PCI vendor id（0 表示未知）
 * @param driver [in] 内核驱动名，可为 NULL
 * @return 加速器种类
 */
AIRY_API airy_accel_kind_t airy_gpu_class(uint16_t vendor, const char *driver);

/**
 * @brief 对整机执行三层探测并给出结论。
 *
 * 无副作用、可重复调用：同一环境下两次结果一致。探测失败不是错误，
 * 一律以 R3 + reason 表达；仅参数非法返回 AIRY_EINVAL。
 *
 * @param cap [out] 探测结果（调用方分配）
 * @return AIRY_SUCCESS / AIRY_EINVAL
 */
AIRY_API int airy_gpu_probe(airy_gpu_cap_t *cap);


#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_PLATFORM_GPU_H */
