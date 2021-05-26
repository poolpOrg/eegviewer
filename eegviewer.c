/*
 * Copyright (c) 2019 Gilles Chehade <gilles@poolp.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/time.h>

#include <complex.h>
#include <err.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xcb/xcb.h>

extern char *__progname;

static void	usage(void);

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

static struct channel {
	uint16_t		values[16384];
	uint16_t		count;
	xcb_gcontext_t	gc;
} channels[6];
static int opt_channels = 0;

static uint32_t
rgb_pixel(const char *rgb)
{
        char buf[3] = { 0, 0, 0 };
        uint32_t r, g, b;

        r = strtol(memcpy(buf, rgb+1, 2), NULL, 16);
        g = strtol(memcpy(buf, rgb+3, 2), NULL, 16);
        b = strtol(memcpy(buf, rgb+5, 2), NULL, 16);

        return ((r << 16) + (g << 8) + b);
        

}

static void
init_channels(xcb_connection_t *c, xcb_window_t win)
{
	uint32_t	mask = XCB_GC_FOREGROUND;
	uint32_t	values[nitems(channels)] = {
		rgb_pixel("#0000ff"),
		rgb_pixel("#00ff00"),
		rgb_pixel("#ff0000"),
		rgb_pixel("#ff00ff"),
		rgb_pixel("#00ff00"),
		rgb_pixel("#335599"),
	};
	size_t	i;

	for (i = 0; i < nitems(channels); ++i) {
		channels[i].gc = xcb_generate_id(c);
		xcb_create_gc(c, channels[i].gc, win, mask, &values[i]);
	}
}

static void
channels_draw(xcb_connection_t *c, xcb_window_t win, uint16_t height, uint16_t width)
{
	size_t		i;
	size_t		j;
	uint16_t	offset;
	xcb_point_t	point[nitems(channels)][width];

	for (i = 0; i < nitems(channels); ++i) {
		for (j = 0; j < width; ++j) {
			offset = 0;
			if (channels[i].count >= width)
				offset = channels[i].count-width;

			if (channels[i].count >= width ||
				(channels[i].count < width && j < channels[i].count)) {
					if (j != 0) {
						point[i][j].x = 1;
						point[i][j].y = channels[i].values[offset+j] - channels[i].values[offset+j-1];
					}
					else {
						point[i][j].x = j;
						point[i][j].y = channels[i].values[offset+j];
					}
			}
		}
	}

	xcb_clear_area (c, 0, win, 0, 0, width, height);
	for (i = 0; i < nitems(channels); ++i) {
		if (opt_channels && (opt_channels & 1<<i)==0)
			continue;
		xcb_poly_line(c, XCB_COORD_MODE_PREVIOUS, win, channels[i].gc, width, point[i]);
	}
}

static void
push_line(char *line, ssize_t linelen)
{
	char	*p;
	char	*endp;
	const char *errstr;
	size_t	i;
	char	*values[6];

	p = line;

	/* skip cycle */
	if ((p = strchr(p, '|')) == NULL)
		return;

	/* skip version */
	if ((p = strchr(p+1, '|')) == NULL)
		return;
	p += 1;

	for (i = 0; i < nitems(values); ++i) {
		if ((endp = strchr(p, '|')) == NULL)
			return;
		values[i] = p;
		p = endp+1;
		*endp = '\0';
	}

	for (i = 0; i < nitems(channels); ++i) {
		if (channels[i].count == nitems(channels[i].values) - 1) {
			memmove(channels[i].values,
			    channels[i].values + 1,
			    sizeof (channels[i].values) - 1);
		}

		channels[i].values[channels[i].count] = strtonum(values[i], 0, 65536, &errstr);
		if (errstr)
			return;
		if (channels[i].count != nitems(channels[i].values) - 1) {
			channels[i].count++;
		}
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct pollfd		 pfd[2];
	xcb_connection_t	*c;
	xcb_screen_t		*screen;
	xcb_window_t		 win;
	uint32_t		 mask = 0;
	uint32_t		 values[2];

	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;

	xcb_get_geometry_cookie_t	cookie;
	xcb_get_geometry_reply_t	*geom;
	uint16_t			 width;
	uint16_t			 height;
	xcb_generic_event_t *e;

	int ch;
	int opt_height = -1;
	int opt_width = -1;

	while ((ch = getopt(argc, argv, "123456h:w:")) != -1) {
		switch (ch) {
			case '1':
				opt_channels |= 1<<0;
				break;
			case '2':
				opt_channels |= 1<<1;
				break;
			case '3':
				opt_channels |= 1<<2;
				break;
			case '4':
				opt_channels |= 1<<3;
				break;
			case '5':
				opt_channels |= 1<<4;
				break;
			case '6':
				opt_channels |= 1<<5;
				break;
			case 'h':
				opt_height = atoi(optarg);
				break;
			case 'w':
				opt_width = atoi(optarg);
				break;
			default:
				usage();
		}
	}

	c = xcb_connect(NULL, NULL);

	screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

	win = xcb_generate_id(c);
	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = screen->black_pixel;
	values[1] = XCB_EVENT_MASK_EXPOSURE;
	width = opt_width == -1 ? screen->width_in_pixels : opt_width;
	height = opt_height == -1 ? screen->height_in_pixels : opt_height;
	xcb_create_window (c,
	    XCB_COPY_FROM_PARENT,
	    win,
	    screen->root,
	    0, 0,
	    width,
	    height,
	    0,
	    XCB_WINDOW_CLASS_INPUT_OUTPUT,
	    screen->root_visual,
	    mask, values);
	xcb_map_window(c, win);
	xcb_flush(c);

	init_channels(c, win);

	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;

	pfd[1].fd = xcb_get_file_descriptor(c);
	pfd[1].events = POLLIN;

	while (poll(pfd, 2, 0) != -1) {
		if ((pfd[0].revents & (POLLIN|POLLHUP))) {
			linelen = getline(&line, &linesize, stdin);
			if (linelen == -1)
				pause();
			line[strcspn(line, "\n")] = '\0';
			push_line(line, linelen);
			goto redraw;
		}
		if ((pfd[1].revents & (POLLIN|POLLHUP))) {
			while ((e = xcb_poll_for_event(c))) {
				switch (e->response_type & ~0x80) {
				case XCB_EXPOSE:
					goto redraw;
				default:
					break;
				}
				free (e);
			}
		}
		continue;

redraw:
		cookie = xcb_get_geometry(c, win);
		geom = xcb_get_geometry_reply(c, cookie, NULL);
		width = geom->width;
		height = geom->height;
		free (geom);
		channels_draw(c, win, height, width);
		xcb_flush(c);
	}

	pause();
	err(1, NULL);

	return (0);
}
