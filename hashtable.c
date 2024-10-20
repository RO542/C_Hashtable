#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "hashtable.h"
#define ht_create(type, base_cap) hashtable_create(sizeof(type), base_cap);

bool hashtable_init(Hashtable *ht, const size_t value_size, const unsigned int base_capacity) {
    if (ht == NULL) {
        fprintf(stderr, "Hashtable is NULL, unable to initialize.\n");
        return false;
    }
    if (value_size < 1 || base_capacity < 1) {
        fprintf(stderr, "To use hashtable_int both the value_size, and capacity must be positive\n");
        return false;
    }
    ht->count = 0;
    ht->capacity = base_capacity;
    ht->element_size = value_size; // size of the stored elements themselves in bytes not the Hashentries
    ht->arr = (Hashentry *)malloc(ht->capacity * sizeof(Hashentry));
    if (!ht->arr) {
        fprintf(stderr, "Unable to allocate memory for Hashtable entries");
        return false;
    }
    for (unsigned int i = 0; i < ht->capacity; i++) {
        ht->arr[i].state = UNUSED;
        ht->arr[i].stored_hash = 0;
        ht->arr[i].key = NULL;
        ht->arr[i].value = NULL;
    }
    return true;
}

void hashtable_deinit(Hashtable *ht) {
    if (!ht || !ht->arr) {
        return;
    }
    for (unsigned int i = 0; i < ht->capacity; i++) {
        if (ht->arr[i].state == USED || ht->arr[i].state == DELETED) {
            free(ht->arr[i].key);
            free(ht->arr[i].value);
            ht->arr[i].key = NULL;
            ht->arr[i].value = NULL;
        }
    }
    free(ht->arr);
    ht->arr = NULL;
}

struct Hashtable *hashtable_create(size_t element_size, unsigned int new_cap) {
    Hashtable *ht = (Hashtable *)malloc(sizeof(Hashtable));
    if (!ht) {
        fprintf(stderr, "Failed to allocate memory for ht during hashtable_create\n");
        return NULL;
    }
    if (!hashtable_init(ht, element_size, new_cap)) { 
        fprintf(stderr, "failed to initialize hashtable during hashtableinit, aborting ...\n");
        free(ht);
        ht = NULL;
    }
    return ht;
}

