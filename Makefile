CFLAGS=  -Wfatal-errors $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer) -s
LDFLAGS= $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lm

all: ld48hr40
ld48hr40: src/ld48hr40
src/ld48hr40: src/main.c
	make -C src
	@cp src/ld48hr40 ./
%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
clean:
	$(RM) ld48hr40
