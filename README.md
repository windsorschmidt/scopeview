## Scopeview

Scopeview is a Linux/GTK+ utility to read screen capture data from an Instek GDS-820C oscilloscope through it's USB port.

![](https://github.com/honeyclaw/scopeview/raw/master/screenshot.png)

## Notes

* This software was developed and tested using Linux (Debian and Arch). YMMV.

* To compile scopeview from the command line, use the make file or try this:
```
gcc -o scopeview scopeview.c `pkg-config --cflags --libs libglade-2.0` -export-dynamic

```
* Invoke scopeview using the device name of the scope, e.g.:

```
./scopeview /dev/ttyUSB0
```

* Thanks to the creators of http://www.reconnsworld.com for the original python code which this code is based on.

## To Do

* Hard-code GUI and ditch the XML layout file?

* keyboard controls for things like double-size, colors, etc.
