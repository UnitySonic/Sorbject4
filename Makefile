CC = gcc
CFLAGS= -Wall -g
outputDirectory = ./recovered_files

notjustcats: diskAnalyzer.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

tar:
	tar cvzf project4.tgz README Makefile diskAnalyzer.c


clean: 
	rm notjustcats

test1: notjustcats
	./notjustcats blankfloppy.img $(outputDirectory)
test2: notjustcats
	./notjustcats simple.img $(outputDirectory)
test3: notjustcats
	./notjustcats simple2.img ./simple2
test4: notjustcats
	./notjustcats random.img ./random
