%option nounistd
%option never-interactive
%option noyywrap

%{
#include <iostream>
#include <stdio.h>
%}

%%

struct 

%%

int main(int argc, char **argv)
{
    if(argc > 1)
    {
        printf("akiro: processing %s into %s\n", argv[1], argv[2]);
        yyin = fopen(argv[1], "r");
        yylex();
    }

    return 0;
}
