IDIR=.
CC=gcc

# Compiler flags

# make mode=debug
ifeq ($(mode),debug)
	CFLAGS = -g -Wall -I$(IDIR)
else
	mode = release
	CFLAGS = -Wall -I$(IDIR)
endif

all: p2txt p2speccy p2t-test1 p2s-test1 hex2rem hex2rem-test1

p2txt: p2txt.o
	$(CC) -o $@ $^ $(CFLAGS)

p2speccy: p2speccy.o
	$(CC) -o $@ $^ $(CFLAGS)

hex2rem: hex2rem.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.p: %.bas
	zmakebas -p -n $* -o $@ $<

%.p: %.txt
	zmakebas -p -n $* -o $@ $<

p2t-test1: TEST1-p2txt-r.txt TEST1-p2txt-1.txt TEST1-p2txt-z.txt TEST1-p2txt-2.txt

TEST1-p2txt-r.txt: p2txt TEST1.p
	./p2txt -r TEST1.p > TEST1-p2txt-r.txt

TEST1-p2txt-1.txt: p2txt TEST1.p
	./p2txt -1 TEST1.p > TEST1-p2txt-1.txt

TEST1-p2txt-z.txt: p2txt TEST1.p
	./p2txt -z TEST1.p > TEST1-p2txt-z.txt

TEST1-p2txt-2.txt: p2txt TEST1.p
	./p2txt -2 TEST1.p > TEST1-p2txt-2.txt

p2s-test1: TEST1-p2speccy-r.txt TEST1-p2speccy-z.txt TEST1-p2speccy.tap

TEST1-p2speccy-z.txt: p2speccy TEST1.p
	./p2speccy -z TEST1.p > TEST1-p2speccy-z.txt

TEST1-p2speccy-r.txt: p2speccy TEST1.p
	./p2speccy -r TEST1.p > TEST1-p2speccy-r.txt

TEST1-p2speccy.tap: TEST1-p2speccy-z.txt
	zmakebas -n TEST1 -o TEST1-p2speccy.tap TEST1-p2speccy-z.txt

hex2rem-test1: hex2rem.bas hex2rem.p

hex2rem_test.bas: hex2rem hex2rem_test.txt
	./hex2rem -h hex2rem_test.txt > hex2rem_test.bas

clean:
	rm -f *.o core