unsigned long hash_func(const char *key_str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key_str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

// probe the next open/usable index in the Hashtable
ProbeResult probe_free_idx(const Hashtable *ht, const char *key_str, unsigned long *out_hash, unsigned int *out_idx) {
    if (ht->count == ht->capacity) {
        fprintf(stderr, "Hashtable is full somehow, unable to probe or add new elements.\n");
        return PROBE_ERROR;
    }
    if (strnlen(key_str, MAX_KEY_LEN) > MAX_KEY_LEN) {
        fprintf(stderr, "Provided key is too long, the MAX_KEY_LEN can be increased by macro if desired.\n");
        return PROBE_ERROR;
    }
    unsigned long hash = hash_func(key_str);
    unsigned int start_idx = hash % ht->capacity;
    unsigned int curr_idx = start_idx;
    unsigned int x = 0, probe_cnt = 0;
    Hashentry *arr = ht->arr;
    do {
        if (arr[curr_idx].state == DELETED || arr[curr_idx].state == UNUSED) {
            *out_hash = hash;
            *out_idx = curr_idx;
            return PROBE_SUCCESS;
        } else if (arr[curr_idx].state == USED && arr[curr_idx].stored_hash == hash && strncmp(arr[curr_idx].key, key_str, MAX_KEY_LEN) == 0) {
            *out_idx = curr_idx;
            return PROBE_SUCCESS;
        }
        x++;
        curr_idx = (start_idx + probe_offset(x)) % ht->capacity;
        if ((++probe_cnt) >= ht->capacity) {
            break;
        }
    } while (curr_idx != start_idx);

    *out_hash = -1;
    *out_idx = -1;
    return PROBE_NOT_FOUND;
}

bool hashtable_resize(Hashtable *ht) {
    unsigned int old_cap = ht->capacity;
    unsigned int new_cap = old_cap * 2;
    Hashentry *old_arr = ht->arr;
    Hashentry *new_arr = (Hashentry *)malloc(sizeof(Hashentry) * new_cap);
    if (!new_arr) {
        fprintf(stderr, "failed to allocate new larger internal array for hashtable during resize\n");
        return false;
    }
    ht->arr = new_arr;
    ht->count = 0;
    ht->capacity = new_cap;
    for (unsigned int i = 0; i < new_cap; i++) {
        ht->arr[i].state = UNUSED;
        ht->arr[i].stored_hash = 0;
        ht->arr[i].key = NULL;
        ht->arr[i].value = NULL;
    }
    for (unsigned int i = 0; i < old_cap; i++) {
        if (old_arr[i].state == USED) {
            if (!hashtable_put(ht, old_arr[i].key, old_arr[i].value)) {
                fprintf(stderr, "failed to resize/double hashmap at load factor\n");
                ht->arr = old_arr;
                free(new_arr);
                return false;
            }
            free(old_arr[i].key);
            free(old_arr[i].value);
            old_arr[i].key = NULL;
            old_arr[i].value = NULL;
        }
    }
    free(old_arr);
    return true;
}

bool hashtable_put(Hashtable *ht, const char *key, void *value) {
    if (!ht || !key || !value) {
        fprintf(stderr, "Hashtable_put failed, check the hashtable pointer is valid plus key/value usage\n");
        return false;
    }
    if ((ht->count + 1)/(float)ht->capacity >= TARGET_LOAD_FACTOR) {
        if (!hashtable_resize(ht)) {
            fprintf(stderr, "hashtable_put failed due to failed resize\n");
            return false;
        }
    }
    unsigned long hash;
    unsigned int free_idx;
    ProbeResult result = probe_free_idx(ht, key, &hash, &free_idx);
    if (result == PROBE_ERROR) {
        if (!hashtable_resize(ht)) {
            fprintf(stderr, "Failed to resize/expand table after probe_free_idx exhaustion.\n");
            return false;
        }
        result = probe_free_idx(ht, key, &hash, &free_idx);
        if (result != PROBE_SUCCESS) { // either another error or table somehow still too full
            fprintf(stderr, "PANIC: Even after expanding hashtable capacity, no free index is found somehow.\n");
            return false;
        }
    }

    if (ht->arr[free_idx].state == USED) {
        memcpy((char *)ht->arr[free_idx].value, (char *)value, ht->element_size);
        return true;
    }

    // reaching here means the entry is UNUSED or DELETED, either way it needs key/val allocations
    Hashentry *entry = &ht->arr[free_idx];

    size_t key_len = strnlen(key, MAX_KEY_LEN);
    if (key_len >= MAX_KEY_LEN) {
        fprintf(stderr, "Provided key is too long: MAX_KEY_LEN: %llu, provided key len: %llu\n", MAX_KEY_LEN, key_len); 
        return false;
    }
    entry->key = (char *)malloc(key_len + 1); // +1 for the null terminator right ?
    if (!entry->key) {
        fprintf(stderr, "failed to allocate memory for HashEntry key\n");
        return false;
    }
    strncpy(entry->key, key, key_len);
    entry->key[key_len] = '\0'; // explicit null termination

    entry->value = malloc(ht->element_size);
    if (!entry->value) {
        fprintf(stderr, "failed to allocate memory for HashEntry value/data \n");
        return false;
    }
    memcpy(entry->value, value, ht->element_size);

    entry->state = USED;
    entry->stored_hash = hash;
    ht->count++;
    printf("hashtable_put stats count: %d, cap: %d, load factor: %f\n", ht->count, ht->capacity, (float)ht->count/ht->capacity);
    return true;
}

ProbeResult probe_used_idx(const Hashtable *ht, const char *key, unsigned int *used_idx) {
    if (ht->count == ht->capacity) {
        fprintf(stderr, "Cannot probe for next used index in Hashtable since count equals capacity.\n");
        return PROBE_ERROR;
    }
    unsigned long hash = hash_func(key);
    unsigned int start_idx = hash % ht->capacity;
    unsigned int curr_idx = start_idx;
    unsigned int x = 0, probe_cnt = 0;
    do {
        curr_idx = start_idx + probe_offset(x);
        if (ht->arr[curr_idx].state == UNUSED || ht->arr[curr_idx].state == DELETED) {
            return PROBE_NOT_FOUND;
        } else if (ht->arr[curr_idx].state == USED && ht->arr[curr_idx].stored_hash == hash && strcmp(key, ht->arr[curr_idx].key) == 0) {
            *used_idx = curr_idx;
            return PROBE_SUCCESS;
        }
        x++;
        if ((++probe_cnt) >= ht->capacity) {
            break;
        }
    } while (curr_idx != start_idx);
    return PROBE_NOT_FOUND;
}

//TODO:
bool hashtable_contains() {
    return true;
}
bool hashtable_remove(Hashtable *ht, const char *key) {
    return true;
}

bool hashtable_get(const Hashtable *ht, const char *key, void *out_element) {
    unsigned int used_idx;
    ProbeResult result = probe_used_idx(ht, key, &used_idx);
    switch (result) {
    case PROBE_SUCCESS: 
        memcpy((char*)out_element, (char *)ht->arr[used_idx].value, ht->element_size);
        return true;
    case PROBE_NOT_FOUND:
        out_element = NULL;
        return true;
    case PROBE_ERROR: 
    default:
        fprintf(stderr, "Failed to perform get for Hashtable, probe_used_idx failed\n");
        out_element = NULL;
        return false;
    }
}


struct Hashentry *hashtble_toArray(const Hashtable *ht) {
    if (!ht) {
        fprintf(stderr, "A valid hashtable pointer is needed to grab all USED Hashentries\n");
        return NULL;
    }
    Hashentry *out_arr = (Hashentry *)malloc(ht->count * sizeof(Hashentry));
    if (!out_arr) {
        fprintf(stderr, "Failed to allocate memory for array in Hashtable_toArray\n");
        return NULL;
    }
    for (unsigned int i = 0, x = 0; i < ht->capacity; i++) {
        if (ht->arr[i].state == USED) {
            out_arr[x++] = ht->arr[i];
        }
    }
    return out_arr;
}


int main() {
    // Resize tests after hashtable_create
    // Hashtable *ht = hashtable_create(sizeof(int), 100);
    Hashtable *ht = ht_create(int, 100);

    int x = 21, y = 60, z = 72;
    hashtable_put(ht, "some_key", &x);
    hashtable_put(ht, "some_key_2", &y);
    hashtable_put(ht, "some_key_3", &z);

    Hashentry *list = hashtble_toArray(ht);
    for (int i = 0; i < ht->count; i++) {
        printf("%s -> %d \n", list[i].key, *(int *)list[i].value);
    }
    free(list);

    int out_int;
    hashtable_get(ht, "some_key", &out_int);
    printf("get key: some_key should be 21 got -> %d\n", out_int);

    hashtable_get(ht, "some_key_3", &out_int);
    printf("get key: some_key_2 should be 72 and got -> %d\n", out_int);
    
    hashtable_get(ht, "some_key_2", &out_int);
    printf("get key: some_key_2 should be 60 and got -> %d\n", out_int);


    hashtable_get(ht, "nonexistent_key", &out_int);

    hashtable_deinit(ht);
    free(ht);
} 
