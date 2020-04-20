/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bindings.h"

#include "sinfo.h"
#include "muschedinfo.h"

#define SINFO_ADDR      0xe00000000UL
#define SCHED_INFO_ADDR 0xe00008000UL

static char subject_name[MAX_NAME_LENGTH + 1];
static bool subject_name_unset = true;

static const struct subject_info_type *sinfo =
    (struct subject_info_type *)(SINFO_ADDR);

static volatile struct scheduling_info_type *sched_info =
    (struct scheduling_info_type *)(SCHED_INFO_ADDR);

bool muen_names_equal(const struct muen_name_type *const n1,
                      const char *const n2)
{
    return n1->length == strlen(n2)
        && strncmp(n1->data, n2, n1->length) == 0;
}

struct iterator {
    const struct muen_resource_type *res;
    unsigned int idx;
};

/*
 * Iterate over all resources beginning at given start resource.  If the res
 * member of the iterator is NULL, the function (re)starts the iteration at the
 * first available resource.
 */
static bool iterate_resources(struct iterator *const iter)
{
    if (!muen_check_magic())
        return false;

    if (!iter->res) {
        iter->res = &sinfo->resources[0];
        iter->idx = 0;
    } else {
        iter->res++;
        iter->idx++;
    }
    return iter->idx < sinfo->resource_count
        && iter->res->kind != MUEN_RES_NONE;
}

inline bool muen_check_magic(void)
{
    return sinfo->magic == MUEN_SUBJECT_INFO_MAGIC;
}

const char * muen_get_subject_name(void)
{
    if (!muen_check_magic())
        return NULL;

    if (subject_name_unset) {
        memset(subject_name, 0, MAX_NAME_LENGTH + 1);
        memcpy(subject_name, &sinfo->name.data, sinfo->name.length);
        subject_name_unset = false;
    }

    return subject_name;
}

const struct muen_resource_type *
muen_get_resource(const char *const name, enum muen_resource_kind kind)
{
    struct iterator i = { NULL, 0 };

    while (iterate_resources(&i))
        if (i.res->kind == kind && muen_names_equal(&i.res->name, name))
            return i.res;

    return NULL;
}

const struct muen_device_type * muen_get_device(const uint16_t sid)
{
    struct iterator i = { NULL, 0 };

    while (iterate_resources(&i))
        if (i.res->kind == MUEN_RES_DEVICE &&
                i.res->data.dev.sid == sid)
            return &i.res->data.dev;

    return NULL;
}

bool muen_for_each_resource(resource_cb func, void *data)
{
    struct iterator i = { NULL, 0 };

    while (iterate_resources(&i))
        if (!func(i.res, data))
            return false;

    return true;
}

uint64_t muen_get_tsc_khz(void)
{
    if (!muen_check_magic())
        return 0;

    return sinfo->tsc_khz;
}

uint64_t muen_get_sched_start(void)
{
    if (!muen_check_magic())
        return 0;

    return sched_info->tsc_schedule_start;
}

uint64_t muen_get_sched_end(void)
{
    if (!muen_check_magic())
        return 0;

    return sched_info->tsc_schedule_end;
}
