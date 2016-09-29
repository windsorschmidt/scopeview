/*
 * About : Capture LCD display data from an Instek GDS-820C oscilloscope.
 *
 * Notes :
 *
 * The GDS-820C uses an FTDI USB to serial chip, appearing to Linux as a tty.
 *
 * Although the screen resolution on the oscillosope appears to be 320x240,
 * data from the scope is padded as if the resolution was 320x256, leaving a
 * 320x16 pixel portion of unused space at the bottom of the final image.
 *
 * Pixel data from the scope is stored 2 pixels/byte, 4 bits/pixel, for a
 * maximum of 16 indexed colors/pixel, and a raster pitch of 128 bytes. This
 * program converts pixel data to 8bpp RGB before writing it's output buffer.
 *
 * The image is sent from the scope in vertical rasters (hence the pitch
 * at 128 bytes, instead of 160 bytes). This program rotates the image 90
 * degrees before writing it's output buffer.
 *
 * Compile using the makefile or try:
 *
 * gcc -o scopeview scopeview.c `pkg-config --cflags --libs libglade-2.0` \
 * -export-dynamic
 *
 * Thanks to the creators of http://www.reconnsworld.com for the original
 * python code which this code is based on.
 *
 * Copyright 2014 Windsor Schmidt. Use it if it's useful.
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>

#define INPUT_WIDTH 320  /* scope gives us 320 pixels per row */
#define UPDATE_PERIOD 250  /* milliseconds between polling scope */
#define RX_TIMEOUT 200000
#define SCREEN_DUMP_SIZE 40960

uint8_t acquire_scope_buffer(uint8_t * buffer);

typedef struct {
    unsigned char r, g, b;
} rgb_color;

GdkPixbuf * pixbuf;
guchar *pixels;
int byte_cnt, console_fd;

/* Original colors from LCD display */
rgb_color colors_orig[] = {
    {0x00, 0x00, 0x00},  /* Menu text                        */
    {0x00, 0x00, 0x00},  /* Trace background                 */
    {0xff, 0xff, 0x00},  /* Channel-1 trace/info             */
    {0x80, 0x80, 0x80},  /* Unknown                          */
    {0x00, 0xff, 0xff},  /* Channel-2 trace/info             */
    {0x80, 0x80, 0x80},  /* Unknown                          */
    {0x66, 0xff, 0x66},  /* Horiz./trigger info/markers      */
    {0xff, 0xff, 0xff},  /* GUI text and borders             */
    {0x88, 0x88, 0x88},  /* Trace reticle, menu shadow       */
    {0x80, 0x80, 0x80},  /* Unknown                          */
    {0x00, 0x00, 0x55},  /* GUI background                   */
    {0xbb, 0xbb, 0xbb},  /* Menu background                  */
    {0x80, 0x80, 0x80},  /* Unknown                          */
    {0x80, 0x80, 0x80},  /* Unknown                          */
    {0xff, 0x22, 0x22},  /* Math trace/info, logo background */
    {0xff, 0xff, 0xff}}; /* Menu highlight                   */

/* Happy colors with a white background */
rgb_color colors_light[] = {
    {0x55, 0x56, 0x50},  /* Menu text                        */
    {0xf9, 0xf8, 0xf5},  /* Trace background                 */
    {0xf9, 0x26, 0x72},  /* Channel-1 trace/info             */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0x46, 0xa9, 0xdf},  /* Channel-2 trace/info             */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0x86, 0xd2, 0x1e},  /* Horiz./trigger info/markers      */
    {0x55, 0x56, 0x50},  /* GUI text and borders             */
    {0xa5, 0xa1, 0xae},  /* Trace reticle, menu shadow       */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0xf8, 0xf8, 0xf2},  /* GUI background                   */
    {0xf8, 0xf8, 0xf2},  /* Menu background                  */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0xf4, 0xbf, 0x35},  /* Math trace/info, logo background */
    {0xf9, 0xf8, 0xf5}}; /* Menu highlight                   */

