#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char* source){
    init_scanner(source);
    int line = -1;
    for(;;){
        Token token = scan_token();
        if(token.line != line){
            printf("%4d ",token.line);
            line = token.line;
        }else{
            printf("    | ");
        }

        /*
            using '*' lets us pass the precision as an argument
            printf will call the first 'token.length' characters of
            the string at 'token.start'

            e.g., `eggs` token.start -> `e`, token.length -> 4
            therefore the lexeme `eggs` would be printed
        */
        printf("%2d '%.*s'\n",token.type, token.length, token.start);

        if(token.type == TOKEN_EOF) break;
    }
}