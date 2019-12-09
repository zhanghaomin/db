%option noyywrap nodefault noyylineno case-insensitive 

%{
#include "ast.h"
#include "parser.tab.h"
#include "util.h"

static char* escape_str(char *s, int s_len, char c);
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
[a-zA-Z_][a-zA-Z0-9_]* {
    yylval.ast = create_val_ast(STRING, yytext);
    return T_IDENTIFIER;
}
[0-9]+ { 
    yylval.ast = create_val_ast(INTEGER, atoi(yytext));
    return T_NUMBER;
}
'([^']|\\')+'|\"([^"]|\\\")+\" {
    yylval.ast = create_val_ast(STRING, escape_str(yytext+1, yyleng-2, yytext[0]));
    return T_LITERAL;
}
[ \t\n] { /* skip */}
. {return *yytext;}
%%

static char* escape_str(char* s, int s_len, char c)
{
    char *new_s;
    int new_len = 0;

    new_s = smalloc(strlen(s) + 1);

    for (int i = 0; i < s_len; i++) {
        if (s[i] == '\\' && s[i+1] == c) {
            continue;
        }
        new_s[new_len++] = s[i];
    }

    new_s[new_len] = '\0';
    new_s = realloc(new_s, new_len+1);
    return new_s;
}