OBJs := $(patsubst %.cpp, %.o, $(wildcard *.cpp)) 
DEPs := $(OBJs:%.o=%.d)
BIN  := portal
CC  = g++

# Use a modern language
CPPFLAGS += --std=c++0x

# Turn on "all" warnings
CPPFLAGS += -Wall

# Generate .d files with dependency info
CPPFLAGS += -MD -MP

# Debuggable and optimized.
CPPFLAGS += -O2

LDFLAGS += -lpthread -lm

.PHONY: all
all: subprocesses portal

portal:	$(OBJs) -lbcm2835 

.PHONY: clean
clean:
	@rm -fv portal $(OBJs) $(DEPs) *.o
	$(MAKE) -C gstvideo clean

.PHONY: subprocesses
subprocesses:
	$(MAKE) -C gstvideo