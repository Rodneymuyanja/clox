#ifndef clox_vm_h
#define clox_vm_h
#include "value.h"
#include "chunk.h"
#include "table.h"
#include "object.h"


#define FRAMES_MAX 64
#define STACK_MAX   (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
}CallFrame;


typedef struct{
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    Value stack[STACK_MAX];
    Value* stack_top;
    Table strings;
    Table globals;
    Obj* objects;
} VM;

typedef enum{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void init_vm();
void free_vm();
void push(Value value);
Value pop();
InterpretResult interpret(const char* source);
#endif
