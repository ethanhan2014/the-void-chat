# dchat: util.o sequencer.o client.o dchat.o
# 	gcc dchat.o sequencer.o client.o util.o -o dchat -lpthread

# # rules to compile the individual .o files
# dchat.o: dchat.c
# 	gcc -c -Wall dchat.c

# util.o: util.c
# 	gcc -c -Wall util.c

# sequencer.o: sequencer.c
# 	gcc -c -Wall sequencer.c

# client.o: client.c 
# 	gcc -c -Wall client.c -lpthread

# # removes .o and executable files
# clean:
# 	rm  -f dchat *.o

# # removes any temporary (~) files
# cleaner: 
# 	rm  -f *~

# # runs both clean and cleaner
# cleanest: clean cleaner

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