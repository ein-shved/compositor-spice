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
#define DEFAULT_MOVEMENT_STEP_DISTANCE wl_fixed_from_int(1)

/* TODO ledstate getting
 * TODO free mouse and kbd interfaces
 */
struct weston_spice_mouse {
    SpiceMouseInstance sin;
    uint32_t buttons_state;
    struct spice_compositor *c;
};

struct weston_spice_kbd {
    SpiceKbdInstance sin;
    uint8_t ledstate; 
    int escape;
    struct spice_compositor *c;
};

static void
weston_mouse_button_notify (struct spice_compositor *c,
        struct weston_spice_mouse* mouse, uint32_t buttons_state)
{
    uint32_t buttons = mouse->buttons_state;
    enum wl_pointer_button_state state;
    
    dprint(3, "buttons_state: %x, buttons: %x", 
            buttons_state, buttons);        
#define DELIVER_NOTIFY_BUTTON(mask, btn)\
    if ( !button_equal (buttons, buttons_state, mask ) ) { \
        state = buttons_state & mask ? \
                WL_POINTER_BUTTON_STATE_PRESSED : \
                WL_POINTER_BUTTON_STATE_RELEASED; \
        notify_button (&c->core_seat, weston_compositor_get_time(), \
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
    struct weston_spice_mouse *mouse = container_of(sin, struct weston_spice_mouse, sin);
    struct spice_compositor *c = mouse->c;

    dprint (3, "called. delta: (%d,%d,%d), buttons: %x",
            dx,dy,dz,buttons_state);
    /*if (!c->core_seat.has_pointer) {
        return;
    }
* look like it depricated
    */
    notify_motion(&c->core_seat, weston_compositor_get_time(),
            DEFAULT_MOVEMENT_STEP_DISTANCE*dx, 
            DEFAULT_MOVEMENT_STEP_DISTANCE*dy );
    if (dz) {
        notify_axis (&c->core_seat, weston_compositor_get_time(),
                WL_POINTER_AXIS_VERTICAL_SCROLL,
                dz*DEFAULT_AXIS_STEP_DISTANCE );
    }

    weston_mouse_button_notify (c, mouse, buttons_state);
}
static void 
weston_mouse_buttons (SpiceMouseInstance *sin, uint32_t buttons_state )
{
    struct weston_spice_mouse *mouse = container_of(sin, struct weston_spice_mouse, sin);
    struct spice_compositor *c = mouse->c;

    /*if (!c->core_seat.has_pointer) {
        return;
    }*/
    dprint (3, "called. Buttons: %x", buttons_state);
    weston_mouse_button_notify (c, mouse, buttons_state);
}

static struct SpiceMouseInterface weston_mouse_interface = {
    .base.type          = SPICE_INTERFACE_MOUSE,
    .base.description   = "weston mouse",
    .base.major_version = SPICE_INTERFACE_MOUSE_MAJOR,
    .base.minor_version = SPICE_INTERFACE_MOUSE_MINOR,

    .motion     = weston_mouse_motion,
    .buttons    = weston_mouse_buttons,
};

static char *weston_mouse_description = "weston mouse";
void 
weston_spice_mouse_init (spice_compositor_t *c)
{
    static int mouse_count = 0;
    struct weston_spice_mouse *mouse;

    if ( ++mouse_count > 1 ) {
        eprint("only one instance of mouse interface is suported");
        exit(1);
    }

    mouse = calloc (1, sizeof *mouse);
    mouse->sin.base.sif     = &weston_mouse_interface.base;
    mouse->buttons_state    = 0;
    mouse->c                = c;

    weston_seat_init_pointer (&c->core_seat);
    spice_server_add_interface (c->spice_server, &mouse->sin.base);
    c->mouse = mouse;
}
void 
weston_spice_mouse_destroy (spice_compositor_t *c)
{
    free (c->mouse);
}

//from xf86-video-qxl/src/spiceqxl_inputs.c
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
    struct weston_spice_kbd *kbd = container_of(sin, struct weston_spice_kbd, sin);
    struct spice_compositor *c = kbd->c;
    enum wl_keyboard_key_state state;
    
    /*if (!c->core_seat.has_pointer) {
        return;
    }*/
    if (frag == 224) {
        dprint (3, "escape called: %x", frag);
        kbd->escape = frag;
        return;
    }
    state = frag & 0x80 ? WL_KEYBOARD_KEY_STATE_RELEASED : 
        WL_KEYBOARD_KEY_STATE_PRESSED;
    frag = frag & 0x7f;
    if (kbd->escape == 224) {
        kbd->escape = 0;
        if (escaped_map[frag] == 0) {
            dprint(2, "escaped_map[%d] == 0\n", frag);
        }
        frag = escaped_map[frag] - 8;
    } else {
        frag += MIN_KEYCODE;
    }

    dprint (3, "called: %x", frag);

    notify_key (&c->core_seat, weston_compositor_get_time(), frag,
                    state, STATE_UPDATE_AUTOMATIC );
}
static uint8_t 
weston_kbd_get_leds (SpiceKbdInstance *sin) 
{
    struct weston_spice_kbd *kbd = container_of(sin, struct weston_spice_kbd, sin);
    struct spice_compositor *c = kbd->c;

    //TODO implement

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
weston_spice_kbd_init (spice_compositor_t *c)
{
    static int kbd_count = 0;
    struct weston_spice_kbd *kbd;

    if ( ++kbd_count > 1 ) {
        eprint("only one instance of kbd interface is suported");
        exit(1);
    }

    kbd = calloc (1, sizeof *kbd);
    kbd->sin.base.sif = &weston_kbd_interface.base;
    kbd->sin.st       = (SpiceKbdState*) kbd;
    kbd->c            = c;

    if ( weston_seat_init_keyboard (&c->core_seat, NULL) < 0 ) {
        dprint (1, "failed to init seat keyboard");
        return -1;
    }
    spice_server_add_interface (c->spice_server, &kbd->sin.base);

    c->kbd = kbd;

    return 0;
}
void
weston_spice_kbd_destroy (spice_compositor_t *c)
{
    free (c->kbd);
}
