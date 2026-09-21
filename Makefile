CFLAGS = -g
CC = gcc


shell: shell.o lexer.o parser.o jobs.o
	$(CC) $(CFLAGS) -o shell shell.o lexer.o parser.o jobs.o

parser: parser.o lexer.o
	$(CC) $(CFLAGS) -o parser lexer.o parser.o


shell.o: shell.c
	$(CC) $(CFLAGS) -c shell.c

lexer.o: lexer.c lexer.h
	$(CC) $(CFLAGS) -c lexer.c

parser.o: parser.c parser.h lexer.h
	$(CC) $(CFLAGS) -c parser.c

jobs.o: jobs.c jobs.h 
		$(CC) $(CFLAGS) -c jobs.c


clean:
	rm -f program *.o

