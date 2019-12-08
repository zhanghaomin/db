%option noyywrap nodefault noyylineno case-insensitive 

%{
#include "ast.h"
#include "parser.tab.h"
%}

%%
"select" {return T_SELECT;}
"insert" {return T_INSERT;}
"update" {return T_UPDATE;}
"delete" {return T_DELETE;}
"where" {return T_WHERE;}
"from" {return T_FROM;}
"and" {return T_AND;}
"or" {return T_OR;}
"=" {return T_EQ;}
"!="|"<>" {return T_NEQ;}
"<" {return T_LT;}
"<=" {return T_LTE;}
">" {return T_GT;}
">=" {return T_GTE;}
[a-zA-Z_][a-zA-Z0-9_]* {yylval.str = strdup(yytext); return T_IDENTIFIER;}
[0-9]+ {yylval.num = atoi(yytext);return T_NUMBER;}
'([^']|\\')+' {yylval.str = strdup(yytext); return T_LITERAL;}
[ \t\n]
. {return *yytext;}
%%