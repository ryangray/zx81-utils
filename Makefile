IDIR=.
CC=gcc

#Compiler flags
#if mode variable is empty, setting debug build mode
ifeq ($(mode),debug)
   CFLAGS = -g -Wall -I$(IDIR)
else
   mode = debug
   CFLAGS = -Wall -I$(IDIR)
endif

all: p2txt

p2txt: p2txt.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o core

