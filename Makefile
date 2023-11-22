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

all: p2txt-all p2speccy-all hex2rem-all rem2bin-all hex2tap-all

.PHONY: all

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.p: %.bas
	zmakebas -p -n $* -o $@ $<

%.p: %.txt
	zmakebas -p -n $* -o $@ $<

p2txt-all: p2txt p2t-test1

p2txt: p2txt.o
	$(CC) -o $@ $^ $(CFLAGS)

p2t-test1: TEST1-p2txt-r.txt TEST1-p2txt-1.txt TEST1-p2txt-z.txt TEST1-p2txt-2.txt

TEST1-p2txt-r.txt: p2txt TEST1.p
	./p2txt -r TEST1.p > TEST1-p2txt-r.txt

TEST1-p2txt-1.txt: p2txt TEST1.p
	./p2txt -1 TEST1.p > TEST1-p2txt-1.txt

TEST1-p2txt-z.txt: p2txt TEST1.p
	./p2txt -z TEST1.p > TEST1-p2txt-z.txt

TEST1-p2txt-2.txt: p2txt TEST1.p
	./p2txt -2 TEST1.p > TEST1-p2txt-2.txt

p2speccy-all: p2speccy p2s-test1

p2speccy: p2speccy.o
	$(CC) -o $@ $^ $(CFLAGS)

p2s-test1: TEST1-p2speccy-r.txt TEST1-p2speccy-z.txt TEST1-p2speccy.tap

TEST1-p2speccy-z.txt: p2speccy TEST1.p
	./p2speccy -z TEST1.p > TEST1-p2speccy-z.txt

TEST1-p2speccy-r.txt: p2speccy TEST1.p
	./p2speccy -r TEST1.p > TEST1-p2speccy-r.txt

TEST1-p2speccy.tap: TEST1-p2speccy-z.txt
	zmakebas -n TEST1 -o TEST1-p2speccy.tap TEST1-p2speccy-z.txt

hex2rem-all: hex2rem hex2rem-test1

hex2rem: hex2rem.o
	$(CC) -o $@ $^ $(CFLAGS)

hex2rem-test1: hex2rem.bas hex2rem.p

hex2rem.bas: hex2rem hex2rem.txt
	./hex2rem -h hex2rem.txt > hex2rem.bas

rem2bin-all: rem2bin rem2bin-test

rem2bin: rem2bin.o
	$(CC) -o $@ $^ $(CFLAGS)

rem2bin-test: rem2bin.bin rem2bin.txt

rem2bin.txt: rem2bin TEST1.p
	./rem2bin -h -o rem2bin.txt TEST1.p

rem2bin.bin: rem2bin TEST1.p
	./rem2bin -b -o rem2bin.bin TEST1.p

hex2tap-all: hex2tap pictest.tap

hex2tap: hex2tap.o
	$(CC) -o $@ $^ $(CFLAGS)

pic.tap: hex2tap pic.scr
	./hex2tap -b -a SCR -n pic -o pic.tap pic.scr

pictest.tap: pic.tap loadpic.tap
	cat loadpic.tap pic.tap > pictest.tap

.PHONY: clean

clean:
	rm -f core
	rm p2txt
	rm p2speccy
	rm hex2rem
	rm rem2bin
	rm hex2tap
