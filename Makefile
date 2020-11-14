CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

xacc: $(SRCS)
	cc $(SRCS) -o $@ $(CFLAGS)

test:
	./test.sh

clean:
	rm -f xacc *.o *~ tmp*
.PHONY: test clean