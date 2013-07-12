## Scopeview

Scopeview is a Linux/GTK+ utility to read screen capture data from an Instek GDS-820C oscilloscope through its USB port.

![](https://github.com/windsorschmidt/scopeview/raw/master/screenshot.png)

## Notes

* Developed and tested using Linux (Debian and Arch). YMMV.

* To compile scopeview from the command line, use the make file or try this:

```gcc -o scopeview scopeview.c `pkg-config --cflags --libs libglade-2.0` -export-dynamic```

* Invoke scopeview using the device name of the scope, e.g.:

```./scopeview /dev/ttyUSB```

* Thanks to the creators of http://www.reconnsworld.com for the original python code which this code is based on.
