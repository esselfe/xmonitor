#ifndef XMONITOR_H
#define XMONITOR_H 1

struct history_frame {
	struct history_frame *prev, *next;
	unsigned long used, buffered;
};

struct history {
	struct history_frame *first_frame, *last_frame;
};

extern struct history mem_history;

void HistoryInit(unsigned int size);
void HistoryAdd(unsigned int used, unsigned int buffered);
void HistoryPopFirst(void);

#endif /* XMONITOR_H */
