OBJECTS = ./main.o ./lib/chunk.o ./lib/debug.o \
			./lib/value.o ./lib/memory.o -I

CLOX: $(OBJECTS)
	cc -o clox $(OBJECTS)

# $(OBJECTS): ./include/chunk.h ./include/common.h \
# 			./include/debug.h ./include/memory.h \
# 			./include/value.h

./lib/chunk.o:./include/chunk.h 
