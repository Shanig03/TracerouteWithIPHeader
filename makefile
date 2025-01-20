CC = gcc # Copiler
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic # Flags


all: traceroute

# Build the executable
traceroute: traceroute.o
	$(CC) $(CFLAGS) -o traceroute traceroute.o

# Compile the source file into an object file
traceroute.o: traceroute.c
	$(CC) $(CFLAGS) -c traceroute.c -o traceroute.o

run: traceroute
	sudo ./traceroute -a 8.8.8.8

# Clean up build files
clean:
	rm -f traceroute.o traceroute
