# (c) https://gist.github.com/ork/11248510
EXEC     = $(shell basename $$(pwd))
CC       = gcc

CFLAGS   = -g

SRC      = $(wildcard *.c)
OBJ      = $(SRC:.c=.o)

all: $(EXEC)

${EXEC}: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean distclean debug

clean:
	@rm -rf *.o

distclean: clean
	@rm -rf $(EXEC)
			
debug:
	gdb -tui $(EXEC)
