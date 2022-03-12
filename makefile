# the compiler to be used
CC = gcc

# compiler flags
#  -g     - this flag adds debugging information to the executable file
#  -Wall  - this flag is used to turn on most compiler warnings
#  -O3    - this flag is used for production level optimization
CFLAGS = -g -Wall -O3

# The build targets
DLX_TARGET = dlx

# The object names
DLX_OBJ = dlx.o

# Build target set
TARGETS = $(DLX_TARGET)

# Object name set
OBJS = $(DLX_OBJ)

# path to any header files not in /usr/include or the current directory 
INCLUDES +=-I include/

# add the path to my library code; -L tells the linker where to find it
#LFLAGS += -L /home/newhall/mylibs

# list of libraries to link into executable; -l tells the linker which 
# library to link into the executable 
#LIBS = -lmymath -lsimple -lm -lpthread

default: $(TARGETS)

clean:
	rm *.o ${DLX_TARGET}

cleanDLX:
	rm *.o ${DLX_TARGET}

# Link DLX_TARGET binary
${DLX_TARGET} : ${DLX_OBJ}
	${CC} ${CFLAGS} ${LFLAGS} -o ${DLX_TARGET} ${DLX_OBJ} ${LIBS}

# Generic build of objects
${OBJS}:
	${CC} -c ${CFLAGS} ${INCLUDES} src/${@:.o=.c}