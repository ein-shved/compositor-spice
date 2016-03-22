/*
 * Copyright © 2013-2016 Yury Shvedov <shved@lvk.cs.msu.su>
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

#ifndef COMPOSITOR_SPICE_H
#define COMPOSITOR_SPICE_H

#include <assert.h>
#include <spice.h>

#include "shared/helpers.h"
#include "compositor.h"
#include "pixman-renderer.h"

#include "weston_spice_interfaces.h"

#define NUM_MEMSLOTS        1
#define NUM_MEMSLOTS_GROUPS 1
#define NUM_SURFACES        2
#define MEMSLOT_ID_BITS     1
#define MEMSLOT_GEN_BITS    1

#define MEMSLOT_GROUP 0

#define MAX_WIDTH 1024
#define MAX_HEIGHT 480

/* TODO: is it necessary to support
 * several instances of spice_backend?
 */

struct spice_backend {
    struct weston_backend base;
    struct weston_compositor *compositor;

    SpiceServer *spice_server;

    QXLInstance display_sin;

    SpiceCoreInterface *core;
    SpiceTimer *wakeup_timer;
    QXLWorker *worker;
    uint32_t mm_clock;

    struct spice_output *primary_output;
    struct weston_seat core_seat;

    weston_spice_mouse_t *mouse;
    weston_spice_kbd_t *kbd;
    weston_spice_qxl_t *qxl;

    void (*produce_command) (struct spice_backend*);
    int (*push_command) (struct spice_backend*, QXLCommandExt *);
    void (*release_resource) (struct spice_backend*, QXLCommandExt *);
};

uint32_t
spice_get_primary_surface_id (struct spice_backend *c);

#endif //COMPOSITOR_SPICE_H
