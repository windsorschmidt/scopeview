## Scopeview

Scopeview is a Linux/GTK+ utility to read screen capture data from an Instek GDS-820C oscilloscope through it's USB port.

## Notes

* The GDS-820C uses an FTDI USB to serial chip, appearing to Linux as a tty. Communication with the scope is typical of serial devices (modems, etc.).

* Although the screen resolution on the oscillosope appears to be 320x240, data from the scope is padded as if the resolution was 320x256, leaving a 320x16 pixel portion of unused space at the bottom of the final image.

* Pixel data from the scope is stored 2 pixels/byte, 4 bits/pixel, for a maximum of 16 indexed colors/pixel, and a raster pitch of 128 bytes. This program converts pixel data to 8bpp RGB before writing it's output buffer.

* The image is sent from the scope in vertical rasters (hence the pitch at 128 bytes, instead of 160 bytes). This program rotates the image 90 degrees before writing it's output buffer.

* This software was developed and tested using Linux (Debian and Arch). YMMV.

* To compile scopeview from the command line, use the make file or try this:

    gcc -o scopeview scopeview.c `pkg-config --cflags --libs libglade-2.0` -export-dynamic

* Thanks to the creators of http://www.reconnsworld.com for the original python code which this code is based on.

## To Do

* Use XML instead of glade for GUI (or just hard code it?)

* User selectable serial port
