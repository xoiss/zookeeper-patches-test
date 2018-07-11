TARGET := test
ifneq (,$(WINDIR))
    TARGET := $(TARGET).exe
endif

SOURCES := $(shell ls *.c 2>/dev/null)

CC = gcc
CFLAGS = -std=gnu99 -pedantic -pedantic-errors -Wall -Wconversion -Wextra -Werror -O0 -g3 -fmessage-length=0

CFLAGS_mt = -DTHREADED
LDLIBS_mt = -lzookeeper_mt

CFLAGS_st =
LDLIBS_st = -lzookeeper_st

.PHONY: all clean

ifneq (,$(SOURCES))
all: $(TARGET)_mt $(TARGET)_st
$(TARGET)_mt: $(SOURCES)
	$(LINK.c) $^ $(CFLAGS_mt) $(LOADLIBES) $(LDLIBS_mt) -o $@
$(TARGET)_st: $(SOURCES)
	$(LINK.c) $^ $(CFLAGS_st) $(LOADLIBES) $(LDLIBS_st) -o $@
else
all:
	@echo No source files found.
endif

clean:
	$(RM) $(TARGET)_mt $(TARGET)_st
