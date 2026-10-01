# Linux build (X11 + OpenGL) and Windows cross-build with MinGW-w64.
SRC = src/app.c src/bike.c src/content.c src/diagrams.c src/font.c src/mem.c src/mesh.c src/sim.c src/ui.c
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter

all: diverge-e5

diverge-e5: $(SRC) src/platform_x11.c src/*.h
	$(CC) $(CFLAGS) -o $@ $(SRC) src/platform_x11.c -lX11 -lGL -lm

windows: bin/DivergeE5Workshop.exe
bin/DivergeE5Workshop.exe: $(SRC) src/platform_win32.c src/*.h
	mkdir -p bin
	x86_64-w64-mingw32-gcc $(CFLAGS) -mwindows -static -o $@ $(SRC) src/platform_win32.c -lopengl32 -lgdi32 -luser32 -lm
	x86_64-w64-mingw32-strip $@

debug: $(SRC) src/platform_x11.c
	$(CC) -O0 -g -std=c99 -Wall -Wextra -fsanitize=address,undefined -o diverge-e5-debug $(SRC) src/platform_x11.c -lX11 -lGL -lm

clean:
	rm -f diverge-e5 diverge-e5-debug bin/DivergeE5Workshop.exe
.PHONY: all windows debug clean
