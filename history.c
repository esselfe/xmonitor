#include <stdio.h>
#include <stdlib.h>

#include "xmonitor.h"

void HistoryInit(struct history *hist, unsigned int size) {
	struct history_frame *frame = malloc(sizeof(struct history_frame));
	frame->next = NULL;
	frame->prev = NULL;
	frame->used = 0;
	frame->buffered = 0;
	hist->first_frame = frame;
	hist->last_frame = frame;

	unsigned int cnt;
	for (cnt = 0; cnt < size; cnt++) {
		frame->next = malloc(sizeof(struct history_frame));
		frame->next->prev = frame;
		frame->next->next = NULL;
		frame = frame->next;
		frame->used = 0;
		frame->buffered = 0;
		hist->last_frame = frame;
	}
}

void HistoryPopFirst(struct history *hist) {
	struct history_frame *frame = hist->first_frame;
	frame->next->prev = NULL;
	hist->first_frame = frame->next;
	free(frame);
}

void HistoryAdd(struct history *hist, unsigned int used, unsigned int buffered) {
	struct history_frame *frame = malloc(sizeof(struct history_frame));
	frame->prev = hist->last_frame;
	frame->next = NULL;
	frame->used = used;
	frame->buffered = buffered;
	hist->last_frame->next = frame;
	hist->last_frame = frame;
}

