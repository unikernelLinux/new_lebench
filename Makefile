.PHONY: new_lebench

all:
	gcc $(CFLAGS) -c -o new_lebench.o new_lebench.c

clean:
	- rm -rf new_lebench.o 
