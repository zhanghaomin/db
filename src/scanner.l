%option noyywrap nodefault noyylineno case-insensitive nounput noinput

%{
#include "include/ast.h"
#include "include/parser.h"
#include "include/util.h"

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
"set" {return T_SET;}
"delete" {return T_DELETE;}
"where" {return T_WHERE;}
"from" {return T_FROM;}
"table" {return T_TABLE;}
"order" {return T_ORDER;}
"by" {return T_BY;}
"limit" {return T_LIMIT;}
"int" {return T_INT;}
"double" {return T_DOUBLE;}
"varchar" {return T_VARCHAR;}
"char" {return T_CHAR;}
"asc" {return T_ASC;}
"desc" {return T_DESC;}
"drop" {return T_DROP;}

"and" {return T_AND;}
"or" {return T_OR;}
","|";"|"("|")" {return *yytext;}
"+"|"-"|"*"|"/"|"%"|"&"|"|"|"~"|"<"|">"|"="  {return *yytext;}
"not"|"!" {return T_NOT;}
"!="|"<>" {return T_NEQ;}
"<=" {return T_LTE;}
">=" {return T_GTE;}

[a-zA-Z_][a-zA-Z0-9_]* {
    yylval.ast = (Ast *)CREATE_AV(AVT_ID, yytext);
    return T_IDENTIFIER;
}
`([^`]|\\`)+` {
    char *s;
    s = escape_str(yytext+1, yyleng-2, yytext[0]);
    yylval.ast = (Ast *)CREATE_AV(AVT_ID, s);
    free(s);
    return T_IDENTIFIER;
}
-?[0-9]+ { 
    yylval.ast = (Ast *)CREATE_AV(AVT_INT, atoi(yytext));
    return T_NUMBER;
}
-?[0-9]+"."[0-9]+ { 
    yylval.ast = (Ast *)CREATE_AV(AVT_DOUBLE, atof(yytext));
    return T_NUMBER;
}
'([^']|\\')*'|\"([^"]|\\\")*\" {
    char *s;
    s = escape_str(yytext+1, yyleng-2, yytext[0]);
    yylval.ast = (Ast *)CREATE_AV(AVT_STR, s);
    free(s);
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

int lex_read_line(FILE* fp, char *s, int len)
{
    YY_BUFFER_STATE bp;
    int res;
    fgets(s, len, fp); //读取一行
    bp = yy_scan_bytes(s, len);
    yy_switch_to_buffer(bp);
    res = yyparse();
    yy_delete_buffer(bp);
    return res;
}

int lex_read(char *s, int len)
{
    YY_BUFFER_STATE bp;
    int res;
    bp = yy_scan_bytes(s, len);
    yy_switch_to_buffer(bp);
    res = yyparse();
    yy_delete_buffer(bp);
    return res;
}