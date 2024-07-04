makelib: msocket.o
	ar -rcs libmsocket.a msocket.o
msocket.o: msocket.c
	gcc -c msocket.c -o msocket.o