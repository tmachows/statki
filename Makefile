CC= gcc
CFLAGS= -Wall -std=c99
LIBS= -lpthread
FILES= serwer klient

all: $(FILES)

%: %.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f $(FILES) *~

.PHONY: all clean
