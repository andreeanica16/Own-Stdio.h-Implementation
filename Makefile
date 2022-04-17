CC = gcc
CFLAGS = -fPIC -Wall

build: libso_stdio.so

libso_stdio.so: so_stdio.o
	$(CC) -shared $^ -o $@

so_stdio.o: so_stdio.c

.PHONY: clean

clean:
	rm so_stdio.o libso_stdio.so
