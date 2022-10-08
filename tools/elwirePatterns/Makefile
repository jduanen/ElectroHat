seqgen: seqgen.c seqdefs.h
	cc -Wall -o seqgen seqgen.c

sequences.h: seqgen
	./seqgen > sequences.h

install: sequences.h
	cp ./sequences.h ../../include/

clean:
	rm -f seqgen.o seqgen sequences.h
