OBJs := $(patsubst %.c, %.o, $(wildcard *.c)) 
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
	$(MAKE) -C portalgl clean

.PHONY: subprocesses
subprocesses:
	$(MAKE) -C portalgl
