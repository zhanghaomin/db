tmp = parser.tab.c lex.yy.c

db: $(tmp)
	gcc -o db $(tmp) ast.c util.c

parser.tab.c: 
	bison -d parser.y
lex.yy.c: 
	flex scanner.l

clean:
	rm db parser.tab.h $(tmp)