/* Darker colors based on https://github.com/morhetz/gruvbox-generalized */
rgb_color colors_dark[] = {
    {0x1d, 0x1c, 0x1a},  /* Menu text                        */
    {0x1d, 0x1c, 0x1a},  /* Trace background                 */
    {0xd7, 0x99, 0x21},  /* Channel-1 trace/info             */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0x45, 0x85, 0x88},  /* Channel-2 trace/info             */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0xb8, 0xbb, 0x26},  /* Horiz./trigger info/markers      */
    {0xa8, 0x99, 0x84},  /* GUI text and borders             */
    {0x92, 0x83, 0x74},  /* Trace reticle, menu shadow       */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0x32, 0x30, 0x2f},  /* GUI background                   */
    {0xa8, 0x99, 0x84},  /* Menu background                  */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0x80, 0x00, 0x80},  /* Unknown                          */
    {0xfb, 0x49, 0x34},  /* Math trace/info, logo background */
    {0xeb, 0xdb, 0xb2}}; /* Menu highlight                   */

/* Black and white, for printing */
rgb_color colors_mono[] = {
    {0x00, 0x00, 0x00},  /* Menu text                        */
    {0xff, 0xff, 0xff},  /* Trace background                 */
    {0x00, 0x00, 0x00},  /* Channel-1 trace/info             */
    {0xff, 0xff, 0xff},  /* Unknown                          */
    {0x00, 0x00, 0x00},  /* Channel-2 trace/info             */
    {0xff, 0xff, 0xff},  /* Unknown                          */
    {0x00, 0x00, 0x00},  /* Horiz./trigger info/markers      */
    {0x00, 0x00, 0x00},  /* GUI text and borders             */
    {0x00, 0x00, 0x00},  /* Trace reticle, menu shadow       */
    {0xff, 0xff, 0xff},  /* Unknown                          */
    {0xff, 0xff, 0xff},  /* GUI background                   */
    {0xff, 0xff, 0xff},  /* Menu background                  */
    {0xff, 0xff, 0xff},  /* Unknown                          */
    {0xff, 0xff, 0xff},  /* Unknown                          */
    {0x00, 0x00, 0x00},  /* Math trace/info, logo background */
    {0xff, 0xff, 0xff}}; /* Menu highlight                   */

#define COLOR_THEME_COUNT 3
static rgb_color *color_themes[] = {colors_dark, colors_light, colors_mono};
static int theme = 0;
static int row;
static int col;
static unsigned char in_byte;
uint8_t buffer[SCREEN_DUMP_SIZE];
fd_set set;
struct timeval timeout;
const uint8_t msg[] = { 0x57, 0x00, 0x00, 0x0A } ; /* screen capture request */
int rv;
int console_fd;
gint win_w, win_h;

GtkBuilder *builder;
GtkWidget *window;
GtkWidget *image_scope;
GtkWidget *button_exit;
GtkWidget *button_pause;
GdkPixbuf * pixbuf_scope;
GdkPixbuf * pixbuf_scaled;
guchar * scope_pixels;

static gboolean redraw_timer_handler(GtkWidget *widget) {
    scope_pixels = gdk_pixbuf_get_pixels(pixbuf_scope);

    if (acquire_scope_buffer(buffer)) { return TRUE; }

    /* unpack input buffer data to output buffer */
    for(byte_cnt=0; byte_cnt<SCREEN_DUMP_SIZE; byte_cnt++)
	{
	    /*
	     * process output pixels 2 at a time, based on the current input byte.
	     * each nibble of the input byte is used to index a color lookup table
	     * and retrieve RGB values which are assigned to separatate output bytes.

	     * because the input image is rotated -90 degrees from normal viewing
	     * orientation, some division and modulus operators are used to save the
	     * pixel data in the output buffer rotated by 90 degrees.
	     */
	    in_byte = buffer[byte_cnt];

	    /* set up to save output data rotated by 90 degrees */
	    row = byte_cnt%128;
	    col = (INPUT_WIDTH-1)-byte_cnt/128;
	    if(byte_cnt%128 < 120) // skip the last 8 rows of the image
		{
		    /* save pixel 1 of this input byte */
		    scope_pixels[(320*3*(row*2))+(3*col)] =
			color_themes[theme][((in_byte >> 4) & 0x0f)].r;
		    scope_pixels[(320*3*(row*2))+(3*col)+1] =
			color_themes[theme][((in_byte >> 4) & 0x0f)].g;
		    scope_pixels[(320*3*(row*2))+(3*col)+2] =
			color_themes[theme][((in_byte >> 4) & 0x0f)].b;
		    /* save pixel 2 of this input byte */
		    scope_pixels[(320*3*(row*2+1))+(3*col)] =
			color_themes[theme][(in_byte & 0x0f)].r;
		    scope_pixels[(320*3*(row*2+1))+(3*col)+1] =
			color_themes[theme][(in_byte & 0x0f)].g;
		    scope_pixels[(320*3*(row*2+1))+(3*col)+2] =
			color_themes[theme][(in_byte & 0x0f)].b;
		}
	}

    pixbuf_scaled = gdk_pixbuf_scale_simple (pixbuf_scope, win_w, win_h,
					     GDK_INTERP_NEAREST);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image_scope), pixbuf_scaled);
    g_object_unref(pixbuf_scaled);
    gtk_widget_queue_draw(window);
    return TRUE;
}

