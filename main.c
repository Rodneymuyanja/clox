#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char * argv[]){
    init_vm();
   
    if(argc == 1){

    }else if(argc == 2){

    }else{
        /*
            stderr is found in #include <stdio.h>
         */
        fprintf(stderr, "Usage:clox [path]\n" );
        exit(64);
    }

    free_vm();
    return 0;
}

static void repl(){
    char line[1024];
    for(;;){
        printf("> ");
        /*
            fgets stops reading from the source
            [stdio] when it encounters a \n
        */
        if(!fgets(line,sizeof(line),stdin)){
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static void run_file(const char* path){
    char* source  = read_file(path);
    InterpretResult result = interpret(source);
    free(source);

    if(result == INTERPRET_COMPILE_ERROR) exit(65);
    if(result == INTERPRET_RUNTIME_ERROR) exit(70);
}

static char* read_file(const char* path){
    /*
        read file in Binary mode
    */
    FILE* file = fopen(path,"rb");
    if(file == NULL){
        fprintf(stderr,"Could not open file \"%s\".\n",path);
        exit(74);
    }
    /*
        set position indicator 0L bytes from
        the end of the stream
    */
    fseek(file, 0L, SEEK_END);
    /*
        in binary mode "rb", this will
        return the number of bytes
        from beginning of the file
    */
    size_t file_size = ftell(file);
    /*
        sets position to the beginning of
        the stream
    */
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if(buffer == NULL){
        fprintf(stderr,"Not enough memory to read \"%s\".\n",path);
        exit(74);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if(bytes_read < file_size){
        fprintf(stderr,"Could not read file \"%s\".\n",path);
        exit(74);
    }
    
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}