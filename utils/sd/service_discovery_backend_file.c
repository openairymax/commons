// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file service_discovery_backend_file.c
 * @brief Cross-process service-discovery file backend: writes service
 *        registrations to JSON files under $AIRY_HOME/state/sd/; list/lookup
 *        scan the directory, implementing a cross-process persistent
 *        registry.
 */

#include "service_discovery_internal.h"
#include "io.h"

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>

#if AIRY_PLATFORM_POSIX
#include <dirent.h>
#endif

static void sd_safe_filename(const char *name, char *out, size_t out_sz)
{
    size_t i = 0;
    while (*name && i + 1 < out_sz) {
        char c = *name++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_') {
            out[i++] = c;
        } else {
            out[i++] = '_';
        }
    }
    out[i] = '\0';
}

static void sd_file_dir(char *buf, size_t size)
{
    const char *home = airy_home_dir();
    if (!home)
        snprintf(buf, size, "state/sd");
    else
        snprintf(buf, size, "%s/state/sd", home);
}

static void sd_file_path(const char *name, char *buf, size_t size)
{
    char dir[512], safe[SD_MAX_NAME_LEN];
    sd_file_dir(dir, sizeof(dir));
    sd_safe_filename(name, safe, sizeof(safe));
    snprintf(buf, size, "%s/%s.json", dir, safe);
}

static void sd_file_name_from_path(const char *path, char *out, size_t out_sz)
{
    size_t i = 0;
    const char *p = path;
    while (*p && *p != '.' && i + 1 < out_sz)
        out[i++] = *p++;
    out[i] = '\0';
}

static airy_err_t sd_file_write_service(const sd_service_entry_t *entry)
{
    char dir[512], path[512];
    sd_file_dir(dir, sizeof(dir));
    if (airy_mkdir_p(dir) != 0)
        return AIRY_ERR_SYS_FILE;
    sd_file_path(entry->name, path, sizeof(path));

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return AIRY_ENOMEM;
    cJSON_AddStringToObject(root, "name", entry->name);
    cJSON_AddStringToObject(root, "version", entry->version);
    cJSON_AddStringToObject(root, "service_type", entry->service_type);
    cJSON_AddStringToObject(root, "tags", entry->tags);
    cJSON_AddStringToObject(root, "dependencies", entry->dependencies);
    cJSON_AddNumberToObject(root, "capabilities", (double)entry->capabilities);
    cJSON_AddBoolToObject(root, "active", entry->active);
    cJSON_AddNumberToObject(root, "last_updated", (double)entry->last_updated);

    cJSON *insts = cJSON_AddArrayToObject(root, "instances");
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        const sd_instance_t *in = &entry->instances[i];
        cJSON *o = cJSON_CreateObject();
        if (!o)
            continue;
        cJSON_AddStringToObject(o, "instance_id", in->instance_id);
        cJSON_AddStringToObject(o, "endpoint", in->endpoint);
        cJSON_AddNumberToObject(o, "state", (double)in->state);
        cJSON_AddBoolToObject(o, "healthy", in->healthy);
        cJSON_AddNumberToObject(o, "weight", (double)in->weight);
        cJSON_AddNumberToObject(o, "active_connections", (double)in->active_connections);
        cJSON_AddNumberToObject(o, "max_connections", (double)in->max_connections);
        cJSON_AddNumberToObject(o, "last_heartbeat", (double)in->last_heartbeat);
        cJSON_AddNumberToObject(o, "register_time", (double)in->register_time);
        cJSON_AddNumberToObject(o, "pid", (double)in->pid);
        cJSON_AddItemToArray(insts, o);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json)
        return AIRY_ENOMEM;

    char tmp[544];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) {
        AIRY_FREE(json);
        return AIRY_ERR_SYS_FILE;
    }
    size_t len = strlen(json);
    size_t written = fwrite(json, 1, len, f);
    int rc = fclose(f);
    AIRY_FREE(json);
    if (written != len || rc != 0)
        return AIRY_ERR_SYS_FILE;
    if (airy_file_rename(tmp, path) != 0)
        return AIRY_ERR_SYS_FILE;
    return AIRY_SUCCESS;
}

/* 注册文件读侧字段原语：21 个字段此前各写一遍「取键—类型校验—写入」三段，
 * 类型判断抄错只会影响单个字段且难以察觉。四个 rd_* 按目标宽度分相，
 * 调用点因此没有分支。字段缺失或类型不符时保持调用方预置的零值。 */
static void rd_str(cJSON *o, const char *key, char *dst, size_t cap)
{
    const char *s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(o, key));

    if (s)
        safe_strcpy(dst, s, cap);
}

static void rd_u32(cJSON *o, const char *key, uint32_t *dst)
{
    cJSON *it = cJSON_GetObjectItemCaseSensitive(o, key);

    if (it && cJSON_IsNumber(it))
        *dst = (uint32_t)it->valuedouble;
}

static void rd_u64(cJSON *o, const char *key, uint64_t *dst)
{
    cJSON *it = cJSON_GetObjectItemCaseSensitive(o, key);

    if (it && cJSON_IsNumber(it))
        *dst = (uint64_t)it->valuedouble;
}

static void rd_bool(cJSON *o, const char *key, bool *dst)
{
    cJSON *it = cJSON_GetObjectItemCaseSensitive(o, key);

    if (it && cJSON_IsBool(it))
        *dst = cJSON_IsTrue(it);
}

static void rd_state(cJSON *o, const char *key, airy_svc_state_t *dst)
{
    cJSON *it = cJSON_GetObjectItemCaseSensitive(o, key);

    if (it && cJSON_IsNumber(it))
        *dst = (airy_svc_state_t)it->valuedouble;
}

static void rd_inst(sd_instance_t *in, cJSON *o)
{
    rd_str(o, "instance_id", in->instance_id, SD_MAX_NAME_LEN);
    rd_str(o, "endpoint", in->endpoint, SD_MAX_ENDPOINT_LEN);
    rd_state(o, "state", &in->state);
    rd_bool(o, "healthy", &in->healthy);
    rd_u32(o, "weight", &in->weight);
    rd_u32(o, "active_connections", &in->active_connections);
    rd_u32(o, "max_connections", &in->max_connections);
    rd_u64(o, "last_heartbeat", &in->last_heartbeat);
    rd_u64(o, "register_time", &in->register_time);
    rd_u32(o, "pid", &in->pid);
}

static airy_err_t sd_file_read_service(const char *name, sd_service_entry_t *out)
{
    char path[512];
    char *buf = NULL;
    cJSON *root;
    cJSON *insts;
    airy_err_t rc;
    uint32_t idx = 0;

    sd_file_path(name, path, sizeof(path));
    rc = (airy_err_t)airy_io_read_file(path, &buf, NULL);
    if (rc != AIRY_SUCCESS)
        return rc;
    root = cJSON_Parse(buf);
    AIRY_FREE(buf);
    if (!root)
        return AIRY_ERR_PARSE_ERROR;

    __builtin_memset(out, 0, sizeof(*out));
    rd_str(root, "name", out->name, SD_MAX_NAME_LEN);
    rd_str(root, "version", out->version, sizeof(out->version));
    rd_str(root, "service_type", out->service_type, SD_MAX_TYPE_LEN);
    rd_str(root, "tags", out->tags, SD_MAX_TAGS_LEN);
    rd_str(root, "dependencies", out->dependencies, SD_MAX_DEPS_LEN);
    rd_u32(root, "capabilities", &out->capabilities);
    rd_bool(root, "active", &out->active);
    rd_u64(root, "last_updated", &out->last_updated);

    insts = cJSON_GetObjectItemCaseSensitive(root, "instances");
    if (insts && cJSON_IsArray(insts)) {
        for (cJSON *o = insts->child; o && idx < SD_MAX_INSTANCES; o = o->next)
            rd_inst(&out->instances[idx++], o);
    }
    out->instance_count = idx;
    cJSON_Delete(root);
    return AIRY_SUCCESS;
}

/* 单个注册文件刷新：dir 扫描（POSIX）与 FindFirstFile（Win32）共用，
 * 避免双平台重复注册逻辑（0.1.13 G1 wave-2：MSVC 无 dirent/DIR，原循环
 * 直用 opendir/readdir 致 C2065/C2037）。 */
static void sd_file_refresh_name(sd_internal_t *sd, const char *dname)
{
    if (!dname || dname[0] == '.')
        return;
    size_t l = strlen(dname);
    if (l < 6 || strcmp(dname + l - 5, ".json") != 0)
        return;
    char sname[SD_MAX_NAME_LEN];
    sd_file_name_from_path(dname, sname, sizeof(sname));
    if (sname[0] == '\0')
        return;
    sd_service_entry_t entry;
    if (sd_file_read_service(sname, &entry) != AIRY_SUCCESS)
        return;
    int32_t idx = find_service_index(sd, sname);
    if (idx >= 0)
        sd->services[idx] = entry;
    else if (sd->service_count < SD_MAX_SERVICES)
        sd->services[sd->service_count++] = entry;
}

