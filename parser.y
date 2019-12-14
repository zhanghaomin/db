%{
#include <stdio.h>
#include <stdarg.h>
#include "ast.h"
#include "util.h"

int yylex();
int yyerror(char *s);
%}

%token T_SELECT T_INSERT T_INTO T_VALUES T_UPDATE T_DELETE T_ORDER T_BY T_ASC T_DESC
%token T_WHERE T_FROM T_SET T_CREATE T_TABLE T_LIMIT
%token T_NEQ T_LTE T_GTE T_NOT
%token T_INT T_CHAR T_VARCHAR T_DOUBLE
%token T_AND T_OR
%token <ast> T_IDENTIFIER T_LITERAL
%token <ast> T_NUMBER

%type <num> col_type
%type <ast> topstmt select_stmt where expr cols_stmt table_name create_table_stmt order_by order_by_col_list order_by_col limit limit_clause
%type <ast> col_fmt_stmt col_fmt_list_stmt insert_row_list_stmt insert_value_list_stmt insert_stmt insert_value

%left T_OR
%left T_AND
%left T_NOT
%left "=" "<" ">" T_LTE T_GTE T_NEQ
%left "+" "-" "&" "^" "|" 
%left "*" "/" "%"
%left "~"


%union {
    int num;
    Ast* ast;
}

%destructor { ast_destory($$); } <ast>

%%
topstmt:
      select_stmt ';' { $$ = $1; G_AST = $$; YYACCEPT;}
    | create_table_stmt ';' { $$ = $1; G_AST = $$; YYACCEPT; }
    | insert_stmt ';' { $$ = $1; G_AST = $$; YYACCEPT; }
    ;

insert_stmt:
      T_INSERT T_INTO table_name T_VALUES insert_row_list_stmt { $$ = create_ast(2, AST_INSERT, -1,  $3, $5); }
    ;

insert_row_list_stmt:
      '(' insert_value_list_stmt ')' { $$ = create_ast(1, AST_INSERT_ROW_LIST, -1, $2); }
    | insert_row_list_stmt ',' '(' insert_value_list_stmt ')' { $$ = ast_add_child($1, $4); }
    ;

insert_value_list_stmt:
      insert_value { $$ = create_ast(1, AST_INSERT_VAL_LIST, -1, $1); }
    | insert_value_list_stmt ',' insert_value { $$ = ast_add_child($1, $3); }
    ;

insert_value:
      T_NUMBER { $$ = $1; }
    | T_LITERAL { $$ = $1; }
    ;

create_table_stmt:
     T_CREATE T_TABLE table_name '(' col_fmt_list_stmt ')' { $$ = create_ast(2, AST_CREATE, -1, $3, $5); }
    ;

col_fmt_list_stmt:
      col_fmt_stmt { $$ = create_ast(1, AST_COL_FMT_LIST, -1, $1); }
    | col_fmt_list_stmt ',' col_fmt_stmt { $$ = ast_add_child($1, $3); }
    ;

col_fmt_stmt:
      T_IDENTIFIER col_type '(' T_NUMBER ')' { $$ = create_ast(2, AST_COL_FMT, $2, $1, $4); }
    | T_LITERAL col_type '(' T_NUMBER ')' { $$ = create_ast(2, AST_COL_FMT, $2, $1, $4); }
    ;

col_type: 
      T_INT { $$ = C_INT; }
    | T_VARCHAR { $$ = C_VARCHAR; }
    | T_CHAR { $$ = C_CHAR; }
    | T_DOUBLE { $$ = C_DOUBLE; }
    ;

select_stmt: 
      T_SELECT cols_stmt T_FROM table_name where order_by limit { $$ = create_ast(5, AST_SELECT, -1, $2, $4, $5, $6, $7); }
    ;

order_by:
      /* empty */ { $$ = NULL; }
    | T_ORDER T_BY order_by_col_list { $$ = $3; }
    ;

order_by_col_list:
      order_by_col { $$ = create_ast(1, AST_ORDER_BY, -1,  $1); }
    | order_by_col_list ',' order_by_col { $$ = ast_add_child($1, $3); }
    ;

