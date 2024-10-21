build_tests:
	gcc -I./ hashtable_tests.c hashtable.c -Wall -Wpedantic -Werror -o hashtable_tests

run_tests:
	./hashtable_tests