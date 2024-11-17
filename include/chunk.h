#ifndef clox_chunk_h
#define clox_chunk_h
#include "common.h"
#include "value.h"

/*
    chunk       - sequence of bytecode
    bytecode    - series of instructions
    opcode      - 1-byte number that controls the instruction 
                    being handled
*/
typedef enum{
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_RETURN,

} OpCode;

typedef struct{
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

void init_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, int line);
void free_chunk(Chunk* chunk);
int add_constant(Chunk* chunk, Value value);
#endif