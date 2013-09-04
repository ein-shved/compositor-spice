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

#ifndef WESTON_QXL_COMMANDS_H
#define WESTON_QXL_COMMANDS_H

#include "compositor-spice.h"
#include "weston_spice_interfaces.h"

#define COLOR_RGB(r,g,b) \
    (((r)<<16) | ((g)<<8) | ((b)<<0) | 0xff000000)

#define COLOR_ARGB(a,r,g,b) \
    (((a)<<24) | ((r)<<16) | ((g)<<8) | ((b)<<0))

#define COLOR_RGB_F(r,g,b) \
    ( ( ((int) ((r)*255)) << 16) | ( ((int) ((g)*255)) <<  8) | \
      ( ((int) ((b)*255)) <<  0) | 0xff000000 )

#define COLOR_ARGB_F(a,r,g,b) \
    ( ( ((int) ((a)*255)) << 24) | ( ((int) ((r)*255)) << 16) | \
      ( ((int) ((g)*255)) <<  8) | ( ((int) ((b)*255)) <<  0) )

typedef uint32_t color_t;

uint32_t
spice_create_primary_surface (struct spice_compositor *c,
        int width, int height, uint8_t *data);

uint32_t
spice_create_image (struct spice_compositor *c);

int
spice_paint_image (struct spice_compositor *c, uint32_t image_id,
        int x, int y, int width, int wight,
        intptr_t data, int32_t stride, 
        pixman_region32_t *region);
#endif //WESTON_QXL_COMMANDS_H
