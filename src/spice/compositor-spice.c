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
#include <string.h>
#include <sys/time.h>

#include <spice.h>
#include <spice/qxl_dev.h>
#include <spice/macros.h>

#include "../compositor.h"
#include "../pixman-renderer.h"
#include "compositor-spice.h"
#include "weston_basic_event_loop.h"
#include "weston_qxl_commands.h"


struct spice_server_ops {
    const char* addr;
    int flags;
    int port;
    int no_auth;
};
struct spice_output {
    struct weston_output base;
    struct spice_compositor *compositor;

    struct weston_mode mode;

    int has_spice_surface;
    uint32_t spice_surface_id;
    uint8_t *surface;

    uint32_t full_image_id;
    pixman_image_t *full_image;

    struct SpiceTimer *wakeup_timer;
};

static void
spice_output_start_repaint_loop(struct weston_output *output_base)
{
    struct spice_output *output = (struct spice_output*) output_base;
    struct spice_compositor *c  = output->compositor;

    c->worker->start(c->worker);
    c->core->timer_start ( c->primary_output->wakeup_timer, 5);
}
static void
spice_output_repaint (struct weston_output *output_base,
        pixman_region32_t *damage)
{
    struct spice_compositor *c = (struct spice_compositor *)
            output_base->compositor;
    struct spice_output *output = (struct spice_output *) output_base;

    dprint (3, "called");

    output->base.compositor->renderer->repaint_output (output_base, damage);
    
    if (output->full_image_id == 0) {
        output->full_image_id = spice_create_image(c);
    }

    spice_paint_image (c, output->full_image_id, 
            output_base->x,
            output_base->y,
            output_base->width,
            output_base->height,
            (intptr_t)pixman_image_get_data(output->full_image),
            output_base->width * 4,
            damage );
}

static void
spice_output_destroy ( struct weston_output *output_base)
{
    struct spice_output *output = (struct spice_output*) output_base;
    struct spice_compositor *c = output->compositor;

    c->core->timer_cancel (output->wakeup_timer);
    c->core->timer_remove (output->wakeup_timer);  

    free ( pixman_image_get_destroy_data(output->full_image));
    free (output->full_image);
}

static void
on_wakeup (void *opaque) {
    struct spice_output *output = opaque;
    struct spice_compositor *c = output->compositor;
    uint32_t msec;
    struct timeval tv;
	
	gettimeofday(&tv, NULL);
	msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    c->worker->wakeup(c->worker);    
    c->core->timer_start (output->wakeup_timer, 1);

    weston_output_finish_frame (&output->base, msec);
    weston_output_schedule_repaint (&output->base);
}

static struct spice_output *
spice_create_output ( struct spice_compositor *c,
        int x, int y,
        int width, int height,
        uint32_t transform )
{
    struct spice_output *output;

    if (c->core == NULL) {
        goto err_core_interface;
    }
    output = malloc (sizeof *output);
    if (output == NULL) {
        goto err_output_malloc;
    }
    memset (output, 0, sizeof *output);

    output->surface = malloc (width * height * 4);
    if (output->surface == NULL) {
        goto err_surface_malloc;
    }
    memset (output->surface, 0, width * height * 4);

    output->full_image = pixman_image_create_bits ( PIXMAN_a8r8g8b8,
            width, height, NULL, width*4 );
    if (output->full_image == NULL) {
        goto err_image_malloc;
    }

    output->spice_surface_id =
        spice_create_primary_surface (c, width, height,
            output->surface);

    output->full_image_id = spice_create_image (c);

    output->has_spice_surface = FALSE;
    output->compositor = c;
	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = width;
	output->mode.height = height;
	output->mode.refresh = 6;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

    //output->base.origin         = output->base.current;
    output->base.start_repaint_loop = spice_output_start_repaint_loop;
    output->base.repaint            = spice_output_repaint;
    output->base.destroy            = spice_output_destroy;
    output->base.assign_planes      = NULL;
	output->base.set_backlight      = NULL;
	output->base.set_dpms           = NULL;
	output->base.switch_mode        = NULL;
    
    output->base.current_mode       = &output->mode;
    output->base.make               = "none";
	output->base.model              = "none";

    weston_output_init ( &output->base, &c->base,
                x, y, width, height, transform, 1 );
    if( pixman_renderer_output_create (&output->base) <0) {
        goto err_pixman_create;
    }
    pixman_renderer_output_set_buffer (&output->base, output->full_image);
    wl_list_insert(c->base.output_list.prev, &output->base.link);

    output->wakeup_timer = c->core->timer_add(on_wakeup, output);
    if (output->wakeup_timer == NULL) {
        goto err_timer;
    }

    weston_log ("Spice output created on (%d,%d), width: %d, height: %d\n",
                x,y,width,height);

    return output;

err_timer:
err_pixman_create:
    free (output->full_image);
err_image_malloc:
    free (output->surface);
err_surface_malloc:
    free (output);
err_output_malloc:
err_core_interface:
    weston_log_error("NULL");
    return NULL;
}

