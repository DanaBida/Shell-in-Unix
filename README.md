# Shell-in-Unix

Support Job Control and pipeline.

The commands of the shell are done by the function execute that receive an array of two integers - the pipe's file descriptors(A left pipe and a right pipe), and execute the commands.

help file: line_parser- parsing the commands in the shell(separates words and sentences by spaces and newline chars).
