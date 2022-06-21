#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <X11/Xlib.h>

#include "xmonitor.h"

Display *display;
Window window;
GC gc, gc_buffers;
time_t t0, tp;
// May require changes on other systems:
char *temp_file = "/sys/class/hwmon/hwmon2/temp3_input";
XTextItem text_temp;
int text_temp_len;
unsigned int text_temp_pos_x = 4, text_temp_pos_y = 12;
unsigned int mem_pos_x = 50, mem_pos_y = 20,
	mem_width = 40, mem_height = 40,
	swap_pos_x = 100, swap_pos_y = 20,
	swap_width = 40, swap_height = 40;

struct history mem_history, swap_history;

void XmonitorSignal(int signum) {
	Display *dsp = XOpenDisplay(NULL);
	XClearArea(dsp, window, mem_pos_x, mem_pos_y, mem_width+1, mem_height+1, False);
	XClearArea(dsp, window, swap_pos_x, swap_pos_y, swap_width+1, swap_height+1, False);
	XClearArea(dsp, window, text_temp_pos_x, text_temp_pos_y-10, 100, 10, False);
	XFlush(dsp);
	exit(0);
}

void Draw(void) {
	XClearArea(display, window, mem_pos_x, mem_pos_y, mem_width, mem_height, False);
	XClearArea(display, window, swap_pos_x, swap_pos_y, swap_width, swap_height, False);

	XClearArea(display, window, text_temp_pos_x, text_temp_pos_y-10, 100, 10, False);
	XDrawText(display, window, gc, text_temp_pos_x, text_temp_pos_y, &text_temp, 1);

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

	// top
	XDrawLine(display, window, gc, swap_pos_x, swap_pos_y,
		swap_pos_x + swap_width, swap_pos_y);
	// right
	XDrawLine(display, window, gc, swap_pos_x + swap_width, swap_pos_y,
		swap_pos_x + swap_width, swap_pos_y + swap_height);
	// bottom
	XDrawLine(display, window, gc, swap_pos_x, swap_pos_y + swap_height,
		swap_pos_x + swap_width, swap_pos_y + swap_height);
	// left
	XDrawLine(display, window, gc, swap_pos_x, swap_pos_y,
		swap_pos_x, swap_pos_y + swap_height);

	int cnt;
	struct history_frame *frame = mem_history.first_frame;
	for (cnt = 0; cnt < mem_width; cnt++) {
		if (frame->used > 0 || frame->buffered > 0) {
			XDrawLine(display, window, gc, 
				mem_pos_x + cnt, mem_pos_y + mem_height,
				mem_pos_x + cnt, mem_pos_y + (mem_height - frame->used));
			XDrawLine(display, window, gc_buffers,
				mem_pos_x + cnt, mem_pos_y + mem_height - frame->used,
				mem_pos_x + cnt, mem_pos_y + mem_height - frame->used - frame->buffered);
		}
		
		if (frame->next)
			frame = frame->next;
		else
			break;
	}

	frame = swap_history.first_frame;
	for (cnt = 0; cnt < swap_width; cnt++) {
		if (frame->used > 0 || frame->buffered > 0) {
			XDrawLine(display, window, gc, 
				swap_pos_x + cnt, swap_pos_y + swap_height,
				swap_pos_x + cnt, swap_pos_y + (swap_height - frame->used));
		}
		
		if (frame->next)
			frame = frame->next;
		else
			break;
	}
}

void UpdateTemp(void) {
	FILE *fp = fopen(temp_file, "r");
	if (fp == NULL) {
		printf("xmonitor error: Cannot open %s: %s\n", temp_file,
			strerror(errno));
		return;
	}

	char *line = malloc(64);
	memset(line, 0, 64);
	size_t line_size = 64;
	ssize_t nread = getline(&line, &line_size, fp);
	unsigned int val = 0;
	if (nread > 0) {
		val = atoi(line)/1000;
		sprintf(text_temp.chars, "%u", val);
		text_temp.nchars = strlen(text_temp.chars);
	}

	fclose(fp);
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

	gcv.foreground = 0x808080;
	gcv.background = 0x101010;
	gcv.line_width = 1;
	gcv.line_style = LineSolid;
	gc_buffers = XCreateGC(display, window,
				GCForeground|GCBackground|GCLineWidth|GCLineStyle,
				&gcv);
	if (gc_buffers == NULL) {
		printf("xmonitor error: Cannot create graphical context!\n");
		return 1;
	}

	text_temp_len = 4;
	text_temp.chars = malloc(text_temp_len);
	sprintf(text_temp.chars, "000");
	text_temp.nchars = strlen(text_temp.chars);
	text_temp.delta = 0;

	HistoryInit(&mem_history, mem_width);
	HistoryInit(&swap_history, swap_width);

	struct sysinfo sinfo;
	while (1) {
		t0 = time(NULL);
		if (t0 > tp) {
			tp = t0;

			UpdateTemp();
			
			sysinfo(&sinfo);
			unsigned int bytes_per_pixel = sinfo.totalram / mem_height;
			unsigned long value = 
				(sinfo.totalram - sinfo.freeram - sinfo.bufferram) / bytes_per_pixel,
				value2 = sinfo.bufferram / bytes_per_pixel;
			HistoryPopFirst(&mem_history);
			HistoryAdd(&mem_history, value, value2);

			if (sinfo.totalswap > 0) {
				bytes_per_pixel = sinfo.totalswap / swap_height;
				value = (sinfo.totalswap - sinfo.freeswap) / bytes_per_pixel;
				HistoryPopFirst(&swap_history);
				HistoryAdd(&swap_history, value, 0);
			}
			else {
				HistoryPopFirst(&swap_history);
				HistoryAdd(&swap_history, 0, 0);
			}

			Draw();
			XFlush(display);
		}

		usleep(10000);
	}

	XCloseDisplay(display);
	return 0;
}

