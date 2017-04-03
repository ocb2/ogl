all: gears movement simple squares squares-wayland sierpinski

gears: es2gears.c
	gcc -g -O -o gears -I /home/remi/src/mesa-demos-8.2/src/egl/eglut/ es2gears.c  -lm -lGLESv2 /home/remi/src/mesa-demos-8.2/src/egl/eglut/.libs/libeglut_x11.a -lX11 -lXext -lEGL

movement: movement.c
	libtool --tag=CC --mode=link gcc -g -O2 -o movement -I $$HOME/src/weston/ -I $$HOME/src/weston/protocol -I $$HOME/src/weston/src movement.c $$HOME/src/weston/protocol/weston_simple_egl-xdg-shell-unstable-v5-protocol.o $$HOME/src/weston/protocol/weston_simple_egl-ivi-application-protocol.o  -L/home/remi/loc/lib -lEGL -lGLESv2 -lwayland-egl -lwayland-client -lwayland-cursor -lm

simple: simple-egl.c
	libtool --tag=CC --mode=link gcc -g -O2 -o simple -I $$HOME/src/weston/ -I $$HOME/src/weston/protocol -I $$HOME/src/weston/src simple-egl.c $$HOME/src/weston/protocol/weston_simple_egl-xdg-shell-unstable-v5-protocol.o $$HOME/src/weston/protocol/weston_simple_egl-ivi-application-protocol.o  -L/home/remi/loc/lib -lEGL -lGLESv2 -lwayland-egl -lwayland-client -lwayland-cursor -lm

squares: squares.c
	gcc -g -O -o squares -I /home/remi/src/mesa-demos-8.2/src/egl/eglut/ squares.c  -lm -lGLESv2 /home/remi/src/mesa-demos-8.2/src/egl/eglut/.libs/libeglut_x11.a -lX11 -lXext -lEGL

squares-wayland: squares-wayland.c
	libtool --tag=CC --mode=link gcc -g -O2 -o squares-wayland -I $$HOME/src/weston/ -I $$HOME/src/weston/protocol -I $$HOME/src/weston/src squares-wayland.c $$HOME/src/weston/protocol/weston_simple_egl-xdg-shell-unstable-v5-protocol.o $$HOME/src/weston/protocol/weston_simple_egl-ivi-application-protocol.o  -L/home/remi/loc/lib -lEGL -lGLESv2 -lwayland-egl -lwayland-client -lwayland-cursor -lm

sierpinski: sierpinski.c
	libtool --tag=CC --mode=link gcc -g -O2 -o sierpinski -I $$HOME/src/weston/ -I $$HOME/src/weston/protocol -I $$HOME/src/weston/src sierpinski.c $$HOME/src/weston/protocol/weston_simple_egl-xdg-shell-unstable-v5-protocol.o $$HOME/src/weston/protocol/weston_simple_egl-ivi-application-protocol.o  -L/home/remi/loc/lib -lEGL -lGLESv2 -lwayland-egl -lwayland-client -lwayland-cursor -lm

clean:
	rm gears
	rm movement
	rm simple
	rm squares
	rm squares-wayland
	rm sierpinski

.PHONY: all