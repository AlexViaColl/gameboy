CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic -ggdb
CFLAGS += -Wno-unused-function -Wno-error=unused-variable -Wno-error=unused-parameter
CFLAGS += `pkg-config --cflags sdl2`

LIBS = `pkg-config --libs sdl2`

.PHONY: clean, test

all: gb_sdl gb_headless gb_test ui

test: gb_test gb_headless
	./gb_test && \
	echo "Running Blargg Tests" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/01-special.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/02-interrupts.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/03-op sp,hl.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/04-op r,imm.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/05-op rp.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/06-ld r,r.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/08-misc instrs.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/09-op r,r.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/10-bit ops.gb" && \
	./gb_headless "./test-roms/blargg/cpu_instrs/individual/11-op a,(hl).gb"

ui: ui.c
	$(CC) $(CFLAGS) -o ui $(LIBS) ui.c

gb_sdl: gb_sdl.c gb.c
	$(CC) $(CFLAGS) -O3 -o gb_sdl $(LIBS) gb_sdl.c gb.c

gb_headless: gb_headless.c gb.c
	$(CC) $(CFLAGS) -O3 -o gb_headless gb_headless.c gb.c

gb_test: gb_test.c gb.c
	$(CC) $(CFLAGS) -o gb_test gb_test.c gb.c

clean:
	rm -f *.o gb_sdl gb_headless gb_test
