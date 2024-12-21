#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "compiler.h"

VM vm;
static void reset_stack();
static InterpretResult run();
static void runtime_error(const char* format, ...);
static void concatenate();
static bool is_falsey(Value value);

void init_vm(){
    reset_stack();
    vm.objects = NULL;
}

void free_vm(){
    free_objects();
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
         _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 
        |     |     |     |     |     |     |     |_ _ _ _ _
        |_ _ _|_ _ _| _ _ |_ _ _|_ _ _|_ _ _|_ _ _|
                 
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
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break; 
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL,<); break;
            case OP_ADD: {
                if(IS_STRING(peek(0)) && IS_NUMBER(peek(1))){
                    concatenate();
                }else if(IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))){
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                }else{
                    runtime_error("Operands must be two numbers or two strings");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(is_falsey(pop())));
                break;
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


static bool is_falsey(Value value){
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(){
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    push(OBJ_VAL(result));
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