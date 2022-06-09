#ifndef XMONITOR_H
#define XMONITOR_H 1

struct history_frame {
	struct history_frame *prev, *next;
	unsigned long used, buffered;
};

struct history {
	struct history_frame *first_frame, *last_frame;
};

extern struct history mem_history, swap_history;

void HistoryInit(struct history *hist, unsigned int size);
void HistoryAdd(struct history *hist, unsigned long used, unsigned long buffered);
void HistoryPopFirst(struct history *hist);

#endif /* XMONITOR_H */
