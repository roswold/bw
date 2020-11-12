CFLAGS=  -Wfatal-errors $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer SDL2_ttf) -s
LDFLAGS= $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf) -lm

all: ld48hr40
ld48hr40: src/ld48hr40
	@cp src/ld48hr40 ./
src/ld48hr40: src/main.c
	make -C src
%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
clean:
	$(RM) ld48hr40
