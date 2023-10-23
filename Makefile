CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic -ggdb
CFLAGS += -Wno-unused-function -Wno-error=unused-variable -Wno-error=unused-parameter
CFLAGS += `pkg-config --cflags sdl2`

LIBS = `pkg-config --libs sdl2`

.PHONY: clean, test

all: gb_sdl gb_test

gb_sdl: gb_sdl.c gb.c
	$(CC) $(CFLAGS) -O3 -o gb_sdl $(LIBS) gb_sdl.c gb.c

gb_test: gb_test.c gb.c
	$(CC) $(CFLAGS) -o gb_test gb_test.c gb.c

clean:
	rm -f *.o gb_sdl gb_test
