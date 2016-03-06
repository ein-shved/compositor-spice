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

#include <linux/input.h>
#include <stdlib.h>

#include "compositor-spice.h"
#include "weston_spice_interfaces.h"

#define button_equal(old, new, mask) \
    ( ( (old) & (mask) ) == ( (new) & (mask) ) )

// TODO find previous declaration of next three
#define WESTON_SPICE_BTN_LEFT (1 << 0)
#define WESTON_SPICE_BTN_MIDDLE (1 << 2)
#define WESTON_SPICE_BTN_RIGHT (1 << 1)

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int(10)

/* TODO ledstate getting
 * TODO free mouse and kbd interfaces
 */
struct weston_spice_mouse {
    SpiceMouseInstance sin;
    uint32_t buttons_state;
    struct spice_backend *b;
};

struct weston_spice_kbd {
    SpiceKbdInstance sin;
    uint8_t ledstate;
    int escape;
    struct spice_backend *b;
};

static void
weston_mouse_button_notify (struct spice_backend *b,
        struct weston_spice_mouse* mouse, uint32_t buttons_state)
{
    uint32_t buttons = mouse->buttons_state;
    enum wl_pointer_button_state state;

#define DELIVER_NOTIFY_BUTTON(mask, btn)\
    if ( !button_equal (buttons, buttons_state, mask ) ) { \
        state = buttons_state & mask ? \
                WL_POINTER_BUTTON_STATE_PRESSED : \
                WL_POINTER_BUTTON_STATE_RELEASED; \
        notify_button (&b->core_seat, weston_compositor_get_time(), \
                    btn, state ); \
    }
    DELIVER_NOTIFY_BUTTON( WESTON_SPICE_BTN_LEFT, BTN_LEFT);
    DELIVER_NOTIFY_BUTTON( WESTON_SPICE_BTN_MIDDLE, BTN_MIDDLE);
    DELIVER_NOTIFY_BUTTON( WESTON_SPICE_BTN_RIGHT, BTN_RIGHT);

#undef DELIVER_NOTIFY_BUTTON
    mouse->buttons_state = buttons_state;
}

static void
weston_mouse_motion (SpiceMouseInstance *sin, int dx, int dy, int dz,
        uint32_t buttons_state)
{
    struct weston_spice_mouse *mouse = wl_container_of(sin, mouse, sin);
    struct spice_backend *b = mouse->b;
    struct weston_pointer_motion_event motion_ev = {
        WESTON_POINTER_MOTION_REL,0,0,
        /* TODO is it necessary? */
        wl_fixed_to_double(wl_fixed_from_int(dx)),
        wl_fixed_to_double(wl_fixed_from_int(dy)),
    };
    struct weston_pointer_axis_event axis_ev = {
        WL_POINTER_AXIS_VERTICAL_SCROLL,
        wl_fixed_from_int(dz),
        0, 0
    };
    notify_motion(&b->core_seat, weston_compositor_get_time(), &motion_ev);

    if (dz) {
        notify_axis (&b->core_seat, weston_compositor_get_time(),
                     &axis_ev);
    }
    weston_mouse_button_notify (b, mouse, buttons_state);
}
static void
weston_mouse_buttons (SpiceMouseInstance *sin, uint32_t buttons_state )
{
    struct weston_spice_mouse *mouse = wl_container_of(sin, mouse, sin);
    struct spice_backend *b = mouse->b;

    /*if (!b->core_seat.has_pointer) {
        return;
    }*/
    weston_mouse_button_notify (b, mouse, buttons_state);
}

static struct SpiceMouseInterface weston_mouse_interface = {
    .base.type          = SPICE_INTERFACE_MOUSE,
    .base.description   = "weston mouse",
    .base.major_version = SPICE_INTERFACE_MOUSE_MAJOR,
    .base.minor_version = SPICE_INTERFACE_MOUSE_MINOR,

    .motion     = weston_mouse_motion,
    .buttons    = weston_mouse_buttons,
};

void
weston_spice_mouse_init (spice_backend_t *b)
{
    static int mouse_count = 0;
    struct weston_spice_mouse *mouse;

    if ( ++mouse_count > 1 ) {
        weston_log("Only one instance of mouse interface is supported");
        exit(1);
    }

    mouse = calloc (1, sizeof *mouse);
    mouse->sin.base.sif     = &weston_mouse_interface.base;
    mouse->buttons_state    = 0;
    mouse->b                = b;

    weston_seat_init_pointer (&b->core_seat);
    spice_server_add_interface (b->spice_server, &mouse->sin.base);
    b->mouse = mouse;
}
void
weston_spice_mouse_destroy (spice_backend_t *b)
{
    free (b->mouse);
}

