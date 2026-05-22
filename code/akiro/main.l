%option nounistd
%option never-interactive
%option noyywrap

%{
#include <stdio.h>

//void expect()

%}

%x EXPECT_STRUCT_NAME EXPECT_STRUCT_BASE_TYPES EXPECT_STRUCT_DATA_MEMBER_TYPE EXPECT_STRUCT_DATA_MEMBER_NAME

TEMPLATE_DECL "<"[[:alnum:],*<> ]+">"
POINTER_DECL [[:space:]*]
ARRAY_DECL "["[[:alnum:]*+, ]+"]"

%%

<INITIAL>struct[[:space:]]+                 { BEGIN(EXPECT_STRUCT_NAME); }
<INITIAL>[[:space:]]                        /*THIS REMOVES THE SPACES IN WRONG PLACES eat up the whitespaces*/

<EXPECT_STRUCT_NAME>[[:alnum:]]+                    { printf("\nBEGIN_STRUCT_NAME:%s\n", yytext); BEGIN(EXPECT_STRUCT_BASE_TYPES); }
<EXPECT_STRUCT_BASE_TYPES>[[:space:]]*:*            /*eat up the :*/
<EXPECT_STRUCT_BASE_TYPES>[[:alnum:]]+{TEMPLATE_DECL}*            { printf("STRUCT_INHERITANCE_DECL:%s\n", yytext); }

<EXPECT_STRUCT_BASE_TYPES>,+                        /*eat up the ,*/
<EXPECT_STRUCT_BASE_TYPES>[\r\n\t\f\v ]+\{          { BEGIN(EXPECT_STRUCT_DATA_MEMBER_TYPE); }

<EXPECT_STRUCT_DATA_MEMBER_TYPE>[[:space:]]         /*eat up the whitespace*/
<EXPECT_STRUCT_DATA_MEMBER_TYPE>[[:alnum:]]+{TEMPLATE_DECL}*{POINTER_DECL}*{ARRAY_DECL}*{POINTER_DECL}* { printf("EXPECT_STRUCT_DATA_MEMBER_TYPE:%s\n", yytext); BEGIN(EXPECT_STRUCT_DATA_MEMBER_NAME); }
<EXPECT_STRUCT_DATA_MEMBER_TYPE>\}                  { BEGIN(INITIAL); }
<EXPECT_STRUCT_DATA_MEMBER_TYPE>"//"[^\r\n]*        /*eat up the single line comments*/

<EXPECT_STRUCT_DATA_MEMBER_NAME>[[:space:]]         /*eat up the whitespace*/
<EXPECT_STRUCT_DATA_MEMBER_NAME>[[:alnum:]]+        { printf("EXPECT_STRUCT_DATA_MEMBER_NAME:%s\n\n", yytext); BEGIN(EXPECT_STRUCT_DATA_MEMBER_NAME); }
<EXPECT_STRUCT_DATA_MEMBER_NAME>[[:space:]*;]       { BEGIN(EXPECT_STRUCT_DATA_MEMBER_TYPE); }
%%

int main(int argc, char **argv)
{
    if(argc > 1)
    {
        printf("akiro: processing %s into %s\n", argv[1], argv[2]);
        yyin = fopen(argv[1], "r");
        yylex();
        fclose(yyin);
    }

    return 0;
}
