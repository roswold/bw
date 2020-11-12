CFLAGS=  -Wfatal-errors $(shell pkg-config --cflags sdl2 SDL2_image) -s
LDFLAGS= $(shell pkg-config --libs sdl2 SDL2_image) -lm

all: ld48hr40
ld48hr40: main.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
