#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "test_cases.h"
extern char *test_arr[]; // test_cases.h

#define QUAD_PROBING
#define TARGET_LOAD_FACTOR 0.65
#include "hashtable.h"


int main() {
    Hashtable *ht1 = hashtable_create(int, int, 10);
    for (int i = 0; i < 1000; i++) {
        assert(hashtable_put(ht1, &i, &i));
    }
    assert(ht1->count == 1000);
    assert(hashtable_count(ht1) == 1000);
    printf("Passed test for 1000 put calls, and contains calls, count is 1000\n");

    int manual_count = 0;
    for (int i = 0; i < ht1->capacity; i++) {
        if (ht1->arr[i].state == ENTRY_USED) {
            bool matched_key = false;
            // brute force check all the hashtable entries for this single key
            for (int j = 0; j < 1000; j++) {
                if (j == *(int *)ht1->arr[i].key) {
                    matched_key = true;
                    manual_count++;
                }
            }
            assert(matched_key);
        }
    }
    assert(hashtable_empty(ht1) == false);
    assert(manual_count == hashtable_count(ht1));
    assert(manual_count == ht1->count);
    assert(manual_count == 1000);
    printf("Passed brute force test for key matches after 1000 puts, count to 1000 elements, all keys matched\n");


    // fails assert here
    for (int i = 0; i < 1000; i++) {
        assert(hashtable_contains(ht1, &i));
        int *out_find = (int *)hashtable_find(ht1, &i);
        assert(out_find != NULL);
        assert(*out_find == i);
    }
    printf("passed test for 1000 hashtable_contains/hashtable_find calls calls\n");


    for (int i = 0; i < 250; i++) {
        unsigned int old_cnt = ht1->count;
        hashtable_remove(ht1, &i);
        assert(ht1->count == old_cnt -1);
        assert(hashtable_contains(ht1, &i) == false);
    }
    assert(hashtable_count(ht1) == 750);
    assert(ht1->count == 750);
    printf("Passed test removed first 250 elements and verified removal hashtable_contains\n");


    HTIterator itr;
    int itr_cnt = 0;
    for (const Hashentry *entry = HTIterator_start(&itr, ht1); entry != NULL; entry = HTIterator_next(&itr)) {
        assert(entry);
        assert(entry->key);
        assert(entry->value);
        itr_cnt++;
        printf("Iterator: %d -> %d\n", *(int *)entry->key, *(int *)entry->value);
    }

    assert(itr_cnt == hashtable_count(ht1));
    printf("Passed tests for HTIterator called to exhasution, all %u key/val pairs found\n", ht1->count);


    // alternative way to iterate all key value pairs
    for (unsigned int i = 0; i < ht1->capacity; i++) {
        Hashentry entry = ht1->arr[i];
        if (entry.state == ENTRY_USED) {

        }
    }



    printf("All Hashtable tests/asserts passed\n");
    return 0;
}
