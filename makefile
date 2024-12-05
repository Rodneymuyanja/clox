OBJECTS = main.o chunk.o debug.o \
			value.o memory.o vm.o

CLOX: $(OBJECTS)
	cc -o clox $(OBJECTS)

main.o:chunk.h common.h debug.h memory.h
chunk.o:chunk.h value.h memory.h
vm.o:common.h debug.h vm.h
value.o:memory.h value.h
debug.o:debug.h
memory.o:memory.h

chunk.h:value.h common.h
vm.h:value.h chunk.h
value.h:common.h
memory.h:common.h
debug.h:chunk.h
common.h: