
CFLAGS = -Wall -std=c17 -O2 -D_DEFAULT_SOURCE
LDFLAGS = -lX11
OBJDIR = obj
OBJS = $(OBJDIR)/xmonitor.o $(OBJDIR)/history.o
PROGNAME = xmonitor

.PHONY: all clean prepare

default: all

all: prepare $(PROGNAME)

prepare:
	@[ -d $(OBJDIR) ] || mkdir -v $(OBJDIR)

$(PROGNAME): $(OBJS)
	$(CC) $(OBJS) -o $(PROGNAME) $(LDFLAGS)

$(OBJDIR)/xmonitor.o: xmonitor.c xmonitor.h
	$(CC) -c $(CFLAGS) xmonitor.c -o $(OBJDIR)/xmonitor.o

$(OBJDIR)/history.o: history.c xmonitor.h
	$(CC) -c $(CFLAGS) history.c -o $(OBJDIR)/history.o

clean:
	@rm -rfv $(OBJDIR) $(PROGNAME) 2>/dev/null || true

