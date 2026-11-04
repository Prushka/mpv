/*
 * Moving objects: where each of Omniphony's object slots is, over the title's
 * time, carried from the orender decoder to the Windows spatial output.
 *
 * The decoder publishes, for every block of audio it outputs, each slot's
 * position at the block's first and last frame, stamped with the block's
 * presentation time; the output looks a slot up at the time of the samples it
 * is about to play. The queue and its lookup are pure (moving_objects.c, the
 * C library only); the one queue the two share lives in
 * moving_objects_shared.c.
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

#pragma once

#include <stdbool.h>

// The most slots Omniphony gives: the most a home Atmos stream carries.
#define MO_MAX_SLOTS 16
// Entries of at least 10 ms each: far more than the audio between the
// decoder and the output.
#define MO_QUEUE_LEN 4096
// Metres in Windows' space for one unit of ADM's.
#define MO_WINDOWS_SCALE_M 1.0f

struct mo_entry {
    double start_pts, end_pts;
    int slots;
    bool active[MO_MAX_SLOTS];
    // ADM cartesian: right, front, up.
    float start[MO_MAX_SLOTS][3];
    float end[MO_MAX_SLOTS][3];
};

struct mo_queue {
    struct mo_entry entries[MO_QUEUE_LEN];
    int first, count;
};

void mo_queue_clear(struct mo_queue *q);

// Extends the newest entry while it spans under 10 ms and its slots hold the
// same objects; else appends, dropping the oldest when full.
void mo_queue_push(struct mo_queue *q, const struct mo_entry *e);

// The slot's position at pts: interpolated within an entry and across the
// gap to the next, the first start before the first entry, the last end
// after the last. False when the queue is empty.
bool mo_queue_lookup(const struct mo_queue *q, double pts, int slot,
                     float out[3], bool *active);

// ADM (right, front, up) to Windows (right, up, back), in metres.
void mo_adm_to_windows(const float adm[3], float out[3]);

// The queue the decoder and the output share (moving_objects_shared.c).
void mp_moving_objects_set_slots(int slots);
int mp_moving_objects_slots(void);
void mp_moving_objects_publish(const struct mo_entry *e);
void mp_moving_objects_reset(void);
bool mp_moving_objects_lookup(double pts, int slot, float out[3], bool *active);
