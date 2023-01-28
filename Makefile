IDIR=.
CC=gcc
CFLAGS=-I$(IDIR)
OBJ=p2txt.o

all: p2txt

p2txt: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o core

