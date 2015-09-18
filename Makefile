CFLAGS=-Wall
SRC=$(wildcard *.c)
PROG=$(patsubst %.c,obj/%,$(SRC))
LIB=../tools/obj/libdmp.a

all: $(PROG)

$(LIB):
	@cd ../tools && make dmplib

obj/%: %.c $(LIB)
	@mkdir -p obj
	gcc -lpthread -I../tools/libdmp -ggdb -O0 $(CFLAGS) -o $@ $< $(LIB)

clean:
	rm -rf obj
