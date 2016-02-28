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

#ifndef _WESTON_SPICE_INTERFACES_
#define _WESTON_SPICE_INTERFACES_

#include "compositor-spice-conf.h"

struct spice_release_info {
    void (*destructor) (struct spice_release_info *);
};

typedef struct spice_backend spice_backend_t;
typedef struct weston_spice_mouse weston_spice_mouse_t;
typedef struct weston_spice_kbd weston_spice_kbd_t;
typedef struct weston_spice_qxl weston_spice_qxl_t;

void weston_spice_qxl_init (spice_backend_t *qxl);
void weston_spice_mouse_init (spice_backend_t *c);
int weston_spice_kbd_init (spice_backend_t *c);

void weston_spice_qxl_destroy (spice_backend_t *c);
void weston_spice_mouse_destroy (spice_backend_t *c);
void weston_spice_kbd_destroy (spice_backend_t *c);

void release_simple (struct spice_release_info *);

#endif //_WESTON_QXL_INTERFACE_
