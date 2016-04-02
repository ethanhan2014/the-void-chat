dchat: util.o sequencer.o client.o
	gcc dchat.c sequencer.c client.c util.c -o dchat -lpthread

# rules to compile the individual .o files
dchat.o: dchat.c
	gcc -c -Wall dchat.c

util.o: util.c
	gcc -c -Wall util.c

sequencer.o: sequencer.c
	gcc -c -Wall sequencer.c

client.o: client.c
	gcc -c -Wall client.c -lpthread

# removes .o and executable files
clean:
	rm  -f dchat dchat.o util.o sequencer.o client.o

# removes any temporary (~) files
cleaner: 
	rm  -f *~

# runs both clean and cleaner
cleanest: clean cleaner