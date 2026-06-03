CC = gcc
CFLAGS = -Wall -Wextra -g
LIB_SRC = uthreads.c jump.c
TESTS = test1 test2 test3

all: $(TESTS)

test%: tests/test%.c $(LIB_SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TESTS)
