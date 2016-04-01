dchat: util.o sequencer.o
	gcc dchat.c sequencer.c util.c -o dchat

# rules to compile the individual .o files
dchat.o: dchat.c
	gcc -c -Wall dchat.c

util.o: util.c
	gcc -c -Wall util.c

sequencer.o: sequencer.c
	gcc -c -Wall sequencer.c

# removes .o and executable files
clean:
	rm  -f dchat dchat.o util.o

# removes any temporary (~) files
cleaner: 
	rm  -f *~

# runs both clean and cleaner
cleanest: clean cleaner