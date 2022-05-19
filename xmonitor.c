#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <X11/Xlib.h>

#include "xmonitor.h"

Display *display;
Window window;
GC gc;
time_t t0, tp;

unsigned int mem_pos_x = 50, mem_pos_y = 20,
	mem_width = 40, mem_height = 40;

struct history mem_history;

void XmonitorSignal(int signum) {
	Display *dsp = XOpenDisplay(NULL);
	XClearArea(dsp, window, mem_pos_x, mem_pos_y, mem_width+1, mem_height+1, True);
	XFlush(dsp);
	exit(0);
}

void DrawMem(void) {
	XClearArea(display, window, mem_pos_x, mem_pos_y, mem_width, mem_height, False);

	// Draw contour lines
	// top
	XDrawLine(display, window, gc, mem_pos_x, mem_pos_y,
		mem_pos_x + mem_width, mem_pos_y);
	// right
	XDrawLine(display, window, gc, mem_pos_x + mem_width, mem_pos_y,
		mem_pos_x + mem_width, mem_pos_y + mem_height);
	// bottom
	XDrawLine(display, window, gc, mem_pos_x, mem_pos_y + mem_height,
		mem_pos_x + mem_width, mem_pos_y + mem_height);
	// left
	XDrawLine(display, window, gc, mem_pos_x, mem_pos_y,
		mem_pos_x, mem_pos_y + mem_height);

	int cnt;
	struct history_frame *frame = mem_history.first_frame;
	for (cnt = 0; cnt < mem_width; cnt++) {
		if (frame->used > 0 || frame->buffered > 0) {
			XDrawLine(display, window, gc, 
				mem_pos_x + cnt, mem_pos_y + mem_height,
				mem_pos_x + cnt, mem_pos_y + (mem_height - frame->used));
		}
		
		if (frame->next)
			frame = frame->next;
		else
			break;
	}
}

int main(int argc, char **argv) {
	printf("xmonitor started\n");

	signal(SIGINT, XmonitorSignal);
	signal(SIGTERM, XmonitorSignal);

	tp = t0 = time(NULL);

	display = XOpenDisplay(NULL);
	if (display == NULL) {
		printf("xmonitor error: Cannot open X display!\n");
		return 1;
	}

	window = XDefaultRootWindow(display);

	XGCValues gcv;
	gcv.foreground = 0xababab;
	gcv.background = 0x101010;
	gcv.line_width = 1;
	gcv.line_style = LineSolid;
	gc = XCreateGC(display, window,
				GCForeground|GCBackground|GCLineWidth|GCLineStyle,
				&gcv);
	if (gc == NULL) {
		printf("xmonitor error: Cannot create graphical context!\n");
		return 1;
	}

	HistoryInit(mem_width);

	struct sysinfo sinfo;
	while (1) {
		t0 = time(NULL);
		if (t0 > tp) {
			tp = t0;
			
			sysinfo(&sinfo);
			unsigned int bytes_per_pixel = sinfo.totalram / mem_height;
			unsigned int value = 
				(sinfo.totalram - sinfo.freeram - sinfo.bufferram) / bytes_per_pixel;
			HistoryPopFirst();
			HistoryAdd(value, 0);

			DrawMem();
		}

		XFlush(display);
		usleep(10000);
	}

	XCloseDisplay(display);
	return 0;
}