static airy_err_t file_refresh(sd_internal_t *sd, const char *name)
{
    if (name) {
        sd_service_entry_t entry;
        airy_err_t err = sd_file_read_service(name, &entry);
        if (err != AIRY_SUCCESS)
            return err;
        int32_t idx = find_service_index(sd, name);
        if (idx >= 0) {
            sd->services[idx] = entry;
        } else if (sd->service_count < SD_MAX_SERVICES) {
            sd->services[sd->service_count++] = entry;
        }
        return AIRY_SUCCESS;
    }

    char dir[512];
    sd_file_dir(dir, sizeof(dir));
#if AIRY_PLATFORM_POSIX
    DIR *d = opendir(dir);
    if (!d)
        return (errno == ENOENT) ? AIRY_ENOENT : AIRY_ERR_SYS_FILE;
    struct dirent *de;
    while ((de = readdir(d)) != NULL)
        sd_file_refresh_name(sd, de->d_name);
    closedir(d);
#elif defined(_WIN32)
    {
        char pattern[520];
        /* Win32 文件 API 接受正斜杠，避免路径尾分隔符拼接问题 */
        snprintf(pattern, sizeof(pattern), "%s/*.json", dir);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            DWORD e = GetLastError();
            return (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND)
                       ? AIRY_ENOENT : AIRY_ERR_SYS_FILE;
        }
        do {
            sd_file_refresh_name(sd, fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    return AIRY_ERR_SYS_FILE;
#endif
    return AIRY_SUCCESS;
}

static airy_err_t file_commit(sd_internal_t *sd, const char *name)
{
    if (name) {
        int32_t idx = find_service_index(sd, name);
        if (idx < 0)
            return AIRY_ENOENT;
        return sd_file_write_service(&sd->services[idx]);
    }
    airy_err_t err = AIRY_SUCCESS;
    for (uint32_t i = 0; i < sd->service_count; i++) {
        airy_err_t e = sd_file_write_service(&sd->services[i]);
        if (e != AIRY_SUCCESS)
            err = e;
    }
    return err;
}

static airy_err_t file_init(sd_internal_t *sd)
{
    char dir[512];
    sd_file_dir(dir, sizeof(dir));
    if (airy_mkdir_p(dir) != 0)
        return AIRY_ERR_SYS_FILE;
    (void)sd;
    return AIRY_SUCCESS;
}

static void file_deinit(sd_internal_t *sd)
{
    (void)sd;
}

static airy_err_t file_register(sd_internal_t *sd, const char *name, const char *type,
                                const sd_instance_t *inst, const char *tags, const char *deps)
{
    airy_err_t err = sd_registry_add_instance(sd, name, type, inst, tags, deps);
    if (err != AIRY_SUCCESS)
        return err;
    int32_t idx = find_service_index(sd, name);
    if (idx < 0)
        return AIRY_ERR_STATE_ERROR;
    return sd_file_write_service(&sd->services[idx]);
}

static airy_err_t file_deregister(sd_internal_t *sd, const char *name, const char *instance_id)
{
    airy_err_t err = sd_registry_remove_instance(sd, name, instance_id, NULL);
    if (err != AIRY_SUCCESS)
        return err;
    int32_t idx = find_service_index(sd, name);
    if (idx < 0)
        return AIRY_SUCCESS;
    if (sd->services[idx].instance_count == 0) {

        char path[512];
        sd_file_path(name, path, sizeof(path));
        remove(path);
        return AIRY_SUCCESS;
    }
    return sd_file_write_service(&sd->services[idx]);
}

static airy_err_t file_deregister_all(sd_internal_t *sd, const char *name)
{
    int32_t svc_idx = find_service_index(sd, name);
    if (svc_idx < 0)
        return AIRY_ENOENT;
    sd->services[svc_idx].instance_count = 0;
    sd->services[svc_idx].last_updated = airy_time_ms();
    char path[512];
    sd_file_path(name, path, sizeof(path));
    remove(path);
    return AIRY_SUCCESS;
}

static airy_err_t file_lookup(sd_internal_t *sd, const char *name, sd_service_entry_t *out)
{

    file_refresh(sd, name);
    int32_t svc_idx = find_service_index(sd, name);
    if (svc_idx < 0)
        return AIRY_ENOENT;
    __builtin_memcpy(out, &sd->services[svc_idx], sizeof(sd_service_entry_t));
    return AIRY_SUCCESS;
}

static airy_err_t file_list(sd_internal_t *sd, sd_service_entry_t *out, uint32_t max,
                            uint32_t *count)
{
    file_refresh(sd, NULL);
    uint32_t n = 0;
    for (uint32_t i = 0; i < sd->service_count && n < max; i++)
        __builtin_memcpy(&out[n++], &sd->services[i], sizeof(sd_service_entry_t));
    *count = n;
    return AIRY_SUCCESS;
}

const sd_backend_t sd_backend_file = {
    .name = "file",
    .init = file_init,
    .deinit = file_deinit,
    .register_service = file_register,
    .deregister_service = file_deregister,
    .deregister_all = file_deregister_all,
    .lookup_service = file_lookup,
    .list_services = file_list,
    .refresh = file_refresh,
    .commit = file_commit,
};

#endif /* AIRY_HAS_CJSON */
