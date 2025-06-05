CC = gcc
CFLAGS = -O2 -Wall

all: guess_signals guess_pipe

guess_signals: guess_signals.c
	$(CC) $(CFLAGS) -o guess_signals guess_signals.c

guess_pipe: guess_pipe.c
	$(CC) $(CFLAGS) -o guess_pipe guess_pipe.c

clean:
	rm -f guess_signals guess_pipe
