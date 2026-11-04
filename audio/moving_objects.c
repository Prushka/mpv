/*
 * Moving objects: the queue of each slot's positions over the title's time,
 * and the coordinates Windows takes. Pure: the C library only, so that its
 * test builds without mpv (test/moving_objects.c).
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "moving_objects.h"

// An entry is extended until it spans this much of the title.
#define MO_ENTRY_SECONDS 0.010

void mo_queue_clear(struct mo_queue *q)
{
    q->first = 0;
    q->count = 0;
}

static struct mo_entry *at(struct mo_queue *q, int n)
{
    return &q->entries[(q->first + n) % MO_QUEUE_LEN];
}

static const struct mo_entry *at_c(const struct mo_queue *q, int n)
{
    return &q->entries[(q->first + n) % MO_QUEUE_LEN];
}

// The same objects in the same slots, so that one entry can stand for both.
static bool same_slots(const struct mo_entry *a, const struct mo_entry *b)
{
    if (a->slots != b->slots)
        return false;
    for (int n = 0; n < a->slots && n < MO_MAX_SLOTS; n++) {
        if (a->active[n] != b->active[n])
            return false;
    }
    return true;
}

void mo_queue_push(struct mo_queue *q, const struct mo_entry *e)
{
    if (q->count) {
        struct mo_entry *last = at(q, q->count - 1);
        if (last->end_pts - last->start_pts < MO_ENTRY_SECONDS &&
            e->start_pts >= last->start_pts && same_slots(last, e))
        {
            last->end_pts = e->end_pts;
            memcpy(last->end, e->end, sizeof(last->end));
            return;
        }
    }
    if (q->count == MO_QUEUE_LEN) {
        q->first = (q->first + 1) % MO_QUEUE_LEN;
        q->count--;
    }
    *at(q, q->count) = *e;
    q->count++;
}

static void lerp(const float a[3], const float b[3], double t, float out[3])
{
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    for (int n = 0; n < 3; n++)
        out[n] = (float)(a[n] + (b[n] - a[n]) * t);
}

static bool slot_active(const struct mo_entry *e, int slot)
{
    return slot >= 0 && slot < e->slots && slot < MO_MAX_SLOTS && e->active[slot];
}

static int clamp_slot(int slot)
{
    return slot < 0 ? 0 : slot >= MO_MAX_SLOTS ? MO_MAX_SLOTS - 1 : slot;
}

bool mo_queue_lookup(const struct mo_queue *q, double pts, int slot,
                     float out[3], bool *active)
{
    if (!q->count)
        return false;
    int s = clamp_slot(slot);

    // The last entry starting at or before pts.
    int lo = 0, hi = q->count - 1, found = -1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (at_c(q, mid)->start_pts <= pts) {
            found = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    if (found < 0) {
        const struct mo_entry *first = at_c(q, 0);
        memcpy(out, first->start[s], sizeof(float[3]));
        *active = slot_active(first, slot);
        return true;
    }

    const struct mo_entry *e = at_c(q, found);
    *active = slot_active(e, slot);
    if (pts <= e->end_pts) {
        double span = e->end_pts - e->start_pts;
        lerp(e->start[s], e->end[s], span > 0 ? (pts - e->start_pts) / span : 1, out);
        return true;
    }
    if (found + 1 < q->count) {
        const struct mo_entry *next = at_c(q, found + 1);
        double gap = next->start_pts - e->end_pts;
        lerp(e->end[s], next->start[s], gap > 0 ? (pts - e->end_pts) / gap : 1, out);
        return true;
    }
    memcpy(out, e->end[s], sizeof(float[3]));
    return true;
}

void mo_adm_to_windows(const float adm[3], float out[3])
{
    out[0] = adm[0] * MO_WINDOWS_SCALE_M;
    out[1] = adm[2] * MO_WINDOWS_SCALE_M;
    out[2] = -adm[1] * MO_WINDOWS_SCALE_M;
}
