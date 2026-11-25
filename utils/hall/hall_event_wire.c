// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file hall_event_wire.c
 * @brief 任务大厅读侧线上事件形态：磁盘 envelope → 扁平事件（SSoT）。
 *
 * 写侧（hall_event.c）只落磁盘 envelope；对外读形态由本件单点定义，
 * hall.replay / hall.stream / hall.watch 三路共用同一字段映射，避免各
 * 读端点各写一份而漂移。cJSON 依赖收敛在本件：写侧对象保持无 cJSON
 * 即可编译，故本件按 AIRY_HAS_CJSON 条件入编译单元列表。
 */

#include "hall_event.h"

#include "airy_memory.h"
#include "io.h"

#include <cjson/cJSON.h>

static void flatten_put_str(cJSON *evt, cJSON *file, const char *key, const char *out_key)
{
    const char *v = file ? cJSON_GetStringValue(cJSON_GetObjectItem(file, key)) : NULL;
    cJSON_AddStringToObject(evt, out_key, v ? v : "");
}

static void flatten_put_num(cJSON *evt, cJSON *file, const char *key)
{
    double v = file ? cJSON_GetNumberValue(cJSON_GetObjectItem(file, key)) : 0;
    cJSON_AddNumberToObject(evt, key, v);
}

char *hall_evt_flat_file(const char *path)
{
    char *raw = NULL;

    if (!path || !path[0])
        return NULL;
    if (airy_io_read_file(path, &raw, NULL) != 0)
        return NULL;
    cJSON *o = cJSON_Parse(raw);
    AIRY_FREE(raw);
    if (!o)
        return NULL;

    cJSON *f = cJSON_GetObjectItem(o, "file");
    cJSON *content = cJSON_GetObjectItem(o, "content");
    cJSON *evt = cJSON_CreateObject();
    if (evt) {
        flatten_put_str(evt, f, "id", "file_id");
        flatten_put_str(evt, f, "category", "category");
        flatten_put_str(evt, f, "task_id", "task_id");
        flatten_put_str(evt, f, "tenant_id", "tenant_id");
        flatten_put_str(evt, f, "node_id", "node_id");
        flatten_put_str(evt, f, "ts_utc", "ts_utc");
        flatten_put_num(evt, f, "seq");
        flatten_put_num(evt, f, "gseq");
        cJSON_AddItemToObject(evt, "content",
                              (content && cJSON_IsObject(content))
                                  ? cJSON_Duplicate(content, 1)
                                  : cJSON_CreateObject());
    }
    cJSON_Delete(o);
    if (!evt)
        return NULL;
    char *s = cJSON_PrintUnformatted(evt);
    cJSON_Delete(evt);
    return s;
}
