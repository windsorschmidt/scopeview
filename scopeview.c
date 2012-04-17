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
int rval, read_chunk, read_total, byte_cnt, ser_fd;
unsigned char input_buffer[INPUT_SIZE];
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

// prototypes
gboolean refresh_capture(void);
void debug_msg(char *msg, int debug_level);

// main: program entry point
int main(int argc, char *argv[])
{
  gtk_init(&argc, &argv);
  
  // load GUI
  xml = glade_xml_new("/usr/share/scopeview/scopeview.glade", NULL, NULL);
  if(!xml)
    {
      xml = glade_xml_new("/usr/local/share/scopeview/scopeview.glade", NULL, NULL);
      if(!xml)
	{
	  xml = glade_xml_new("scopeview.glade", NULL, NULL);
	  if(!xml)
	    {
	      debug_msg("couldn't parse GUI description", DEBUG_ERROR);
	      gtk_main_quit();
	    }
	}
    }

  // set up GUI signals
  glade_xml_signal_autoconnect(xml);
  
  // initialize serial port
  ser_fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
  if (ser_fd == -1)
  {
    debug_msg("couldn't open serial port", DEBUG_ERROR);
    gtk_main_quit();
  }


  // create pixel buffer
  pixbuf = (GdkPixbuf *)gdk_pixbuf_new(GDK_COLORSPACE_RGB,
				       FALSE,
				       8,
				       OUTPUT_WIDTH,
				       OUTPUT_HEIGHT);
  if(!pixbuf)
    {
      debug_msg("couldn't create pixbuf", DEBUG_ERROR);
      gtk_main_quit();
    }

  // set up screen refresh callback
  g_timeout_add (UPDATE_PERIOD, (void *)refresh_capture, 0);

  gtk_main();

  // close serial port
  close(ser_fd);
  return 0;
}

// display a new image from the scope
gboolean refresh_capture(void)
{
  // send screen capture request to scope
  rval = write(ser_fd, "W\0\0\n", 4);
  if (rval<0)
    {
      debug_msg("couldn't write to tty", DEBUG_ERROR);
    }

  // get time stamp to avoid waiting on serial port
  gettimeofday(&timestruct, NULL);
  t1=timestruct.tv_sec;

  // read screen capture data to input buffer
  read_total = 0;
  while(read_total < INPUT_SIZE)
    {
      // read from serial port
      read_chunk = read(ser_fd, &input_buffer[read_total], INPUT_SIZE);
      read_total += read_chunk;

      // update time stamp
      gettimeofday(&timestruct, NULL);
      t2=timestruct.tv_sec;

      // break if we time out
      if((t2-t1) > 1) break;
    }

  // get pointer to our output pixel buffer (since we're using a GdkPixbuf)
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  // unpack input buffer data to output buffer
  for(byte_cnt=0; byte_cnt<INPUT_SIZE; byte_cnt++)
    {
      // process output pixels 2 at a time, based on the current input byte.
      // each nibble of the input byte is used to index a color lookup table
      // and retrieve RGB values which are assigned to separatate output bytes.
      //
      // because the input image is rotated -90 degrees from normal viewing
      // orientation, some division and modulus operators are used to save the
      // pixel data in the output buffer rotated by 90 degrees.

      in_byte = input_buffer[byte_cnt];

      // set up to save output data rotated by 90 degrees
      row = byte_cnt%128;
      col = (INPUT_WIDTH-1)-byte_cnt/128;
      if(byte_cnt%128 < 120) // skip the last 8 rows of the image
	{
	  // save pixel 1 of this input byte
	  pixels[(320*3*(row*2))+(3*col)]=colors[((in_byte >> 4) & 0x0f)].r;
	  pixels[(320*3*(row*2))+(3*col)+1]=colors[((in_byte >> 4) & 0x0f)].g;
	  pixels[(320*3*(row*2))+(3*col)+2]=colors[((in_byte >> 4) & 0x0f)].b;
	  // save pixel 2 of this input byte
	  pixels[(320*3*(row*2+1))+(3*col)]=colors[(in_byte & 0x0f)].r;
	  pixels[(320*3*(row*2+1))+(3*col)+1]=colors[(in_byte & 0x0f)].g;
	  pixels[(320*3*(row*2+1))+(3*col)+2]=colors[(in_byte & 0x0f)].b;
	}
    }

  // update image widget with new image buffer data
  gtk_image_set_from_pixbuf((GtkImage *)glade_xml_get_widget(xml, "capture"), pixbuf);

  return TRUE;
}


void on_quit_activate(GtkMenuItem *menuitem, gpointer user_data)
{
  gtk_main_quit();
}


gboolean on_drawingarea1_expose_event(GtkWidget *widget, GdkEventExpose  *event, gpointer user_data)
{
  return FALSE;
}


void on_window1_destroy(GtkObject *object, gpointer user_data)
{
  gtk_main_quit();
}

void debug_msg(char *msg, int debug_level)
{
  if (debug_level >= DEBUG_LEVEL)
    {
      printf("%s\n",msg);
    }
}
