CC = gcc
CFLAGS = -Wextra -Wall -Werror -std=c99 -pedantic -I ./src/
SRCDIR = ./src
OBJ = $(SRCDIR)/minimake.o \
      $(SRCDIR)/options/options.o \
      $(SRCDIR)/parse/linked.o $(SRCDIR)/parse/parse.o \
      $(SRCDIR)/rule/rule.o \
      $(SRCDIR)/variable/variable.o \
      $(SRCDIR)/error/error.o

all: minimake

minimake: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: CFLAGS += -fsanitize=address
check: clean all
	@./tests/test8.py ./minimake tests/

doc:
	cd ./src && doxygen doc/Doxyfile

clean:
	$(RM) minimake $(OBJ) test test.o

.PHONY: all check clean doc
