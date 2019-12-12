%option noyywrap nodefault noyylineno case-insensitive nounput noinput

%{
#include "ast.h"
#include "parser.tab.h"
#include "util.h"

static char* escape_str(char *s, int s_len, char c);
int yyerror(char *s);
%}

%%
"select" {return T_SELECT;}
"insert" {return T_INSERT;}
"create" {return T_CREATE;}
"into" {return T_INTO;}
"values" {return T_VALUES;}
"update" {return T_UPDATE;}
"delete" {return T_DELETE;}
"where" {return T_WHERE;}
"from" {return T_FROM;}
"and" {return T_AND;}
"table" {return T_TABLE;}
"order" {return T_ORDER;}
"by" {return T_BY;}
"limit" {return T_LIMIT;}
"or" {return T_OR;}
"=" {return T_EQ;}
"int" {return T_INT;}
"double" {return T_DOUBLE;}
"varchar" {return T_VARCHAR;}
"char" {return T_CHAR;}
"!="|"<>" {return T_NEQ;}
"<" {return T_LT;}
"<=" {return T_LTE;}
">" {return T_GT;}
">=" {return T_GTE;}
","|";"|"("|")" {return *yytext;}

[a-zA-Z_][a-zA-Z0-9_]* {
    yylval.ast = (Ast *)create_val_ast(STRING, yytext);
    return T_IDENTIFIER;
}
`[a-zA-Z_][a-zA-Z0-9_]*`|'[a-zA-Z_][a-zA-Z0-9_]*'|\"[a-zA-Z_][a-zA-Z0-9_]*\" {
    char *s;
    s = strndup(yytext+1, yyleng-2);
    yylval.ast = (Ast *)create_val_ast(STRING, s);
    free(s);
    return T_IDENTIFIER;
}
-?[0-9]+ { 
    yylval.ast = (Ast *)create_val_ast(INTEGER, atoi(yytext));
    return T_NUMBER;
}
-?[0-9]+"."[0-9]+ { 
    yylval.ast = (Ast *)create_val_ast(DOUBLE, atof(yytext));
    return T_NUMBER;
}
'([^']|\\')+'|\"([^"]|\\\")+\" {
    yylval.ast = (Ast *)create_val_ast(STRING, escape_str(yytext+1, yyleng-2, yytext[0]));
    return T_LITERAL;
}
[ \t\n] { /* skip */}
. { }
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