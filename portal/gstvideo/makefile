OBJs := $(patsubst %.c, %.o, $(wildcard *.c)) model_board/model_board.o model_board/png_texture.o  ../sharedmem.o
DEPs := $(OBJs:%.o=%.d)
BIN  := gstvideo
CC  = gcc

#Gstreamer stuff
LDFLAGS = -lX11 -lGL -lpng -lgstgl-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lm
CFLAGS = -I/usr/include/gstreamer-1.0 -I/usr/lib/arm-linux-gnueabihf/gstreamer-1.0/include -I/usr/include/glib-2.0 -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include

CFLAGS += --std=gnu99 -Wall -Werror -O2

.PHONY: all
all: $(BIN)

$(BIN): $(OBJs)

.PHONY: clean
clean:
	@rm -fv $(BIN) $(OBJs) $(DEPs) *.o
