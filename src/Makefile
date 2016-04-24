## MAKEFILE 
CC=gcc
CFLAGS=-g -Wall -I. 
LIB  = -lpthread
DEPS = util.h
OBJ = dchat.o sequencer.o client.o util.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

dchat: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIB)

clean:
	rm  -f dchat *.o