CC= gcc
CFLAGS= -Wall -std=c99
LIBS= -lpthread
FILES= serwer klient klient-d

all: $(FILES)

%: %.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)


klient-d: klient.c
	$(CC) $(CFLAGS) -D DEBUG -o $@ $^ $(LIBS)

.PHONY: all clean

clean:
	rm -f $(FILES) *~
