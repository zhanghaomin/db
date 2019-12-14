tmp = parser.tab.c lex.yy.c
common = ht.c util.c ast.c 

db: $(tmp)
	gcc -Wall -W -g -o db $(tmp) $(common) cli.c

test_table: $(tmp)
	gcc -Wall -W -g -o table_test table_test.c table.c $(common) $(tmp) && ./table_test

test_persist: $(tmp)
	gcc -Wall -W -g -o persist_test persist_test.c table.c $(common) $(tmp) && ./persist_test

test_ast: $(tmp)
	gcc -Wall -W -g -o ast_test ast_test.c table.c $(common) $(tmp) && ./ast_test

parser.tab.c: 
	bison -d parser.y
lex.yy.c: 
	flex scanner.l

clean:
	rm -f db ast_test table_test persist_test parser.tab.h $(tmp)