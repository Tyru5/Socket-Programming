# Tyrus Malmstrom
# Makefile for cs457 Project 1

# List of files
C_SRCS		= chat.c
C_OBJS		= chat.o

OBJS		= ${C_OBJS} 
EXE	        = chat

# Compiler and loader commands and flags
CC		= gcc
CC_FLAGS	= -g -O0 -Wall -c
LD_FLAGS	= -g -Wall


all  : chat
	@echo "-- Success!"

# Target is the executable
chat : $(OBJS)
	@echo "Linking all object modules ..."
	$(CC) $(LD_FLAGS) $(OBJS) -o $(EXE)
	@echo ""

# Compile .c files to .o files
.c.o:
	@echo "Compiling each C source file separately ..."
	$(CC) $(CC_FLAGS) $<
	@echo ""

# Clean up the directory
clean:
	@echo "Cleaning up project directory ..."
	rm -f *.o *~ $(EXE).tar $(EXE) core
	@echo ""

# Package the directory
package:
	@echo "Packaging up project directory ..."
	tar -cvf $(EXE).tar *.c Makefile README.txt
	@echo ""


