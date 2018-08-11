# All Targets
all: myshell

# Tool invocations
myshell: line_parser.o job_control.o task1e.o
	gcc -m64 -Wall -ansi line_parser.o  job_control.o task1e.o -o myshell

# Depends on the source and header files
line_parser.o: line_parser.c
	gcc -m64 -Wall -ansi -c -fno-stack-protector line_parser.c -o line_parser.o

job_control.o: job_control.c
	gcc -m64 -Wall -ansi -c -fno-stack-protector job_control.c -o job_control.o

task1e.o: task1e.c
	gcc -m64 -Wall -ansi -c -fno-stack-protector task1e.c -o task1e.o

#tell make that "clean" is not a file name!
.PHONY: clean

#Clean the build directory
clean: 
	rm -f *.o myshell
