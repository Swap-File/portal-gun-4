OBJs := $(patsubst %.cpp, %.o, $(wildcard *.cpp)) 
DEPs := $(OBJs:%.o=%.d)
BIN  := portal
CC  = g++


# Turn on "all" warnings
CPPFLAGS += -Wall

# Generate .d files with dependency info
CPPFLAGS += -MD -MP

# Debuggable and optimized.
CPPFLAGS += -O2

LDFLAGS += -lpthread -lm -lbcm2835

.PHONY: all
all: subprocesses portal

portal:	$(OBJs) -lbcm2835 

.PHONY: clean
clean:
	@rm -fv portal $(OBJs) $(DEPs) *.o

.PHONY: subprocesses
subprocesses:
