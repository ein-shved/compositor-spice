/*
 * Copyright © 2012 Intel Corporation
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
#include <stdint.h>
#include <sys/mman.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <cairo.h>

#define WCAP_HEADER_MAGIC	0x57434150

#define WCAP_FORMAT_XRGB8888	0x34325258
#define WCAP_FORMAT_XBGR8888	0x34324258
#define WCAP_FORMAT_RGBX8888	0x34325852
#define WCAP_FORMAT_BGRX8888	0x34325842

struct wcap_header {
	uint32_t magic;
	uint32_t format;
	uint32_t width, height;
};

struct wcap_frame_header {
	uint32_t msecs;
	uint32_t nrects;
};

struct wcap_rectangle {
	int32_t x1, y1, x2, y2;
};

struct wcap_decoder {
	int fd;
	size_t size;
	void *map, *p, *end;
	uint32_t *frame;
	int width, height;
};

static void
wcap_decoder_decode_rectangle(struct wcap_decoder *decoder,
			      struct wcap_rectangle *rect)
{
	uint32_t v, *p = decoder->p, *d;
	int width = rect->x2 - rect->x1, height = rect->y2 - rect->y1;
	int x, i, j, k, l, count = width * height;
	
	d = decoder->frame + (rect->y2 - 1) * decoder->width;
	x = rect->x1;
	i = 0;
	while (i < count) {
		v = *p++;
		l = v >> 24;
		if (l < 0xe0) {
			j = l + 1;
		} else {
			j = 1 << (l - 0xe0 + 7);
		}

		for (k = 0; k < j; k++) {
			d[x] = (d[x] + v) | 0xff000000;
			x++;
			if (x == rect->x2) {
				x = rect->x1;
				d -= decoder->width;
			}
		}
		i += j;
	}

	if (i != count)
		printf("rle encoding longer than expected (%d expected %d)\n",
		       i, count);

	decoder->p = p;
}

static int
wcap_decoder_get_frame(struct wcap_decoder *decoder)
{
	struct wcap_rectangle *rects;
	struct wcap_frame_header *header;
	uint32_t *s;
	uint32_t i;
	int width, height;

	if (decoder->p == decoder->end)
		return 0;

	header = decoder->p;

	rects = (void *) (header + 1);
	decoder->p = (uint32_t *) (rects + header->nrects);
	for (i = 0; i < header->nrects; i++) {
		width = rects[i].x2 - rects[i].x1;
		height = rects[i].y2 - rects[i].y1;
		wcap_decoder_decode_rectangle(decoder, &rects[i]);
	}

	return 1;
}

struct wcap_decoder *
wcap_decoder_create(const char *filename)
{
	struct wcap_decoder *decoder;
	struct wcap_header *header;
	int frame_size;
	struct stat buf;

	decoder = malloc(sizeof *decoder);
	if (decoder == NULL)
		return NULL;

	decoder->fd = open(filename, O_RDONLY);
	if (decoder->fd == -1)
		return NULL;

	fstat(decoder->fd, &buf);
	decoder->size = buf.st_size;
	decoder->map = mmap(NULL, decoder->size,
			    PROT_READ, MAP_PRIVATE, decoder->fd, 0);
		
	header = decoder->map;
	decoder->width = header->width;
	decoder->height = header->height;
	decoder->p = header + 1;
	decoder->end = decoder->map + decoder->size;

	frame_size = header->width * header->height * 4;
	decoder->frame = malloc(frame_size);
	memset(decoder->frame, 0, frame_size);

	return decoder;
}

void
wcap_decoder_destroy(struct wcap_decoder *decoder)
{
	munmap(decoder->map, decoder->size);
	free(decoder->frame);
	free(decoder);
}

static void
write_png(struct wcap_decoder *decoder, const char *filename)
{
	cairo_surface_t *surface;

	surface = cairo_image_surface_create_for_data((unsigned char *) decoder->frame,
						      CAIRO_FORMAT_ARGB32,
						      decoder->width,
						      decoder->height,
						      decoder->width * 4);
	cairo_surface_write_to_png(surface, filename);
	cairo_surface_destroy(surface);
}

int main(int argc, char *argv[])
{
	struct wcap_decoder *decoder;
	int i, output_frame;
	char filename[200];

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: wcap-decode WCAP_FILE [FRAME]\n");
		return 1;
	}			

	decoder = wcap_decoder_create(argv[1]);
	output_frame = -1;
	if (argc == 3)
		output_frame = strtol(argv[2], NULL, 0);

	i = 0;
	while (wcap_decoder_get_frame(decoder)) {
		if (i == output_frame) {
			snprintf(filename, sizeof filename,
				 "wcap-frame-%d.png", i);
			write_png(decoder, filename);
			printf("wrote %s\n", filename);
		}
		i++;
	}

	printf("wcap file: size %dx%d, %d frames\n",
	       decoder->width, decoder->height, i);

	wcap_decoder_destroy(decoder);
}