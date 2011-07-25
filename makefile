CS_LIBS=-L/usr/local/lib -L/home/wbhart/gc/lib 
CS_INC=-I/usr/local/include -I$(CS_GC_INC)
CS_FLAGS=-O2 -g -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS

all: parser.c symbol.o ast.o types.o unify.o environment.o backend.o cesium.c exception.o
	g++ $(CS_FLAGS) $(CS_INC) $(CS_LIBS) cesium.c symbol.o ast.o types.o unify.o environment.o backend.o exception.o -lgc `/usr/local/bin/llvm-config --libs --cflags --ldflags core analysis executionengine jit interpreter native` -o cs

parser.c: greg parser.leg
	greg-0.1/greg parser.leg -o parser.c

symbol.o: symbol.h symbol.c
	g++ -c $(CS_FLAGS) $(CS_INC) symbol.c -o symbol.o

ast.o: ast.h ast.c
	g++ -c $(CS_FLAGS) $(CS_INC) ast.c -o ast.o

types.o: types.h types.c
	g++ -c $(CS_FLAGS) $(CS_INC) types.c -o types.o

unify.o: unify.h unify.c
	g++ -c $(CS_FLAGS) $(CS_INC) unify.c -o unify.o

environment.o: environment.h environment.c
	g++ -c $(CS_FLAGS) $(CS_INC) environment.c -o environment.o

backend.o: backend.h backend.c
	g++ -c $(CS_FLAGS) $(CS_INC) backend.c -o backend.o

exception.o: exception.h exception.c
	g++ -c $(CS_FLAGS) $(CS_INC) exception.c -o exception.o

greg:
	$(MAKE) -C greg-0.1

clean:
	rm *.o
	rm greg-0.1/*.o
	rm cs
	rm greg-0.1/greg

