CC = gcc
CFLAGS= -Wall -g


notjustcats: diskAnalyzer.c
	$(CC) $(CFLAGS) -o $@ $^


clean: 
	rm notjustcats

test1: notjustcats
	./notjustcats blankfloppy.img output.txt
test2: notjustcats
	./notjustcats simple.img output.txt
