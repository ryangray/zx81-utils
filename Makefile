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

all: p2txt p2spectrum test1p2t test1p2s

p2txt: p2txt.o
	$(CC) -o $@ $^ $(CFLAGS)

p2spectrum: p2spectrum.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.p: %.bas
	zmakebas -p -o %@ $<

%.p: %.txt
	zmakebas -p -o %@ $<

test1p2t: p2txt TEST1.p
	./p2txt -r TEST1.p > TEST1-p2txt-r.txt
	./p2txt -z TEST1.p > TEST1-p2txt-z.txt
	
test1p2s: p2spectrum TEST1.p
	./p2spectrum -r TEST1.p > TEST1-p2spectrum-r.txt
	./p2spectrum -z TEST1.p > TEST1-p2spectrum-z.txt
	zmakebas -o TEST1-p2spectrum.tap TEST1-p2spectrum-z.txt

clean:
	rm -f *.o core

