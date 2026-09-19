CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
PREFIX  ?= /usr/local

X11_CFLAGS    := $(shell pkg-config --cflags x11)
X11_LIBS      := $(shell pkg-config --libs x11)
CURSES_CFLAGS := $(shell pkg-config --cflags ncurses)
CURSES_LIBS   := $(shell pkg-config --libs ncurses)

SRC := src/main.c src/core.c src/segments.c src/eyes.c src/ui_x11.c src/ui_tui.c
OBJ := $(SRC:.c=.o)
BIN := clock

ALL_CFLAGS := $(CFLAGS) $(X11_CFLAGS) $(CURSES_CFLAGS)
ALL_LIBS   := $(X11_LIBS) $(CURSES_LIBS)

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(ALL_LIBS)

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

src/main.o:     src/core.h src/ui.h
src/core.o:     src/core.h
src/segments.o: src/segments.h
src/eyes.o:     src/eyes.h
src/ui_x11.o:   src/ui.h src/core.h src/segments.h src/eyes.h
src/ui_tui.o:   src/ui.h src/core.h src/segments.h src/eyes.h

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all install clean
