prefix ?= /usr/local

scrollpaper: main.cpp
	g++ -Wall -g -o scrollpaper main.cpp -lsfml-graphics -lsfml-window -lsfml-system -lX11 -lXrandr -lpthread -lconfig++

clean:
	rm scrollpaper

install:
	install -D -m0755 scrollpaper ${DESTDIR}${prefix}/bin/scrollpaper

uninstall:
	rm -f ${DESTDIR}${prefix}/bin/scrollpaper
