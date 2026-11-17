// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_platform_gpu.c
 * @brief 加速器能力探测（0.1.21 B-0）单元测试
 *
 * 覆盖三件事：厂商/驱动归类的正确性（含虚拟设备排除）、探测契约的
 * 三值完备性（R1/R2 必须有 backend，R3 必须有 reason）、以及可重复性。
 */

#include <stdio.h>
#include <string.h>

#include "platform_gpu.h"

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("Running %s...\n", #test_func);                 \
        if (test_func() != 0) {                                \
            fprintf(stderr, "✗Test failed: %s\n", #test_func); \
            failed_tests++;                                    \
        } else {                                               \
            printf("✔PASS: %s\n", #test_func);                 \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

/* 厂商号归类：四类真实厂商 + 未知厂商 + 零厂商号。 */
static int test_class_vendor(void)
{
    TEST_ASSERT(airy_gpu_class(0x10de, "nvidia") == AIRY_ACCEL_NVIDIA, "NVIDIA 归类");
    TEST_ASSERT(airy_gpu_class(0x1002, "amdgpu") == AIRY_ACCEL_AMD, "AMD 归类");
    TEST_ASSERT(airy_gpu_class(0x8086, "i915") == AIRY_ACCEL_INTEL, "Intel 归类");
    TEST_ASSERT(airy_gpu_class(0x8086, "xe") == AIRY_ACCEL_INTEL, "Intel xe 归类");
    TEST_ASSERT(airy_gpu_class(0x1234, "foo") == AIRY_ACCEL_OTHER, "未知厂商归类");
    TEST_ASSERT(airy_gpu_class(0x0000, "") == AIRY_ACCEL_NONE, "零厂商号归类");
    return 0;
}

/* 虚拟设备必须排除：同厂商号但驱动为 vgem/vkms/virtio 时不得计入算力。 */
static int test_class_virtual(void)
{
    TEST_ASSERT(airy_gpu_class(0x8086, "vgem") == AIRY_ACCEL_VIRTUAL, "vgem 排除");
    TEST_ASSERT(airy_gpu_class(0x8086, "vkms") == AIRY_ACCEL_VIRTUAL, "vkms 排除");
    TEST_ASSERT(airy_gpu_class(0x1af4, "virtio_gpu") == AIRY_ACCEL_VIRTUAL, "virtio 排除");
    TEST_ASSERT(airy_gpu_class(0x10de, "virtio-gpu") == AIRY_ACCEL_VIRTUAL, "连字符变体排除");
    TEST_ASSERT(airy_gpu_class(0x10de, NULL) == AIRY_ACCEL_NVIDIA, "空驱动不误判为虚拟");
    return 0;
}

/* 参数契约：空指针必须拒绝，且不得写入。 */
static int test_probe_einval(void)
{
    TEST_ASSERT(airy_gpu_probe(NULL) == AIRY_EINVAL, "空指针返回 EINVAL");
    return 0;
}

/* 三值完备性：任何平台都必须给出可展示的结论，不得静默空成功。 */
static int test_probe_tristate(void)
{
    airy_gpu_cap_t cap;

    TEST_ASSERT(airy_gpu_probe(&cap) == AIRY_SUCCESS, "探测成功返回");
    TEST_ASSERT(cap.verdict == AIRY_GPU_R1 || cap.verdict == AIRY_GPU_R2 ||
                    cap.verdict == AIRY_GPU_R3,
                "结论落在三值内");
    TEST_ASSERT(cap.dev_count <= AIRY_GPU_MAX_DEVS, "设备数不越界");
    if (cap.verdict == AIRY_GPU_R3)
        TEST_ASSERT(cap.reason[0] != '\0', "R3 必须写明原因");
    else
        TEST_ASSERT(cap.backend[0] != '\0', "R1/R2 必须写明后端");
    if (cap.dev_count > 0)
        TEST_ASSERT(cap.layers & AIRY_GPU_L1_HW, "有设备即打通 L1");
    return 0;
}

/* 可重复性：同一环境连续两次结论必须一致（无随机、无残留状态）。 */
static int test_probe_repeat(void)
{
    airy_gpu_cap_t a;
    airy_gpu_cap_t b;

    TEST_ASSERT(airy_gpu_probe(&a) == AIRY_SUCCESS, "首次探测");
    TEST_ASSERT(airy_gpu_probe(&b) == AIRY_SUCCESS, "二次探测");
    TEST_ASSERT(a.verdict == b.verdict, "结论可重复");
    TEST_ASSERT(a.dev_count == b.dev_count, "设备数可重复");
    TEST_ASSERT(strcmp(a.reason, b.reason) == 0, "原因串可重复");
    return 0;
}

int main(void)
{
    TEST_RUN(test_class_vendor);
    TEST_RUN(test_class_virtual);
    TEST_RUN(test_probe_einval);
    TEST_RUN(test_probe_tristate);
    TEST_RUN(test_probe_repeat);

    printf("\n%d passed, %d failed\n", passed_tests, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
