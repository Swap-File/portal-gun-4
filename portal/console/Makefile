OBJs := $(patsubst %.c, %.o, $(wildcard *.c)) $(patsubst ../common/%.c, ../common/%.o, $(wildcard ../common/*.c))
DEPs := $(OBJs:%.o=%.d)
BIN  := console
CC  = gcc

#Gstreamer stuff
LDFLAGS = -lgbm -ldrm -lfreetype -lGLESv2 -lEGL -lm -lGL -lpng -lpthread -lglib-2.0 -lgstgl-1.0 -lgstreamer-1.0 -lgobject-2.0
CFLAGS = -I/usr/include/freetype2 -I/usr/include/libdrm -I/usr/include/gstreamer-1.0 -I/usr/lib/arm-linux-gnueabihf/gstreamer-1.0/include -I/usr/include/glib-2.0 -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include

CFLAGS += --std=gnu18 -Wall -O2

.PHONY: all
all: $(BIN)

$(BIN): $(OBJs) $(LDFLAGS) 

.PHONY: clean
clean:
	@rm -fv $(BIN) $(OBJs) $(DEPs) *.o