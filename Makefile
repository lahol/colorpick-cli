CC=gcc
CFLAGS=-Wall
LIBS = -lX11

PREFIX := /usr

COLORPICKVERSION := '$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1), branch \"$(shell git describe --tags --always --all | sed s:heads/::)\")'

cpc_SRC := $(wildcard *.c)
cpc_OBJ := $(cpc_SRC:.c=.o)
cpc_HEADERS := $(wildcard *.h)

CFLAGS += -DCOLORPICKVERSION=\"${COLORPICKVERSION}\"

all: colorpick-cli

colorpick-cli: $(cpc_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

%.o: %.c $(cpc_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

install: colorpick-cli
	install colorpick-cli $(PREFIX)/bin

uninstall:
	rm $(PREFIX)/bin/colorpick-cli

clean:
	rm -f colorpick-cli $(cpc_OBJ)

.PHONY: all clean install uninstall
