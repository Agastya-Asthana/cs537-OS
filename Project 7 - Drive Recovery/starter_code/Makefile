CC = gcc
CFLAGS = -Wall -Wextra -Werror
DFLAGS = -g
DEPENDENCIES.C = read_ext2.c
EXEC = runscan
MAIN.C = runscantest3.c

default: main

run:
	./runscan ../disk_images/image-02 ./Images/

clean:
	rm -f $(EXEC)
	rm -rf ./Images/

main: $(MAIN.C)
	$(CC) $(CFLAGS) $(DFLAGS) $(MAIN.C) $(DEPENDENCIES.C) -o $(EXEC)
