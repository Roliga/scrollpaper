ScrollPaper
===========

Lets you drag your wallpaper horizontally or vertically depending on its aspect ratio. Too wide for your screen = drag horizontally, too tall for your screen = drag vertically.

Features:

- Scales wallpaper to fit your screen
- Configure wallpaper by monitor name
- Multi-monitor support
- Uses no CPU/GPU while idle (good for laptops)

Installation
------------

Make sure the following dependencies are installed:

- [SFML](https://www.sfml-dev.org/)
- [libconfig](https://hyperrealm.github.io/libconfig/)
- [libx11](https://xorg.freedesktop.org/)
- [libxrandr](https://xorg.freedesktop.org/)
- [glibc](http://www.gnu.org/software/libc/)

Then to build and install ScrollPaper just do:

	make
	make install

Configuration
-------------

ScrollPaper looks for its config file in `XDG_CONFIG_HOME/scrollpaper/config.cfg`, where `XDG_CONFIG_HOME` is usually `~/.config`. An example config file is provided in this repository as `example_config.cfg`.

Todo/Ideas
----------

- Pre-scale images to use less video memory
- Auto-detect connected/removed monitors
- Auto-detect config changes
- Improve Makefile
