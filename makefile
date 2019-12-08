objects = parser.tab.c lex.yy.c

db: $(objects)
	gcc -o db $(objects)

parser.tab.c: 
	bison -d parser.y
lex.yy.c: 
	flex scanner.l

clean:
	rm db parser.tab.h $(objects)