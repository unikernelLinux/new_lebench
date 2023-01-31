.PHONY: new_lebench

all:
	gcc -c -o new_lebench.o new_lebench.c CFLAGS="$(CFLAGS)"

clean:
	- rm -rf new_lebench.o 