order_by_col:
      T_IDENTIFIER { $$ = create_ast(1, AST_ORDER_BY_COL, 0, $1); }
    | T_IDENTIFIER T_ASC { $$ = create_ast(1, AST_ORDER_BY_COL, 0, $1); }
    | T_IDENTIFIER T_DESC { $$ = create_ast(1, AST_ORDER_BY_COL, 1, $1); }
    | T_LITERAL { $$ = create_ast(1, AST_ORDER_BY_COL, 0, $1); }
    | T_LITERAL T_ASC { $$ = create_ast(1, AST_ORDER_BY_COL, 0, $1); }
    | T_LITERAL T_DESC { $$ = create_ast(1, AST_ORDER_BY_COL, 1, $1); }
    ;

limit:
      /* empty */ { $$ = NULL; }
    | T_LIMIT limit_clause { $$ = $2; }
    ;

limit_clause:
      T_NUMBER { $$ = create_ast(2, AST_LIMIT, -1, (Ast *)create_val_ast(INTEGER, 0), $1); }
    | T_NUMBER ',' T_NUMBER { $$ = create_ast(2, AST_LIMIT, -1, $1, $3); }
    ;

table_name:
      T_IDENTIFIER { $$ = create_ast(1, AST_TABLE, -1, $1); }
    | T_LITERAL { $$ = create_ast(1, AST_TABLE, -1, $1); }
    ;

where:
      /* empty */ { $$ = NULL; }
    | T_WHERE expr { $$ = create_ast(1, AST_WHERE_TOP_EXP, -1, $2); }
    ;

expr:
      T_NUMBER { $$ = $1; }
    | T_LITERAL { $$ = $1; }
    | T_IDENTIFIER { $$ = $1; }
    | expr '+' expr { $$ = create_ast(2, AST_WHERE_EXP, E_ADD, $1, $3); }
    | expr '-' expr { $$ = create_ast(2, AST_WHERE_EXP, E_SUB, $1, $3); }
    | expr '*' expr { $$ = create_ast(2, AST_WHERE_EXP, E_MUL, $1, $3); }
    | expr '/' expr { $$ = create_ast(2, AST_WHERE_EXP, E_DIV, $1, $3); }
    | expr '%' expr { $$ = create_ast(2, AST_WHERE_EXP, E_MOD, $1, $3); }
    | expr '&' expr { $$ = create_ast(2, AST_WHERE_EXP, E_B_AND, $1, $3); }
    | expr '^' expr { $$ = create_ast(2, AST_WHERE_EXP, E_B_XOR, $1, $3); }
    | expr '|' expr { $$ = create_ast(2, AST_WHERE_EXP, E_B_OR, $1, $3); }
    | '~' expr { $$ = create_ast(1, AST_WHERE_EXP, E_B_NOT, $2); }
    | expr '=' expr { $$ = create_ast(2, AST_WHERE_EXP, E_EQ, $1, $3); }
    | expr T_NEQ expr { $$ = create_ast(2, AST_WHERE_EXP, E_NEQ, $1, $3); }
    | expr '>' expr { $$ = create_ast(2, AST_WHERE_EXP, E_GT, $1, $3); }
    | expr T_GTE expr { $$ = create_ast(2, AST_WHERE_EXP, E_GTE, $1, $3); }
    | expr '<' expr { $$ = create_ast(2, AST_WHERE_EXP, E_LT, $1, $3); }
    | expr T_LTE expr { $$ = create_ast(2, AST_WHERE_EXP, E_LTE, $1, $3); }
    | expr T_AND expr { $$ = create_ast(2, AST_WHERE_EXP, E_AND, $1, $3); }
    | expr T_OR expr { $$ = create_ast(2, AST_WHERE_EXP, E_OR, $1, $3); }
    | T_NOT expr { $$ = create_ast(1, AST_WHERE_EXP, E_NOT, $2); }
    | '(' expr ')' { $$ = $2; }
    ;

cols_stmt: 
      T_IDENTIFIER { $$ = create_ast(1, AST_EXPECT_COLS, -1, $1);}
    | cols_stmt ',' T_IDENTIFIER { $$ = ast_add_child($1, $3); }
    | T_LITERAL { $$ = create_ast(1, AST_EXPECT_COLS, -1, $1);}
    | cols_stmt ',' T_LITERAL { $$ = ast_add_child($1, $3); }
    ;

%%

int yyerror(char *s)
{
    extern int yylineno;
    extern char* yytext;
    printf("line %d: %s occur near \" %s\n \"", yylineno, s, yytext);
    return 1;
}