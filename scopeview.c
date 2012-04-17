///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2008 by Windsor Schmidt
// <windsor.schmidt@gmail.com>
// http://www.windsorschmidt.com
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
// Free Software Foundation, Inc.,
// 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////
//
// Name : scopeview.c
//
// Version : 1.0
//
// Description : Read screen capture data from an Instek GDS-820C oscilloscope
// through it's USB port.
//
// Notes :
//
// 1. The GDS-820C uses an FTDI USB to serial chip, appearing to Linux as a tty.
// Communication with the scope is typical of serial devices (modems, etc.).
//
// 2. Although the screen resolution on the oscillosope appears to be 320x240,
// data from the scope is padded as if the resolution was 320x256, leaving a
// 320x16 pixel portion of unused space at the bottom of the final image.
//
// 3. Pixel data from the scope is stored 2 pixels/byte, 4 bits/pixel, for a
// maximum of 16 indexed colors/pixel, and a raster pitch of 128 bytes. This
// program converts pixel data to 8bpp RGB before writing it's output buffer.
//
// 4. The image is sent from the scope in vertical rasters (hence the pitch
// at 128 bytes, instead of 160 bytes). This program rotates the image 90
// degrees before writing it's output buffer.
//
// 5. This software was tested using Linux (Ubuntu 8.10).
//
// 6. To compile scopeview from the command line, try this:
// gcc -o scopeview scopeview.c `pkg-config --cflags --libs libglade-2.0` -export-dynamic
//
// 7. Thanks to the creators of http://www.reconnsworld.com for the original
// python code which this code is based on.
//
///////////////////////////////////////////////////////////////////////////////

// includes
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>

// defines
#define DEBUG_LEVEL 1 // dont bug us unless an error is at least this severe
#define DEBUG_INFO 1
#define DEBUG_WARN 2
#define DEBUG_ERROR 3
#define DEBUG_CATASTROPHIC_FAILURE 4
#define SERIAL_PORT "/dev/ttyUSB0"
#define INPUT_WIDTH 320 // scope gives us 320 pixels per row
#define INPUT_HEIGHT 128 // scope gives us 256 pixels per column
#define OUTPUT_WIDTH 320
#define OUTPUT_HEIGHT 240
#define INPUT_SIZE INPUT_WIDTH*INPUT_HEIGHT
#define UPDATE_PERIOD 200 // milliseconds between polling scope

#define RX_TIMEOUT 200000
#define DEFAULT_TTY "/dev/ttyUSB0"
#define SCREEN_DUMP_SIZE 40960

uint8_t acquire_scope_buffer(uint8_t * buffer);
//timer->invalidate->update->draw->ac_pixbuf->ac_buffer

guchar *pixels;

// structures
typedef struct {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} rgb_color;

// global variables
GladeXML *xml;
GdkPixbuf * pixbuf;
guchar *pixels;
int read_chunk, read_total, byte_cnt, console_fd;
rgb_color colors[] = {
  {0,0,0},
  {0,0,0},
  {255,255,0},
  {48,48,0},
  {0,255,255},
  {0,48,48},
  {102,255,102},
  {255,255,255},
  {136,136,136},
  {255,0,0},
  {0,0,85},
  {187,187,187},
  {0,128,0},
  {128,0,0},
  {255,34,34},
  {255,255,255}};
static int row;
static int col;
static unsigned char in_byte;
struct timeval timestruct;
double t1, t2;

uint8_t buffer[SCREEN_DUMP_SIZE];
uint16_t total;
uint8_t rval;
fd_set set;
struct timeval timeout;
const uint8_t msg[] = { 0x57, 0x00, 0x00, 0x0A } ; // hey, send us back a screenshot!
  int rv;
int console_fd;

GtkBuilder *builder;
GtkWidget *window;
GtkWidget *image_scope;
GtkWidget *button_exit;
GtkWidget *button_pause;
GdkPixbuf * pixbuf_scope;

static gboolean redraw_timer_handler(GtkWidget *widget) {
  gtk_image_set_from_pixbuf(GTK_IMAGE(image_scope), pixbuf_scope);
  return TRUE;
}

