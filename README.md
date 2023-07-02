# mach
Multithreaded command execution utility similar to POSIX make.

This utility uses a semaphore (sem.h) and a code execution library (run.h) developed by FAU´s i4 (https://sys.cs.fau.de).

# Usage
```
usage: mach <number of threads> <mach-file>
```
The machfile should include commands to be executed that are each seperated by a newline. The utility will execute at most ```<number of threads>``` commands of a block in parallel. Blocks of commands are separated by an empty line.

# Example machfile

This mach examplefile will compile all necessary files needed for the mach utility, first creating .o files in parallel using the first two lines, then linking them and creating the final mach executable using the line after the emptyline.
```
gcc -std=c11 -g -pthread -pedantic -Wall -Werror -D_XOPEN_SOURCE=700 -c queue.c
gcc -std=c11 -g -pthread -pedantic -Wall -Werror -D_XOPEN_SOURCE=700 -c mach.c

gcc -std=c11 -g -pthread -pedantic -Wall -Werror -D_XOPEN_SOURCE=700 -o mach mach.o queue.o pub/run.o pub/sem.o
```
