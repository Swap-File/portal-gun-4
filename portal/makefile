OBJs := $(patsubst %.c, %.o, $(wildcard *.c)) common/memory.o
DEPs := $(OBJs:%.o=%.d)
BIN  := portal
CC  = gcc

CFLAGS += --std=gnu18 -Wall -Werror -O2
LDFLAGS += -lm

.PHONY: all
all: subprocesses portal

portal:	$(OBJs) -lbcm2835 

.PHONY: clean
clean:
	@rm -fv portal $(OBJs) $(DEPs) *.o
	$(MAKE) -C console clean
	$(MAKE) -C projector clean

.PHONY: subprocesses
subprocesses:
	$(MAKE) -C console
	$(MAKE) -C projector