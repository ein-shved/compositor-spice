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

#include <spice/qxl_dev.h>
#include <spice.h>
#include <spice/macros.h>

#include "weston_qxl_commands.h"
#include "compositor-spice.h"
#include "weston_spice_interfaces.h"

//TODO implement
typedef pixman_region32_t ClipList;

struct surface_create_cmd {
    struct spice_release_info base;
    QXLCommandExt ext;
    QXLSurfaceCmd cmd;
};
struct create_image_cmd {
    struct spice_release_info base;
    QXLCommandExt ext;
    QXLDrawable drawable;
    QXLImage image;
};
struct fill_cmd {
    struct spice_release_info base;
    QXLCommandExt ext;
    QXLDrawable drawable;
};

void release_simple (struct spice_release_info *base)
{
    free (base);
}

static void set_cmd(QXLCommandExt *ext, uint32_t type, QXLPHYSICAL data)
{
    ext->cmd.type = type;
    ext->cmd.data = data;
    ext->cmd.padding = 0;
    ext->group_id = MEMSLOT_GROUP;
    ext->flags = 0;
}
static void set_release_info(QXLReleaseInfo *info, intptr_t ptr)
{
    info->id = ptr;
    //info->group_id = MEMSLOT_GROUP;
}


static int surfaces_count = 0;
uint32_t
spice_create_primary_surface (struct spice_backend *b,
        int width, int height, uint8_t *data)
{
    QXLDevSurfaceCreate surface;

    assert (surfaces_count < NUM_SURFACES );
    assert (b->worker != NULL);
    assert (width > 0);
    assert (height > 0);

    surface.format     = SPICE_SURFACE_FMT_32_xRGB;
    surface.width      = width;
    surface.height     = height;
    surface.stride     = -width * 4;
    surface.mouse_mode = FALSE; /* unused by red_worker */
    surface.flags      = 0;
    surface.type       = 0;    /* unused by red_worker */
    surface.position   = 0;    /* unused by red_worker */
    surface.mem        = (uint64_t)data;
    surface.group_id   = MEMSLOT_GROUP;

    spice_qxl_create_primary_surface(&b->display_sin, surfaces_count, &surface);

    return surfaces_count++;
}
static void
fill_clip_data (QXLDrawable *drawable, ClipList *clip_rects)
{
    if (clip_rects == NULL /*|| clip_rects->num_rects == 0*/) {
        drawable->clip.type = SPICE_CLIP_TYPE_NONE;
    } /*else {
        QXLClipRects *cmd_clip;

        cmd_clip = calloc (sizeof(QXLClipRects) +
            clip_rects->num_rects * sizeof(QXLRect), 1);
        cmd_clip->num_rects = clip_rects->num_rects;
        cmd_clip->chunk.data_size = clip_rects->num_rects*sizeof(QXLRect);
        cmd_clip->chunk.prev_chunk = cmd_clip->chunk.next_chunk = 0;
        memcpy(cmd_clip + 1, clip_rects->ptr, cmd_clip->chunk.data_size);

        drawable->clip.type = SPICE_CLIP_TYPE_RECTS;
        drawable->clip.data = (intptr_t)cmd_clip;

        if (clip_rects->destroyable) {
            free(clip_rects->ptr);
        }
    }*/

   //TODO implement
}
static int
make_drawable (const QXLRect *bbox, uint32_t surface_id,
        ClipList *clip_rects, intptr_t release_info,
        intptr_t image, QXLDrawable *drawable, uint32_t mm_time)
{
    /*
    uint32_t bw = bbox->right - bbox->left;
    uint32_t bh = bbox->bottom - bbox->top;
    */
    drawable->surface_id = surface_id;
    drawable->bbox = *bbox;
    //FIXME: Next line is forced nail.
    drawable->mm_time = mm_time - 350;

    fill_clip_data (drawable, clip_rects);

    drawable->effect            = QXL_EFFECT_OPAQUE;
    set_release_info (&drawable->release_info, release_info);
    drawable->type              = QXL_DRAW_COPY;
    drawable->surfaces_dest[0]  = -1;
    drawable->surfaces_dest[1]  = -1;
    drawable->surfaces_dest[2]  = -1;

    drawable->u.copy.rop_descriptor     = SPICE_ROPD_OP_PUT;
    drawable->u.copy.src_bitmap         = (intptr_t)image;
    drawable->u.copy.src_area.top       = clip_rects->extents.y1;
    drawable->u.copy.src_area.left      = clip_rects->extents.x1;
    drawable->u.copy.src_area.right     = clip_rects->extents.x2;
    drawable->u.copy.src_area.bottom    = clip_rects->extents.y2;

    return 0;
}

static uint32_t image_counter = 0;

uint32_t
spice_create_image (struct spice_backend *b)
{
    uint32_t image_id;

    image_id = ++image_counter;
    return image_id;
}

int
spice_paint_image (struct spice_backend *b, uint32_t image_id,
        int x, int y, int width, int height,
        intptr_t data, int32_t stride, pixman_region32_t *damage )
{
    struct create_image_cmd *cmd;
    QXLImage *image;
    QXLDrawable *drawable;
    uint32_t surface_id = spice_get_primary_surface_id(b);
    QXLRect bbox = {
        .left = x,
        .right = x + width,
        .top = y,
        .bottom = y + height,
    };

    cmd = calloc (sizeof *cmd, 1);
    if ( cmd == NULL ) {
        goto err_cmd_malloc;
    }
    drawable = &cmd->drawable;
    image = &cmd->image;
    cmd->base.destructor = release_simple;

    if ( make_drawable (&bbox, surface_id, damage,
            (intptr_t) cmd, (intptr_t) image, drawable,
            b->mm_clock) < 0 )
    {
        goto err_drawable;
    }

    QXL_SET_IMAGE_ID (image, QXL_IMAGE_GROUP_DEVICE, image_id);

    image->descriptor.type      = SPICE_IMAGE_TYPE_BITMAP;
    image->descriptor.flags     = 0;
    image->descriptor.width     = image->bitmap.x = width;
    image->descriptor.height    = image->bitmap.y = height;

    image->bitmap.data          = data;
    image->bitmap.flags         = QXL_BITMAP_DIRECT | QXL_BITMAP_TOP_DOWN;
    image->bitmap.stride        = stride;
    image->bitmap.palette       = 0;
    image->bitmap.format        = SPICE_BITMAP_FMT_32BIT;

    set_cmd (&cmd->ext, QXL_CMD_DRAW, (intptr_t)drawable);

    b->push_command (b, &cmd->ext);

    return 0;

err_drawable:
    free (cmd);
err_cmd_malloc:
    return -1;
}