//from xf86-video-qxl/src/spiceqxl_inputs.b
static uint8_t escaped_map[256] = {
    [0x1c] = 104, //KEY_KP_Enter,
    [0x1d] = 105, //KEY_RCtrl,
    [0x2a] = 0,//KEY_LMeta, // REDKEY_FAKE_L_SHIFT
    [0x35] = 106,//KEY_KP_Divide,
    [0x36] = 0,//KEY_RMeta, // REDKEY_FAKE_R_SHIFT
    [0x37] = 107,//KEY_Print,
    [0x38] = 108,//KEY_AltLang,
    [0x46] = 127,//KEY_Break,
    [0x47] = 110,//KEY_Home,
    [0x48] = 111,//KEY_Up,
    [0x49] = 112,//KEY_PgUp,
    [0x4b] = 113,//KEY_Left,
    [0x4d] = 114,//KEY_Right,
    [0x4f] = 115,//KEY_End,
    [0x50] = 116,//KEY_Down,
    [0x51] = 117,//KEY_PgDown,
    [0x52] = 118,//KEY_Insert,
    [0x53] = 119,//KEY_Delete,
    [0x5b] = 133,//0, // REDKEY_LEFT_CMD,
    [0x5c] = 134,//0, // REDKEY_RIGHT_CMD,
    [0x5d] = 135,//KEY_Menu,
};
#ifndef MIN_KEYCODE
# define MIN_KEYCODE 0;
#endif //MIN_KEYCODE

static void
weston_kbd_push_scan_frag (SpiceKbdInstance *sin, uint8_t frag)
{
    struct weston_spice_kbd *kbd = wl_container_of(sin, kbd, sin);
    struct spice_backend *b = kbd->b;
    enum wl_keyboard_key_state state;

    /*if (!b->core_seat.has_pointer) {
        return;
    }*/
    if (frag == 224) {
        kbd->escape = frag;
        return;
    }
    state = frag & 0x80 ? WL_KEYBOARD_KEY_STATE_RELEASED :
        WL_KEYBOARD_KEY_STATE_PRESSED;
    frag = frag & 0x7f;
    if (kbd->escape == 224) {
        kbd->escape = 0;
        if (escaped_map[frag] == 0) {
        }
        frag = escaped_map[frag] - 8;
    } else {
        frag += MIN_KEYCODE;
    }

    notify_key (&b->core_seat, weston_compositor_get_time(), frag,
                    state, STATE_UPDATE_AUTOMATIC );
}
static uint8_t
weston_kbd_get_leds (SpiceKbdInstance *sin)
{
#if 0
    struct weston_spice_kbd *kbd = wl_container_of(sin, kbd, sin);
    struct spice_backend *b = kbd->b;

    //TODO implement
#endif
    return 0;
}
static struct SpiceKbdInterface weston_kbd_interface = {
    .base.type          = SPICE_INTERFACE_KEYBOARD,
    .base.description   = "weston keyboard",
    .base.major_version = SPICE_INTERFACE_KEYBOARD_MAJOR,
    .base.minor_version = SPICE_INTERFACE_KEYBOARD_MINOR,

    .push_scan_freg     = weston_kbd_push_scan_frag,
    .get_leds           = weston_kbd_get_leds,
};
int
weston_spice_kbd_init (spice_backend_t *b)
{
    static int kbd_count = 0;
    struct weston_spice_kbd *kbd;

    if ( ++kbd_count > 1 ) {
        weston_log("Only one instance of kbd interface is suported");
        exit(1);
    }

    kbd = calloc (1, sizeof *kbd);
    kbd->sin.base.sif = &weston_kbd_interface.base;
    kbd->sin.st       = (SpiceKbdState*) kbd;
    kbd->b            = b;
    if(weston_seat_init_keyboard (&b->core_seat, NULL) < 0 ) {
        weston_log ("Failed to init seat keyboard");
        return -1;
    }
    spice_server_add_interface (b->spice_server, &kbd->sin.base);
    b->kbd = kbd;

    return 0;
}
void
weston_spice_kbd_destroy (spice_backend_t *b)
{
    free (b->kbd);
}
