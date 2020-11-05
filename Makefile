CFLAGS=-std=c11 -g -static
SRCS=$(filter-out helloworld.c irdump.c, $(wildcard *.c))
xacc: $(SRCS)
	rm -f xacc *.o *~ tmp*
	cc $(SRCS) -o xacc -g
test:
	./test.sh
clean:
	rm -f xacc *.o *~ tmp*
.PHONY: test clean