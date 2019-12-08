%{
#include <stdio.h>
#include <stdarg.h>
#include "ast.h"

int yylex();
int yyerror(char *s);
%}

%token <str> T_SELECT T_INSERT T_UPDATE T_DELETE
%token <str> T_WHERE T_FROM T_SET
%token <str> T_EQ T_NEQ T_LT T_LTE T_GT T_GTE
%token <str> T_AND T_OR
%token <str> T_IDENTIFIER T_LITERAL
%token <num> T_NUMBER

%type <num> compare

%left T_AND T_OR

%union {
    int num;
    char *str;
    double d;
    ast* ast;
}

%%
topstmt:
      select_stmt ';'
    ;

select_stmt: 
      T_SELECT cols_stmt T_FROM T_IDENTIFIER T_WHERE where_clause 
    | T_SELECT cols_stmt T_FROM T_IDENTIFIER
    ;

where_clause:
      T_IDENTIFIER compare T_NUMBER {printf("%s %d %d\n", $1, $2, $3);}
    | T_IDENTIFIER compare T_LITERAL {printf("%s %d %s\n", $1, $2, $3);}
    | '(' where_clause ')'
    | where_clause T_AND where_clause
    | where_clause T_OR where_clause
    ;

cols_stmt: 
      T_IDENTIFIER {}
    | T_IDENTIFIER ',' cols_stmt {}
    ;
    
compare: 
      T_EQ {$$ = EQ;}
    | T_NEQ {$$ = NEQ;}
    | T_LT {$$ = LT;}
    | T_LTE {$$ = LTE;}
    | T_GT {$$ = GT;}
    | T_GTE {$$ = GTE;}
    ;

%%

int yyerror(char *s)
{
    extern int yylineno;
    extern char* yytext;
    printf("line %d: %s occur near '%s'\n", yylineno, s, yytext);
    return 1;
}

int main()
{
    yyparse();
    return 1;
}