static gboolean on_scope_expose( GtkWidget *widget, GdkEventExpose *event, gpointer user_data ) {
  guchar * scope_pixels = gdk_pixbuf_get_pixels(pixbuf_scope);

  acquire_scope_buffer(buffer);

  // unpack input buffer data to output buffer
  for(byte_cnt=0; byte_cnt<SCREEN_DUMP_SIZE; byte_cnt++)
    {
      // process output pixels 2 at a time, based on the current input byte.
      // each nibble of the input byte is used to index a color lookup table
      // and retrieve RGB values which are assigned to separatate output bytes.
      //
      // because the input image is rotated -90 degrees from normal viewing
      // orientation, some division and modulus operators are used to save the
      // pixel data in the output buffer rotated by 90 degrees.

      in_byte = buffer[byte_cnt];

      // set up to save output data rotated by 90 degrees
      row = byte_cnt%128;
      col = (INPUT_WIDTH-1)-byte_cnt/128;
      if(byte_cnt%128 < 120) // skip the last 8 rows of the image
	{
	  // save pixel 1 of this input byte
	  scope_pixels[(320*3*(row*2))+(3*col)]=colors[((in_byte >> 4) & 0x0f)].r;
	  scope_pixels[(320*3*(row*2))+(3*col)+1]=colors[((in_byte >> 4) & 0x0f)].g;
	  scope_pixels[(320*3*(row*2))+(3*col)+2]=colors[((in_byte >> 4) & 0x0f)].b;
	  // save pixel 2 of this input byte
	  scope_pixels[(320*3*(row*2+1))+(3*col)]=colors[(in_byte & 0x0f)].r;
	  scope_pixels[(320*3*(row*2+1))+(3*col)+1]=colors[(in_byte & 0x0f)].g;
	  scope_pixels[(320*3*(row*2+1))+(3*col)+2]=colors[(in_byte & 0x0f)].b;
	}
    }

  return FALSE;
}

void on_window_destroy (GtkObject * object, gpointer user_data) {
    gtk_main_quit();
}

uint8_t init_gui(int argc, char *argv[]) {
  return 0;
}

int serial_init(const char * dev) {
  struct termios tio;
  int console_fd;

  memset(&tio, 0, sizeof(tio));
  tio.c_iflag = 0;
  tio.c_oflag = 0;
  tio.c_cflag = CS8 | CREAD | CLOCAL; // 8N1, see termios.h for more information
  tio.c_lflag = 0;

  console_fd = open(dev, O_RDWR);
  if (console_fd == -1) {
    return 0; // error
  }

  cfsetospeed(&tio, B1200);
  cfsetispeed(&tio, B1200);

  tcsetattr(console_fd, TCSANOW, &tio);

  return console_fd;
}

void pump(void) {
  total = 0;
  timeout.tv_sec = 0;
  timeout.tv_usec = RX_TIMEOUT;
  FD_SET(console_fd, &set);
  write(console_fd, &msg, 4);
}

uint8_t acquire_scope_buffer(uint8_t * buffer) {
  pump();
  uint8_t * buffer_index;
  buffer_index = &buffer[0];

  while (1) {
    rv = select(console_fd + 1, &set, NULL, NULL, &timeout);
    if(rv == -1) {
      // error?
      return 1;
    } else if (rv == 0) {
      // timeout
      pump();
    } else {
      rval = read(console_fd, buffer_index, 10);
      total += rval;
      if (rval > 0) {
        if (buffer_index-buffer < 40960) {
          buffer_index += rval;
        }
      }
      if (total == 40960) {
        return 0;
      }
      if (total >= 40960) {
        printf(">> rval: %d, total: %d\n", rval, total);
        return 1;
      }
    }
  }
}

// main: program entry point
int main(int argc, char *argv[])
{
  // initialize serial port
  console_fd = serial_init(argv[1]);
  if (!console_fd) {
    printf ("error opening serial port\n");
    return 1;
  }

  init_gui(argc, argv);

  gtk_init(&argc, &argv);
  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "scopeview.xml", NULL);
  window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
  image_scope = GTK_WIDGET(gtk_builder_get_object(builder, "image_scope"));
  gtk_builder_connect_signals(builder, NULL);

  pixbuf_scope = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 320, 240);

  // enable timers
  g_timeout_add(250, (GSourceFunc) redraw_timer_handler, (gpointer) window);

  // set up send message notebook page
  //GtkWidget * sendmsg_period = GTK_WIDGET(gtk_builder_get_object(builder, "sendmsg_period"));

  // set up drawing callback
  g_signal_connect(G_OBJECT(image_scope), "expose-event", G_CALLBACK(on_scope_expose), 0 );

  // show main window
  //g_object_unref(G_OBJECT(builder));
  gtk_widget_show(window);

  //////
  FD_ZERO(&set); /* clear the set */
  ///////

  gtk_main();

  // close serial port
  close(console_fd);
  return 0;
}

