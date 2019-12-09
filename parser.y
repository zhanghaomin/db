%{
#include <stdio.h>
#include <stdarg.h>
#include "ast.h"

int yylex();
int yyerror(char *s);
%}

%token T_SELECT T_INSERT T_UPDATE T_DELETE
%token T_WHERE T_FROM T_SET
%token T_EQ T_NEQ T_LT T_LTE T_GT T_GTE
%token T_AND T_OR
%token <ast> T_IDENTIFIER T_LITERAL
%token <ast> T_NUMBER

%type <num> compare
%type <ast> topstmt select_stmt where_clause cols_stmt

%left T_AND T_OR

%union {
    int num;
    ast* ast;
}

%%
topstmt:
      select_stmt ';' { $$ = $1; G_AST = $$; }
    ;

select_stmt: 
      T_SELECT cols_stmt T_FROM T_IDENTIFIER T_WHERE where_clause { $$ = create_ast(3, AST_SELECT, -1, $2, $4, $6); }
    | T_SELECT cols_stmt T_FROM T_IDENTIFIER { $$ = create_ast(2, AST_SELECT, -1, $2, $4); }
    ;

where_clause:
      T_IDENTIFIER compare T_NUMBER { $$ = create_ast(2, AST_WHERE_LEAF, $2, $1, $3); }
    | T_IDENTIFIER compare T_LITERAL { $$ = create_ast(2, AST_WHERE_LEAF, $2, $1, $3); }
    | '(' where_clause ')' { $$ = $2; }
    | where_clause T_AND where_clause { $$ = create_ast(2, AST_WHERE_NODE, W_AND, $1, $3); }
    | where_clause T_OR where_clause { $$ = create_ast(2, AST_WHERE_NODE, W_OR, $1, $3); }
    ;

cols_stmt: 
      T_IDENTIFIER { $$ = create_ast(1, AST_EXPECT_COLS, -1, $1);}
    | T_IDENTIFIER ',' cols_stmt { $$ = $3;ast_add_child($$, $1); }
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