static void 
weston_spice_server_new ( struct spice_compositor *c, 
        const struct spice_server_ops *ops )
{
    //Init spice server
    c->spice_server = spice_server_new();
    
    spice_server_set_addr ( c->spice_server,
                            ops->addr, ops->flags );
    spice_server_set_port ( c->spice_server,
                            ops->port );
    if (ops->no_auth ) {
        spice_server_set_noauth ( c->spice_server );
    }

    //TODO set another spice server options here   

    spice_server_init (c->spice_server, c->core);

    //qxl interface
    weston_spice_qxl_init (c);
    spice_server_add_interface (c->spice_server, &c->display_sin.base);
}

static int
weston_spice_input_init ( struct spice_compositor *c, 
        const struct spice_server_ops *ops )
{
    weston_seat_init (&c->core_seat, &c->base, "default");
    
    //mouse interface
    weston_spice_mouse_init (c);

    //keyboard interface
    if ( weston_spice_kbd_init (c) < 0) {
        return -1;
    }
    return 0;
}

static void 
spice_destroy (struct weston_compositor *ec)
{
    struct spice_compositor *c = (struct spice_compositor*) ec;

    weston_compositor_shutdown (ec);

    ec->renderer->destroy(ec);
    c->worker->stop (c->worker);

    /* TODO: after calling next line double free detect.
     * recognize, why?
     */
    //spice_server_destroy(c->spice_server);

    weston_spice_mouse_destroy (c);
    weston_spice_kbd_destroy (c);
    weston_spice_qxl_destroy (c);

    free (c->primary_output->surface);
    free (c->primary_output);
}

static void
spice_restore (struct weston_compositor *compositor_base)
{
}

static struct spice_compositor *
spice_compositor_create ( struct wl_display *display, 
        const struct spice_server_ops *ops, 
        int *argc, char *argv[], 
        struct weston_config *config )
{
    struct spice_compositor *c;

    c = malloc(sizeof *c);
    if ( c == NULL ) {
        goto err_malloc_compositor;
    }
    memset (c, 0, sizeof *c);

    if (weston_compositor_init (&c->base, display, argc, argv, config) < 0) 
    {
        goto err_weston_init;
    }
    
    c->core = basic_event_loop_init(display);
    weston_spice_server_new (c, ops);
    
    weston_log ("Spice server is up on %s:%d\n", ops->addr, ops->port);

    c->base.wl_display = display;
    if ( pixman_renderer_init (&c->base) < 0) {
        goto err_init_pixman;
    } 
    weston_log ("Using %s renderer\n", "pixman");

    c->base.destroy = spice_destroy;
    c->base.restore = spice_restore;

    if ( weston_spice_input_init(c, ops) < 0) {
        goto err_input_init;
    }

    c->primary_output = spice_create_output ( c,
                0, 0, //(x,y)
                MAX_WIDTH, MAX_HEIGHT, //WIDTH x HEIGTH
                WL_OUTPUT_TRANSFORM_NORMAL ); //transform

    if (c->primary_output == NULL ) {
        goto err_output;
    }

    return c;

err_output:
err_input_init:
err_init_pixman:
err_weston_init:
    free (c);
err_malloc_compositor:
    return NULL;
}

WL_EXPORT struct weston_compositor *
backend_init( struct wl_display *display, int *argc, char *argv[],
        struct weston_config *config)
{
    struct spice_compositor *c;
    struct spice_server_ops ops = {
        .addr = "localhost",
        .port = 5912,
        .flags = 0,
        .no_auth = TRUE,
    };

    const struct weston_option spice_options[] = {
		{ WESTON_OPTION_STRING,  "host", 0, &ops.addr },
		{ WESTON_OPTION_INTEGER, "port", 0, &ops.port },
        //TODO parse auth options here
	};

    parse_options (spice_options, ARRAY_LENGTH (spice_options), argc, argv);
    weston_log ("Initialising spice compositor\n");
    c = spice_compositor_create ( display, &ops,
            argc, argv, config);
    if (c == NULL ) {
        return NULL;
    }

    dprint (3, "done: %lx", (long unsigned)c);

    return &c->base;
}

uint32_t
spice_get_primary_surface_id (struct spice_compositor *c)
{
    return c->primary_output->spice_surface_id;
}
