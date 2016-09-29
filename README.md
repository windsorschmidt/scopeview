## Scopeview for Instek GDS-820C

Scopeview is a utility to read display buffer data from the LCD of an Instek GDS-820C oscilloscope through its USB port.

<p>
  <img style="border:1px solid #222222; margin-right:1em;}" src="https://github.com/windsorschmidt/scopeview/raw/master/screenshot_dark.png">
  <img style="border:1px solid #222222; margin-right:1em;}" src="https://github.com/windsorschmidt/scopeview/raw/master/screenshot_light.png">
  <img style="border:1px solid #222222; margin-right:1em;}" src="https://github.com/windsorschmidt/scopeview/raw/master/screenshot_mono.png">
  <img style="border:1px solid #222222; margin-right:1em;}" src="https://github.com/windsorschmidt/scopeview/raw/master/screenshot_orig.png">
</p>

### Compiling

Use the included Makefile or try:

```gcc -o scopeview scopeview.c `pkg-config --cflags --libs gtk+-3.0` -export-dynamic```

### Usage

```scopeview <serial-device>``

e.g. ```./scopeview /dev/ttyUSB1```

## Notes

This is quick and dirty code, tested only in Linux (Debian and Arch). Requies GTK and Glade libraries. Based on a similar python implementation from http://www.reconnsworld.com
