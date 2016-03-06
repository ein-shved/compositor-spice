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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include <spice.h>
#include <spice/qxl_dev.h>
#include <spice/macros.h>

#include "compositor-spice.h"
#include "weston_basic_event_loop.h"
#include "weston_qxl_commands.h"

struct spice_backend_config {
    const char* addr;
    int flags;
    int port;
    char *password;
    char *image_compression;
};
struct spice_output {
    struct weston_output base;
    struct spice_backend *backend;

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
    struct spice_backend *b  = output->backend;

    spice_server_vm_start(b->spice_server);
    b->core->timer_start(b->primary_output->wakeup_timer, 5);
}
static int
spice_output_repaint (struct weston_output *output_base,
        pixman_region32_t *damage)
{
    struct spice_output *output = (struct spice_output *) output_base;
    struct spice_backend *b = output->backend;

    output->base.compositor->renderer->repaint_output (output_base, damage);

    if (output->full_image_id == 0) {
        output->full_image_id = spice_create_image(b);
    }

    return spice_paint_image (b, output->full_image_id,
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
    struct spice_backend *b = output->backend;

    b->core->timer_cancel (output->wakeup_timer);
    b->core->timer_remove (output->wakeup_timer);

    free ( pixman_image_get_destroy_data(output->full_image));
    free (output->full_image);
}

static void
on_wakeup (void *opaque) {
    struct spice_output *output = (struct spice_output *)opaque;
    struct spice_backend *b = output->backend;
	struct timespec ts;

	weston_compositor_read_presentation_clock(b->compositor, &ts);

    b->core->timer_start (output->wakeup_timer, 1);
    spice_qxl_wakeup(&b->display_sin);

    weston_output_finish_frame (&output->base, &ts, 0);
    weston_output_schedule_repaint (&output->base);
}

static struct spice_output *
spice_create_output ( struct spice_backend *b,
        int x, int y,
        int width, int height,
        uint32_t transform )
{
    struct spice_output *output;

    if (b->core == NULL) {
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
        spice_create_primary_surface (b, width, height,
            output->surface);

    output->full_image_id = spice_create_image (b);

    output->has_spice_surface = FALSE;
    output->backend = b;
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

    weston_output_init ( &output->base, b->compositor,
                x, y, width, height, transform, 1 );
    if( pixman_renderer_output_create (&output->base) <0) {
        goto err_pixman_create;
    }
    pixman_renderer_output_set_buffer (&output->base, output->full_image);
    wl_list_insert(b->compositor->output_list.prev, &output->base.link);

    output->wakeup_timer = b->core->timer_add(on_wakeup, output);
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
    weston_log("Failed to initialize spice compositor");
    return NULL;
}

/* This will find corresponding integer representation of spice's compression
 * type by it's string representation or return -1. This function is
 * case insensitive.
 */
static int parse_spice_name(const char *in_name,
        const char *names[], size_t len)
{
    char *name = strdupa(in_name), *p;
    unsigned i = 0;

    /* Make input lowercase */
    for(p=name; *p != '\0'; ++p) {
        *p = tolower(*p);
    }

    /* Search through names */
    for (i=0; i < len; ++i){
        /* Ignore empty fields */
        if (names[i] == NULL) {
            continue;
        }
        if (!strcmp(name, names[i])) {
            return i;
        }
    }
    /* Failure at the end */
    return -1;
};
static const char *image_compression_names[] = {
    [ SPICE_IMAGE_COMPRESS_OFF ]      = "off",
    [ SPICE_IMAGE_COMPRESS_AUTO_GLZ ] = "auto_glz",
    [ SPICE_IMAGE_COMPRESS_AUTO_LZ ]  = "auto_lz",
    [ SPICE_IMAGE_COMPRESS_QUIC ]     = "quic",
    [ SPICE_IMAGE_COMPRESS_GLZ ]      = "glz",
    [ SPICE_IMAGE_COMPRESS_LZ ]       = "lz",
#if SPICE_SERVER_VERSION >= 0x000c06
    [ SPICE_IMAGE_COMPRESS_LZ4 ]      = "lz4",
#endif
};
static inline spice_image_compression_t
parse_spice_image_compression_name(const char *in_name) {
    int res = parse_spice_name(in_name, image_compression_names,
            ARRAY_LENGTH(image_compression_names));
    return res < 0 ? SPICE_IMAGE_COMPRESS_INVALID :
                        (spice_image_compression_t)res;
}

static int
weston_spice_server_new (struct spice_backend *b,
        const struct spice_backend_config *config)
{
    /* Choose image compression */
    spice_image_compression_t compression = SPICE_IMAGE_COMPRESS_AUTO_GLZ;
    if (config->image_compression) {
        compression = parse_spice_image_compression_name(
                config->image_compression);
        if (compression == SPICE_IMAGE_COMPRESS_INVALID) {
            weston_log("Invalid image compression '%s'\n",
                    config->image_compression);
            return -1;
        }
    }
    weston_log("Using image compression '%s'\n",
            image_compression_names[compression]);

    //Init spice server
    b->spice_server = spice_server_new();
    spice_server_set_addr(b->spice_server, config->addr, config->flags);
    spice_server_set_port(b->spice_server, config->port);
    if (config->password) {
        spice_server_set_ticket(b->spice_server, config->password, 0, 0, 0);
    } else {
        spice_server_set_noauth (b->spice_server);
    }
    spice_server_set_image_compression(b->spice_server, compression);

    //TODO set another spice server options here
    spice_server_init (b->spice_server, b->core);

    //qxl interface
    weston_spice_qxl_init (b);
    spice_server_add_interface (b->spice_server, &b->display_sin.base);

    return 0;
}

static int
weston_spice_input_init (struct spice_backend *b,
        const struct spice_backend_config *config)
{
    weston_seat_init (&b->core_seat, b->compositor, "default");

    //mouse interface
    weston_spice_mouse_init (b);

    //keyboard interface
    if ( weston_spice_kbd_init (b) < 0) {
        return -1;
    }
    return 0;
}

static void
spice_destroy (struct weston_compositor *ec)
{
    struct spice_backend *b = (struct spice_backend*) ec->backend;

    weston_compositor_shutdown (ec);

    //ec->renderer->destroy(ec);
    spice_server_vm_stop(b->spice_server);

    /* TODO: after calling next line double free detect.
     * recognize, why?
     */
    //spice_server_destroy(b->spice_server);

    weston_spice_mouse_destroy (b);
    weston_spice_kbd_destroy (b);
    weston_spice_qxl_destroy (b);

    free (b->primary_output->surface);
    free (b->primary_output);
}

static void
spice_restore (struct weston_compositor *compositor_base)
{
}

static struct spice_backend *
spice_backend_create (struct weston_compositor *compositor,
        const struct spice_backend_config *config,
        int *argc, char *argv[],
        struct weston_config *wconfig )
{
    struct spice_backend *b;

    b = zalloc(sizeof *b);
    if ( b == NULL ) {
        return NULL;
    }

    b->compositor = compositor;
    b->base.destroy = spice_destroy;
    b->base.restore = spice_restore;

	if (weston_compositor_set_presentation_clock_software(compositor) < 0)
		goto err_compositor;
	if (pixman_renderer_init(compositor) < 0)
		goto err_compositor;
#if 0
    if (weston_compositor_init (&b->base, display, argc, argv, config) < 0)
    {
        goto err_weston_init;
    }
#endif

	compositor->capabilities |= WESTON_CAP_ARBITRARY_MODES;

    b->core = basic_event_loop_init(compositor);
    if (weston_spice_server_new (b, config) < 0) {
        goto err_server;
    }

    weston_log ("Spice server is up on %s:%d\n",
            config->addr[0] == '\0' ? "*" : config->addr, config->port);

    if (weston_spice_input_init(b, config) < 0) {
        goto err_input_init;
    }

    b->primary_output = spice_create_output(b, 0, 0, //(x,y)
                MAX_WIDTH, MAX_HEIGHT, //WIDTH x HEIGTH
                WL_OUTPUT_TRANSFORM_NORMAL); //transform

    if (b->primary_output == NULL ) {
        goto err_output;
    }
    compositor->backend = &b->base;
    return b;

err_output:
err_input_init:
err_compositor:
err_server:
    free (b);
    return NULL;
}

WL_EXPORT int
backend_init(struct weston_compositor *compositor, int *argc, char *argv[],
        struct weston_config *wconfig,
        struct weston_backend_config *config_base)
{
    struct spice_backend *b;
    struct spice_backend_config config = {
        .addr = "",
        .port = 5912,
        .flags = 0,
        .password = NULL,
        .image_compression = NULL,
    };

    const struct weston_option spice_options[] = {
		{ WESTON_OPTION_STRING,  "host", 0, &config.addr },
		{ WESTON_OPTION_INTEGER, "port", 0, &config.port },
		{ WESTON_OPTION_STRING,  "password", 0, &config.password },
		{ WESTON_OPTION_STRING,  "image-compression", 0, &config.image_compression },
        //TODO parse auth options here
	};

    parse_options (spice_options, ARRAY_LENGTH (spice_options), argc, argv);
    weston_log ("Initialising spice compositor\n");
    b = spice_backend_create (compositor, &config, argc, argv, wconfig);
    if (b == NULL ) {
        return -1;
    }
    return 0;
}

uint32_t
spice_get_primary_surface_id (struct spice_backend *b)
{
    return b->primary_output->spice_surface_id;
}