gboolean on_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data ) {
    win_w = event->width;
    win_h = event->height;
    gtk_widget_queue_draw(window);
    return FALSE;
}

void on_window_destroy () {
    gtk_main_quit();
}

gboolean key_event(GtkWidget *widget, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_space) {
	theme = (theme + 1) % COLOR_THEME_COUNT;
    }
    return FALSE;
}

uint8_t gui_init(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "scopeview.glade", NULL);
    window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    image_scope = GTK_WIDGET(gtk_builder_get_object(builder, "image_scope"));
    gtk_builder_connect_signals(builder, NULL);
    pixbuf_scope = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 320, 240);

    /* enable timers */
    g_timeout_add(UPDATE_PERIOD, (GSourceFunc) redraw_timer_handler,
		  (gpointer) window);

    /* set up drawing callback */
    g_signal_connect(G_OBJECT(window), "configure-event",
		     G_CALLBACK(on_configure), 0 );

    g_signal_connect(window, "key-press-event", G_CALLBACK(key_event), NULL);
    return 0;
}

int serial_init(const char * dev) {
    struct termios tio;
    int console_fd;

    memset(&tio, 0, sizeof(tio));
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CREAD | CLOCAL; /* 8N1, see termios.h for more info */
    tio.c_lflag = 0;

    console_fd = open(dev, O_RDWR);
    if (console_fd == -1) {
	return 0; /* error */
    }

    cfsetospeed(&tio, B1200);
    cfsetispeed(&tio, B1200);

    tcsetattr(console_fd, TCSANOW, &tio);

    return console_fd;
}

uint8_t acquire_scope_buffer(uint8_t * buffer) {
    uint8_t * buffer_index = &buffer[0];
    uint8_t temp_buffer[64];
    int rval;
    int total = 0;

    /* request data */
    timeout.tv_sec = 0;
    timeout.tv_usec = RX_TIMEOUT;
    FD_SET(console_fd, &set);
    write(console_fd, &msg, 4);

    while (1) {
	rv = select(console_fd + 1, &set, NULL, NULL, &timeout);
	if (rv == -1) {
	    /* error? */
	    return 1;
	} else if (rv == 0) {
	    /* timeout */
	    return 1;
	} else {
	    rval = read(console_fd, &temp_buffer, 64);
	    if (rval > 0) {
		total += rval;
		if (total <= SCREEN_DUMP_SIZE) {
		    memcpy(buffer_index, &temp_buffer, rval);
		    buffer_index += rval;
		} else {
		    printf(">> overflow: last rval=%d, bytes total=%d\n", rval, total);
		    return 1;
		}
		if (total == SCREEN_DUMP_SIZE) {
		    /* just the exact amount of data we wanted */
		    return 0;
		}
	    }
	}
    }
}

int main(int argc, char *argv[]) {
    /* initialize serial port */
    console_fd = serial_init(argv[1]);
    if (!console_fd) {
	printf ("error opening serial port\n");
	return 1;
    }

    /* initialize user interface */
    if (gui_init(argc, argv)) {
	printf ("error setting up gui\n");
	return 1;
    }

    /* show main window and enter main loop */
    gtk_widget_show(window);
    gtk_main();

    /* clean up and exit */
    g_object_unref(G_OBJECT(builder));
    close(console_fd);
    return 0;
}
