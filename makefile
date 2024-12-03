run_tests: build_tests
	./hashtable_tests

build_tests:
	gcc -I./ hashtable_tests.c hashtable.c -Wall -Wpedantic -o hashtable_tests
