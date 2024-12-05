#include <stdarg.h>
#include <stdio.h>
#include "common.h"
#include "vm.h"
#include "debug.h"


VM vm;

void init_vm(){
    reset_stack();
}

void free_vm(){

}

InterpretResult interpret(const char* source){
    Chunk chunk;
    init_chunk(&chunk);

    if(!compile(source,&chunk)){
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();
    free_chunk(&chunk);
    return result;
}

void push(Value value){
    /*
        the function reset_stack() sets the stack_top pointer to
        the very first element in the stack

        so here we directly push 'value' into that first slot
         _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ____
        |     |     |
        |_ _ _|_ _ _| _ _ _ _ _ _ _ _ _ _ _ _ _ _ ______
                 
    */
    *vm.stack_top = value;
    vm.stack_top++;
}

Value pop(){
    vm.stack_top--;
    return *vm.stack_top;
}

static Value peek(int distance){
    return vm.stack_top[-1 - distance];
}

static void reset_stack(){
    vm.stack_top = vm.stack;
}

static InterpretResult run(){
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
/*
    adventurous use of the C-Preprocessor, but pay attention here
    1 - operators can be passed as arguments, thats cuz the C-Preprocessor doesn't care that
        they're not first class, just tokens
    2 - notice how we get `b` (subtrahend) off the stack first before `a` (minuend)
    3 - the while loop enables the macro substitution to work without syntax errors regarding
        ';'. it enables containing multiple statements in a block and also permits a ';'
*/
#define BINARY_OP(value_type, op) \
    do { \
        if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(value_type(a op b)); \
    } while (false)
    
    for (;;)
    {

#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (Value* slot = vm.stack; slot < vm.stack_top; slot++)
        {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk,(int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()){
            case OP_CONSTANT:
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            case OP_ADD: BINARY_OP(NUMBER_VAL, +); break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            /*pop negate push back the result*/
            case OP_NEGATE:
                if(!IS_NUMBER(peek(0))){
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
            case OP_RETURN:
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;

            
            default:
                break;
        }
    }

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_BYTE
}

static void runtime_error(const char* format, ...){
    va_list args;
    va_start(args,format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr,"[line %d] in script\n",line);
    reset_stack();
}