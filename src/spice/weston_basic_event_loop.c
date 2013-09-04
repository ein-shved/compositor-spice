/*
 * Copyright © 2013 Yury Shvedov <shved@lvk.cs.msu.su>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <spice/macros.h>
#include "weston_basic_event_loop.h"
#include "compositor-spice.h"

static SpiceCoreInterface core;
static struct wl_event_loop *loop = NULL;

typedef struct SpiceTimer {
    struct wl_event_source *event_source;
    SpiceTimerFunc func;
    void *opaque;
} Timer;

typedef struct SpiceWatch {
    struct wl_event_source *event_source;
    SpiceWatchFunc func;
    void *opaque;
} Watch;

static int
exec_timer (void *data) {
    SpiceTimer *timer = data;

    timer->func(timer->opaque);
    return 1;
}

static SpiceTimer* 
timer_add(SpiceTimerFunc func, void *opaque)
{
    SpiceTimer *timer;
    
    dprint (3, "called");

    assert (loop != NULL);

    timer = calloc(sizeof(SpiceTimer), 1);

    if (timer == NULL) {
        goto err_timer_malloc;
    }

    timer->func = func;
    timer->opaque = opaque;
    timer->event_source = wl_event_loop_add_timer (loop, 
        exec_timer, timer );
    if (timer->event_source == NULL) {
        goto err_add;
    }
    return timer;

err_add:
    free (timer);
err_timer_malloc:
    return NULL;
}
static void 
timer_start (SpiceTimer *timer, uint32_t ms)
{
    wl_event_source_timer_update (
            timer->event_source, ms);
}
static void 
timer_cancel (SpiceTimer *timer) 
{
    wl_event_source_timer_update (
            timer->event_source, 0);
}
static void 
timer_remove (SpiceTimer *timer)
{
    timer_cancel (timer);
    wl_event_source_remove (timer->event_source);
    free (timer);
}

static int
exec_watch (int fd, uint32_t mask, void *data) 
{
    SpiceWatch *watch = data;

    watch->func(fd, mask, watch->opaque);
    return 1;
}
static SpiceWatch *
watch_add (int fd, int event_mask, SpiceWatchFunc func, void *opaque)
{
    SpiceWatch *watch;

    assert (loop != NULL);
    watch = calloc (sizeof *watch, 1);
    if (watch == NULL) {
        return NULL;
    }

    watch->event_source =  wl_event_loop_add_fd (
        loop, fd, event_mask, exec_watch, watch ); 
    watch->func = func;
    watch->opaque = opaque;

    return watch;

err_add:
    free (watch);
err_watch_malloc:
    return NULL;
}
void 
watch_update_mask (SpiceWatch *watch, int event_mask)
{
    wl_event_source_fd_update (watch->event_source, event_mask);
}
void 
watch_remove (SpiceWatch *watch)
{
    wl_event_source_remove (watch->event_source);
    free (watch);
}

void 
channel_event (int event, SpiceChannelEventInfo *info)
{
}

SpiceCoreInterface *
basic_event_loop_init(struct wl_display *display)
{
    loop = wl_display_get_event_loop (display); 
    memset(&core, 0, sizeof(core));
    core.base.major_version = SPICE_INTERFACE_CORE_MAJOR;
    core.base.minor_version = SPICE_INTERFACE_CORE_MINOR; // anything less then 3 and channel_event isn't called
    core.timer_add = timer_add;
    core.timer_start = timer_start;
    core.timer_cancel = timer_cancel;
    core.timer_remove = timer_remove;
    core.watch_add = watch_add;
    core.watch_update_mask = watch_update_mask;
    core.watch_remove = watch_remove;
    core.channel_event = channel_event;
    return &core;
}
