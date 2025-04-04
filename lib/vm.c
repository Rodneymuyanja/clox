#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "compiler.h"
#include "time.h"

VM vm;

static void reset_stack();
static InterpretResult run();
static void runtime_error(const char* format, ...);
static void concatenate();
static bool is_falsey(Value value);

static Value clock_native(int arg_count, Value* args){
    return NUMBER_VAL((double)clock() /CLOCKS_PER_SEC);
}

void init_vm(){
    reset_stack();
    vm.objects = NULL;
    init_table(&vm.strings);
    init_table(&vm.globals);
    define_native("clock", clock_native);
}

void free_vm(){
    free_table(&vm.strings);
    free_table(&vm.globals);
    free_objects();
}

static bool call(ObjFunction* function, int argument_count){
    //check if arity is fine
    if(argument_count > function->arity){
        runtime_error("Too many arguments passed to %s, expected %s but got %s", 
            function->name, function->arity, argument_count);
        return false;
    }

    //at the moment we only support upto 64 frames
    if(vm.frame_count >= FRAMES_MAX){
        runtime_error("Stack overflow, too many calls.");
        return false;
    }

    CallFrame* callframe = new_function(function);
    callframe->function = function;
    callframe->ip = function->chunk.code;

    //the callframe is at the top of the VM's stack, 
    //it's so the callee is the in the slot zero of this callframe
    //and it's arguments follow, this is in the perspertive of the callee anyway
    //not the VM, to the VM, it's the SCRIPT in the slot zero
    //the -1 is to actually hit callframe slot 0
    callframe->slots = vm.stack_top - argument_count - 1;

    return true;
}


InterpretResult interpret(const char* source){
    ObjFunction* function = compile(source);
    if(function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));

    call(function,0);

    return run();
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
    vm.frame_count = 0;
}


static bool call_value(Value callee, int arg_count){
    if(!IS_OBJ(callee)){
        runtime_error("Only callables can actually be called i.e functions and classes can be called");
        return false;
    }

    switch (OBJ_TYPE(callee)){
        case OBJ_FUNCTION:
            return call(AS_FUNCTION(callee), arg_count);
            break;
        case OBJ_NATIVE:{
            NativeFn native = AS_NATIVE(callee);
            Value result = native(arg_count, vm.stack_top - arg_count);
            vm.stack_top -= arg_count + 1;
            push(result);
            return true;
        }

        default:
            break;
    }
}

static InterpretResult run(){
    CallFrame* frame = &vm.frames[vm.frame_count - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
        (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8 | frame->ip[-1])))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
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
        disassemble_instruction(&frame->function->chunk,(int)(frame->ip - frame->function->chunk.code));
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
            case OP_PRINT:{
                print_value(pop());
                printf("\n");
                break;
            }
            case OP_RETURN:
                Value result = pop();
                vm.frame_count--;
                if(vm.frame_count == 0){
                    pop();
                    return INTERPRET_OK;
                }
                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;

            case OP_POP: pop(); break;
            case OP_GET_LOCAL:{
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            
            case OP_DEFINE_GLOBAL:{
                ObjString* name = READ_STRING();
                table_set(&vm.globals, name, peek(0));
                pop();
            }

            case OP_SET_LOCAL:{
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }

            case OP_GET_GLOBAL:{
                ObjString* name = READ_STRING();
                Value value;
                if(!table_get(&vm.globals,name,&value)){
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }

            case OP_SET_GLOBAL:{
                ObjString* name = READ_STRING();
                if(table_set(&vm.globals,name,peek(0))){
                    table_delete(&vm.globals, name);
                    runtime_error("Setting Undefined variable '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }

            case OP_JUMP_IF_FALSE:{
                uint16_t offset = READ_SHORT();
                if(is_falsey(peek(0))) frame->ip += offset;
                break;
            }

            case OP_JUMP:{
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }

            case OP_LOOP:{
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            case OP_CALL:{
                uint8_t arg_count = READ_BYTE();
                if(!call_value(peek(arg_count), arg_count)){
                    return INTERPRET_RUNTIME_ERROR;
                }

                frame = &vm.frames[vm.frame_count - 1];
                break;
            }

            default:
                break;
        }
    }

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_BYTE
#undef READ_SHORT
#undef READ_STRING
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

    for (int i = vm.frame_count - 1; i > 0; i--){
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;

        size_t instruction = frame->ip - function->chunk.code -1;
        fprintf(stderr,"[line %d]", function->chunk.lines[instruction]);

        if(function->name == NULL){
            fprintf(stderr," script\n");
        }else{
            fprintf(stderr," %s()", function->name->chars);
        }
    }
    
    reset_stack();
}

static void define_native(const char* name, NativeFn function){
    push(OBJ_VAL(copy_string(name,(int)strlen(name))));
    push(OBJ_VAL(new_native(function)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}