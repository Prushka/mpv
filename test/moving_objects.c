/*
 * The moving objects queue: lookups by the title's time, coalescing, a seek,
 * another speed, and the coordinates Windows takes.
 *
 * Standalone, so that it builds without mpv:
 *   gcc -std=gnu11 -O2 -I. -o moving-objects-test test/moving_objects.c \
 *       audio/moving_objects.c -lm
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/moving_objects.h"

static int failures;

#define check(cond) do {                                                    \
    if (!(cond)) {                                                          \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        failures++;                                                         \
    }                                                                       \
} while (0)

static bool near3(const float a[3], float x, float y, float z)
{
    return fabsf(a[0] - x) < 1e-4f && fabsf(a[1] - y) < 1e-4f &&
           fabsf(a[2] - z) < 1e-4f;
}

static struct mo_entry entry(double start, double end, const float from[3],
                             const float to[3])
{
    struct mo_entry e = {.start_pts = start, .end_pts = end, .slots = 1};
    e.active[0] = true;
    memcpy(e.start[0], from, sizeof(e.start[0]));
    memcpy(e.end[0], to, sizeof(e.end[0]));
    return e;
}

static struct mo_queue q;

static void lookup_inside_an_entry(void)
{
    mo_queue_clear(&q);
    struct mo_entry e = entry(0, 0.010, (float[3]){-1, 1, 0}, (float[3]){1, 1, 0});
    mo_queue_push(&q, &e);
    float pos[3];
    bool active = false;
    check(mo_queue_lookup(&q, 0.005, 0, pos, &active));
    check(active);
    check(near3(pos, 0, 1, 0));
}

static void lookup_across_a_gap_and_past_the_end(void)
{
    mo_queue_clear(&q);
    struct mo_entry a = entry(0, 0.010, (float[3]){0, 0, 0}, (float[3]){0, 1, 0});
    struct mo_entry b = entry(0.020, 0.030, (float[3]){0, 0, 1}, (float[3]){1, 0, 1});
    mo_queue_push(&q, &a);
    mo_queue_push(&q, &b);
    float pos[3];
    bool active;
    check(mo_queue_lookup(&q, 0.015, 0, pos, &active));
    check(near3(pos, 0, 0.5f, 0.5f));
    check(mo_queue_lookup(&q, 0.5, 0, pos, &active));
    check(near3(pos, 1, 0, 1));
    check(mo_queue_lookup(&q, -1, 0, pos, &active));
    check(near3(pos, 0, 0, 0));
}

static void pushes_coalesce_to_10_ms(void)
{
    mo_queue_clear(&q);
    for (int n = 0; n < 40; n++) {
        float x = n / 40.0f;
        struct mo_entry e = entry(n / 1200.0, (n + 1) / 1200.0,
                                  (float[3]){x, 0, 0}, (float[3]){x + 0.025f, 0, 0});
        mo_queue_push(&q, &e);
    }
    check(q.count == 4);
    float pos[3];
    bool active;
    check(mo_queue_lookup(&q, 1, 0, pos, &active));
    check(near3(pos, 1, 0, 0));
}

static void clear_forgets_what_came_before_a_seek(void)
{
    mo_queue_clear(&q);
    struct mo_entry before = entry(10, 10.01, (float[3]){-1, 0, 0}, (float[3]){-1, 0, 0});
    mo_queue_push(&q, &before);
    mo_queue_clear(&q);
    float pos[3];
    bool active;
    check(!mo_queue_lookup(&q, 10, 0, pos, &active));
    struct mo_entry after = entry(40, 40.01, (float[3]){1, 0, 0}, (float[3]){1, 0, 0});
    mo_queue_push(&q, &after);
    check(mo_queue_lookup(&q, 40, 0, pos, &active));
    check(near3(pos, 1, 0, 0));
    check(mo_queue_lookup(&q, 10, 0, pos, &active));
    check(near3(pos, 1, 0, 0));
}

static void positions_follow_the_titles_time_at_another_speed(void)
{
    // At 1.5x, 10 ms of output carry 15 ms of the title: the entries are
    // stamped in title time, and the output looks up by title time.
    mo_queue_clear(&q);
    struct mo_entry a = entry(0, 0.015, (float[3]){0, 0, 0}, (float[3]){0, 1, 0});
    struct mo_entry b = entry(0.015, 0.030, (float[3]){0, 1, 0}, (float[3]){0, 1, 1});
    mo_queue_push(&q, &a);
    mo_queue_push(&q, &b);
    float pos[3];
    bool active;
    check(mo_queue_lookup(&q, 0.0225, 0, pos, &active));
    check(near3(pos, 0, 1, 0.5f));
}

static void adm_to_windows(void)
{
    float out[3];
    mo_adm_to_windows((float[3]){0.5f, 1, 0.25f}, out);
    check(near3(out, 0.5f, 0.25f, -1));
}

static void inactive_slots_say_so(void)
{
    mo_queue_clear(&q);
    struct mo_entry e = entry(0, 0.010, (float[3]){0, 1, 0}, (float[3]){0, 1, 0});
    e.slots = 2;
    mo_queue_push(&q, &e);
    float pos[3];
    bool active = true;
    check(mo_queue_lookup(&q, 0.005, 1, pos, &active));
    check(!active);
    active = true;
    check(mo_queue_lookup(&q, 0.005, 5, pos, &active));
    check(!active);
}

int main(void)
{
    lookup_inside_an_entry();
    lookup_across_a_gap_and_past_the_end();
    pushes_coalesce_to_10_ms();
    clear_forgets_what_came_before_a_seek();
    positions_follow_the_titles_time_at_another_speed();
    adm_to_windows();
    inactive_slots_say_so();
    if (failures) {
        fprintf(stderr, "moving objects: %d failed\n", failures);
        return 1;
    }
    printf("moving objects: ok\n");
    return 0;
}
