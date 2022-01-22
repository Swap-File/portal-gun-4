OBJs := $(patsubst %.c, %.o, $(wildcard *.c)) $(patsubst i2c/%.c, i2c/%.o, $(wildcard i2c/*.c)) common/memory.o
DEPs := $(OBJs:%.o=%.d)
BIN  := portal
CC  = gcc -fdiagnostics-color
# -I/usr/include/libdrm must be included here even though it isnt used so higher than j1 works
CFLAGS += --std=gnu18 -Wall -Werror -O2 -I/usr/include/libdrm -fdiagnostics-color=always
LDFLAGS += -lm
MAKEFLAGS += -j4

.PHONY: all
all: subprocesses portal

portal:	$(OBJs) $(LDFLAGS) -lbcm2835

.PHONY: clean
clean:
	@rm -fv portal $(OBJs) $(DEPs) *.o
	$(MAKE) -C console clean
	$(MAKE) -C projector clean

.PHONY: subprocesses
subprocesses:
	$(MAKE) -C console
	$(MAKE) -C projector