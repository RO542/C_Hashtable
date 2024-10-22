#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "test_cases.h"
extern char *test_arr[]; // test_cases.h

#define QUAD_PROBING
#include "hashtable.h"


int main() {
    Hashtable *ht = hashtable_create(sizeof(int), 1);
    assert(hashtable_empty(ht) == true);
    printf("PASSED: newly created hashmap hashmap_empty == true passed\n");


    int num_to_test = 1000;
    for (int i = 0; i < num_to_test; i++) {
        assert(hashtable_put(ht, test_arr[i], &i) == true);
    }
    printf("PASSED: put 1000 elements into hashtable no errors\n");


    for (int i = 0; i < num_to_test; i++) {
        assert(hashtable_contains(ht, test_arr[i]) == true);
    }
    printf("PASSED: hashtable_contains returns true for the 1000 previously inserted elements\n");


    int get_counter = 0;
    for (int i = 0; i < num_to_test; i++) {
        if (hashtable_get(ht, test_arr[i]) != NULL) {
            get_counter++;
        }
    }
    assert(get_counter == num_to_test);
    printf("PASSED: 1000 calls hashtable_get with non NULL returns\n");

    int manual_count = 0;
    for (int i = 0; i < ht->capacity; i++) {
        if (ht->arr[i].state == ENTRY_USED) {
            manual_count++;
        }
    }
    assert(manual_count == get_counter);
    assert(manual_count == num_to_test);
    printf("PASSED: Manually verified %d ENTRY_USED in hashtable\n", num_to_test);


    for (int i = 0; i < num_to_test / 2; i++) {
        hashtable_remove(ht, test_arr[i]);
    }
    assert(ht->count == num_to_test / 2);
    assert(ht->count == 500);
    printf("PASSED Removed %d elements from hashtable, count is now : %d \n", num_to_test / 2, ht->count);


    hashtable_clear(ht);
    assert(ht->count == 0);
    assert(hashtable_empty(ht) == true);
    printf("PASSED: hashtable_clear makes the total count 0\n");


    // multiple destroy calls should cause no issues
    hashtable_destroy(ht);
    hashtable_destroy(ht);
    hashtable_destroy(ht);
    return 0;
}
