CFLAGS=-std=c11 -g
LDFLAGS=-static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

xacc: $(OBJS)
	cc $(OBJS) -o $@ $(LDFLAGS)

test:
	./test.sh

clean:
	rm -f xacc *.o *~ tmp*
.PHONY: test clean