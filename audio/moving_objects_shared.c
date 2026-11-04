/*
 * Moving objects: the one queue the orender decoder and the Windows spatial
 * output share. mpv runs one of each in a session.
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

#include "osdep/threads.h"

#include "moving_objects.h"

static mp_static_mutex lock = MP_STATIC_MUTEX_INITIALIZER;
static struct mo_queue queue;
static int slots;

void mp_moving_objects_set_slots(int n)
{
    mp_mutex_lock(&lock);
    slots = n;
    mo_queue_clear(&queue);
    mp_mutex_unlock(&lock);
}

int mp_moving_objects_slots(void)
{
    mp_mutex_lock(&lock);
    int n = slots;
    mp_mutex_unlock(&lock);
    return n;
}

void mp_moving_objects_publish(const struct mo_entry *e)
{
    mp_mutex_lock(&lock);
    mo_queue_push(&queue, e);
    mp_mutex_unlock(&lock);
}

void mp_moving_objects_reset(void)
{
    mp_mutex_lock(&lock);
    mo_queue_clear(&queue);
    mp_mutex_unlock(&lock);
}

bool mp_moving_objects_lookup(double pts, int slot, float out[3], bool *active)
{
    mp_mutex_lock(&lock);
    bool found = mo_queue_lookup(&queue, pts, slot, out, active);
    mp_mutex_unlock(&lock);
    return found;
}
