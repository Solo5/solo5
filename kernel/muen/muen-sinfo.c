/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
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

#include "../kernel.h"

#include "sinfo.h"
#include "musinfo.h"
#include "muschedinfo.h"

#define SINFO_ADDR      0xe00000000UL
#define SCHED_INFO_ADDR 0xe00009000UL

static char subject_name[MAX_NAME_LENGTH + 1];
static bool subject_name_unset = true;

static const struct subject_info_type *sinfo =
    (struct subject_info_type *)(SINFO_ADDR);

static volatile struct scheduling_info_type *sched_info =
    (struct scheduling_info_type *)(SCHED_INFO_ADDR);


static void fill_channel_data(uint8_t idx, struct muen_channel_info *channel)
{
    const struct resource_type resource = sinfo->resources[idx];
    const struct memregion_type memregion =
        sinfo->memregions[resource.memregion_idx - 1];
    const struct channel_info_type channel_info =
        sinfo->channels_info[resource.channel_info_idx - 1];

    memset(&channel->name, 0, MAX_NAME_LENGTH + 1);
    memcpy(&channel->name, resource.name.data, resource.name.length);

    channel->address  = memregion.address;
    channel->size     = memregion.size;
    channel->writable = memregion.flags & MEM_WRITABLE_FLAG;

    channel->has_event    = channel_info.flags & CHAN_EVENT_FLAG;
    channel->event_number = channel_info.event;
    channel->has_vector   = channel_info.flags & CHAN_VECTOR_FLAG;
    channel->vector       = channel_info.vector;
}

static void fill_memregion_data(uint8_t idx, struct muen_memregion_info *region)
{
    const struct resource_type resource = sinfo->resources[idx];
    const struct memregion_type memregion =
        sinfo->memregions[resource.memregion_idx - 1];

    memset(&region->name, 0, MAX_NAME_LENGTH + 1);
    memcpy(&region->name, resource.name.data, resource.name.length);

    memcpy(&region->hash, memregion.hash, HASH_LENGTH);

    region->content    = memregion.content;
    region->address    = memregion.address;
    region->size       = memregion.size;
    region->pattern    = memregion.pattern;
    region->writable   = memregion.flags & MEM_WRITABLE_FLAG;
    region->executable = memregion.flags & MEM_EXECUTABLE_FLAG;
}

static bool is_memregion(const struct resource_type * const resource)
{
    return resource->memregion_idx != NO_RESOURCE;
}

static bool is_channel(const struct resource_type * const resource)
{
    return is_memregion(resource) &&
           resource->channel_info_idx != NO_RESOURCE;
}

static void fill_dev_data(uint8_t idx, struct muen_dev_info *dev)
{
    const struct dev_info_type dev_info = sinfo->dev_info[idx];

    dev->sid         = dev_info.sid;
    dev->irte_start  = dev_info.irte_start;
    dev->irq_start   = dev_info.irq_start;
    dev->ir_count    = dev_info.ir_count;
    dev->msi_capable = dev_info.flags & DEV_MSI_FLAG;
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

bool muen_get_channel_info(const char * const name,
               struct muen_channel_info *channel)
{
    int i;

    if (!muen_check_magic())
        return false;

    for (i = 0; i < sinfo->resource_count; i++) {
        if (is_channel(&sinfo->resources[i]) &&
            strncmp(sinfo->resources[i].name.data, name,
                sinfo->resources[i].name.length) == 0) {
            fill_channel_data(i, channel);
            return true;
        }
    }
    return false;
}

bool muen_get_memregion_info(const char * const name,
                 struct muen_memregion_info *memregion)
{
    int i;

    if (!muen_check_magic())
        return false;

    for (i = 0; i < sinfo->resource_count; i++) {
        if (is_memregion(&sinfo->resources[i]) &&
            strncmp(sinfo->resources[i].name.data, name,
                sinfo->resources[i].name.length) == 0) {
            fill_memregion_data(i, memregion);
            return true;
        }
    }
    return false;
}

bool muen_get_dev_info(const uint16_t sid, struct muen_dev_info *dev)
{
    int i;

    if (!muen_check_magic())
        return false;

    for (i = 0; i < sinfo->dev_info_count; i++) {
        if (sinfo->dev_info[i].sid == sid) {
            fill_dev_data(i, dev);
            return true;
        }
    }
    return false;
}

bool muen_for_each_channel(channel_cb func, void *data)
{
    int i;
    struct muen_channel_info current_channel;

    if (!muen_check_magic())
        return false;

    for (i = 0; i < sinfo->resource_count; i++) {
        if (is_channel(&sinfo->resources[i])) {
            fill_channel_data(i, &current_channel);
            if (!func(&current_channel, data))
                return false;
        }
    }
    return true;
}

bool muen_for_each_memregion(memregion_cb func, void *data)
{
    int i;
    struct muen_memregion_info current_region;

    if (!muen_check_magic())
        return false;

    for (i = 0; i < sinfo->resource_count; i++) {
        if (is_memregion(&sinfo->resources[i])) {
            fill_memregion_data(i, &current_region);
            if (!func(&current_region, data))
                return false;
        }
    }
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
