TMPFILES = lex.c parse.c
MODULES = main parse output
OBJECTS = $(MODULES:=.o)
CC = gcc -g -Wall
LINK = $(CC)
LIBS = -lreadline

all: fake_edit shell

fake_edit:
	touch main.c

shell: $(OBJECTS)
	$(LINK) $(OBJECTS) -o $@ $(LIBS)

# Compiling:
%.o: %.c
	$(CC) -c $<

# parsers

parse.o: parse.c lex.c
parse.c: parse.y global.h
	bison parse.y -o $@
lex.c: lex.l
	flex -o$@ lex.l

#test

test: fake_edit_test
	gcc test/slow.c -o test/slow
	gcc test/slow_fail.c -o test/slow_fail
	gcc test/rl_test.c -lreadline -o test/rl_test

fake_edit_test:
	touch test/slow.c

# clean

clean: 
	rm -f shell $(OBJECTS) $(TMPFILES) core* *.o
	rm test/slow test/slow_fail test/rl_test
