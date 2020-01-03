CC = gcc
CFLAGS = -I. -g -Wall -W
IDIR = include

LEX = flex
DFT_PARSER_H = parser.h
YACC = bison --defines=$(IDIR)/$(DFT_PARSER_H) -y

TDIR = tests
SDIR = src
ODIR = src/obj

_TOBJ = ast_test.o create_insert_test.o persist_test.o select_test.o update_test.o delete_test.o
TOBJ = $(patsubst %, $(ODIR)/%, $(_TOBJ))

_OBJ = ast.o ht.o parser.o scanner.o table.o util.o cursor.o pager.o fmt.o row.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

_DOBJ = ast.o ht.o parser.o scanner.o table.o util.o cursor.o pager.o fmt.o cli.o row.o linenoise.o
DOBJ = $(patsubst %, $(ODIR)/%, $(_DOBJ))

_DEP = ast.h ht.h table.h util.h linenoise.h
DEP = $(patsubst %, $(IDIR)/%, $(_DEP))

$(ODIR)/%.o: $(SDIR)/%.c $(DEP)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/%_test.o: $(TDIR)/%_test.c
	$(CC) $(CFLAGS) -c -o $@ $<

db: $(DOBJ)
	$(CC) $(CFLAGS) -o $@ $(DOBJ)

test: $(OBJ) $(TOBJ)
	for i in $(TOBJ); do \
		$(CC) $(CFLAGS) -o $@ $(OBJ) $$i && ./$@; \
	done

scanner.o: scanner.l

parser.o: parser.y

.PHONY: clean

clean:
	rm -rf test *.dat $(ODIR)/*.o $(IDIR)/$(DFT_PARSER_H)

cleandat:
	rm -rf *.dat