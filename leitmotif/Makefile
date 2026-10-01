# Linux build (X11 + OpenGL) and Windows cross-build with MinGW-w64.
SRC = src/app.c src/content.c src/draw.c src/font.c src/mem.c src/stage.c src/synth.c src/tex.c
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation

all: leitmotif

leitmotif: $(SRC) src/platform_x11.c src/*.h
	$(CC) $(CFLAGS) -o $@ $(SRC) src/platform_x11.c -lX11 -lGL -lm -ldl

windows: bin/Leitmotif.exe
bin/Leitmotif.exe: $(SRC) src/platform_win32.c src/*.h
	mkdir -p bin
	x86_64-w64-mingw32-gcc $(CFLAGS) -mwindows -static -o $@ $(SRC) src/platform_win32.c -lopengl32 -lgdi32 -luser32 -lwinmm -lm
	x86_64-w64-mingw32-strip $@

debug: $(SRC) src/platform_x11.c
	$(CC) -O0 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -o leitmotif-debug $(SRC) src/platform_x11.c -lX11 -lGL -lm -ldl

selftest: leitmotif
	mkdir -p shots
	xvfb-run -a -s "-screen 0 1440x900x24" ./leitmotif --selftest shots

clean:
	rm -f leitmotif leitmotif-debug bin/Leitmotif.exe
.PHONY: all windows debug selftest clean
