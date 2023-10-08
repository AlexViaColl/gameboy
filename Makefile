CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic -ggdb
CFLAGS += -Wno-unused-function -Wno-error=unused-variable -Wno-error=unused-parameter
CFLAGS += `pkg-config --cflags sdl2`

LIBS = `pkg-config --libs sdl2`

.PHONY: clean, test

all: gb_sdl.c gb.c
	$(CC) $(CFLAGS) -O3 -o gb $(LIBS) gb_sdl.c gb.c

test: build_test
	./test

build_test: gb_test.c gb.c
	$(CC) -o test gb_test.c gb.c

clean:
	rm -f *.o gb test
