## About

A utility to view the display of an Instek GDS-820C oscilloscope via USB in near-realtime (tm).

![Screenshot](screenshot_mono.png?raw=true "Screenshot")                                           

## Requirements

Requies GTK and Glade libraries.

### Compiling

Use the included Makefile or try:

```gcc -o scopeview scopeview.c `pkg-config --cflags --libs gtk+-3.0` -export-dynamic```

### Usage

```scopeview <serial-device>```

e.g. ```./scopeview /dev/ttyUSB1```

- Switch between color themes with <kbd>space</kbd>.

### Notes

This is quick and dirty code, tested only in (Arch) Linux. Based on a similar python implementation from http://www.reconnsworld.com.
