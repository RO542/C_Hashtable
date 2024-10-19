test: build
	./hashtable.exe
build: hashtable.c hashtable.h
	gcc -o hashtable .\hashtable.c

build_strict:
	gcc -Wall -Wpedantic -Werror -o hashtable .\hashtable.c