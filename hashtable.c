#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "hashtable.h"


bool hashtable_init(Hashtable *ht, const size_t value_size, const unsigned int base_capacity) {
    if (!ht) {
        fprintf(stderr, "Hashtable is NULL, unable to initialize.\n");
        return false;
    }
    if (value_size < 1 || base_capacity < 1) {
        fprintf(stderr, "To use hashtable_init both the value_size for the type, and capacity must be positive\n");
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
        hashtable_init_entry(ht, i, ENTRY_UNUSED);
    }
    return true;
}

void hashtable_deinit(Hashtable *ht) {
    if (!ht || !ht->arr) {
        return;
    }
    for (unsigned int i = 0; i < ht->capacity; i++) {
        if (ht->arr[i].state == ENTRY_USED || ht->arr[i].state == ENTRY_DELETED) {
            free(ht->arr[i].key);
            free(ht->arr[i].value);
            ht->arr[i].key = NULL;
            ht->arr[i].value = NULL;
        }
    }
    free(ht->arr);
    ht->arr = NULL;
}

struct Hashtable *_hashtable_create(size_t element_size, unsigned int new_cap) {
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

void _hashtable_destroy(Hashtable **ht_ptr) {
    if (ht_ptr && *ht_ptr)  {
        Hashtable *ht = *ht_ptr;
        hashtable_deinit(ht);
        free(ht);
        *ht_ptr = NULL;
    }
}

unsigned long hash_func(const char *key_str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key_str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

ProbeResult probe_free_idx(const Hashtable *ht, const char *key_str, unsigned long *out_hash, unsigned int *out_idx) {
    if (ht->count == ht->capacity) {
        fprintf(stderr, "Hashtable is full, unable to add new elements.\n");
        return PROBE_ERROR;
    }
    if (strlen(key_str) >= MAX_KEY_LEN) {
        fprintf(stderr, "Provided key is too long.\n");
        return PROBE_ERROR;
    }

    unsigned long key_hash = hash_func(key_str);
    unsigned int start_idx = key_hash % ht->capacity;
    unsigned int curr_idx = start_idx;
    unsigned int x = 0, probe_cnt = 0;
    int first_deleted_idx = -1;
    Hashentry *arr = ht->arr;

    do {
        switch (arr[curr_idx].state) {
        case ENTRY_UNUSED: 
            *out_hash = key_hash;
            *out_idx = (unsigned int)(first_deleted_idx != -1 ? first_deleted_idx : curr_idx);
            return PROBE_KEY_NOT_FOUND;
        case ENTRY_DELETED: 
            if (first_deleted_idx == -1) {
                first_deleted_idx = curr_idx;
            }
            break;
        case ENTRY_USED:
            if (arr[curr_idx].stored_hash == key_hash 
                && strncmp(arr[curr_idx].key, key_str, MAX_KEY_LEN) == 0) {
                *out_idx = curr_idx;
                return PROBE_KEY_FOUND;
            }
            break;
        default:
            break;
        }
        
        curr_idx = (start_idx + probe_offset(++x)) % ht->capacity;
        if (++probe_cnt >= ht->capacity) {
            break;
        }
    } while (curr_idx != start_idx);

    if (first_deleted_idx != -1) { // only a deleted index was found, use it
        *out_hash = key_hash;
        *out_idx = (unsigned int)first_deleted_idx;
        return PROBE_KEY_NOT_FOUND;
    }

    fprintf(stderr, "Hashtable is full, unable to find free index.\n");
    return PROBE_ERROR;
}

bool is_prime(unsigned int x) {
    if (x <= 1) return false;
    if (x == 2) return true;
    // if (x % 2 == 0) { // next_prime checks for this case anyway 
    //     return false;
    // }
    for (unsigned int i = 3; (i * i < x); i += 2) {
        if (x % i == 0) {
            return false;
        }
    }
    return true;
}

unsigned int next_prime(unsigned int x) {
    if (x <= 2) return 2;
    x = (x % 2 == 0 ? x++ : x);
    while (!is_prime(x)) {
        x += 2;
    }
    return x;
}

bool hashtable_resize(Hashtable *ht, unsigned int desired_capacity) {
    if (desired_capacity < 2) {
        fprintf(stderr, "for hashtable_resize desired capacity must be >= 2\n");
        return false;
    }
    if (desired_capacity <= ht->capacity) {
        float desired_load_factor = (float)ht->count / desired_capacity;
        if (desired_load_factor >= TARGET_LOAD_FACTOR) {
            fprintf(stderr, "The desired capacity passed to resize is too low \
                 to contain all current elements\n");
            fprintf(stderr, "The current table will be maintained\n");
            return false;
        }
    }
    unsigned int old_cap = ht->capacity;
    unsigned int new_cap = next_prime(desired_capacity);
    Hashentry *old_arr = ht->arr;
    Hashentry *new_arr = (Hashentry *)malloc(sizeof(Hashentry) * new_cap);
    if (!new_arr) {
        fprintf(stderr, "failed to allocate new larger internal \
            array for hashtable during resize\n");
        return false;
    }
    ht->arr = new_arr;
    ht->count = 0;
    ht->capacity = new_cap;
    for (unsigned int i = 0; i < new_cap; i++) {
        ht->arr[i].state = ENTRY_UNUSED;
        ht->arr[i].stored_hash = 0;
        ht->arr[i].key = NULL;
        ht->arr[i].value = NULL;
    }
    for (unsigned int i = 0; i < old_cap; i++) {
        if (old_arr[i].state == ENTRY_USED) {
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
    if (hashtable_load_factor(ht) >= TARGET_LOAD_FACTOR) {
        if (!hashtable_resize(ht, 2 * ht->capacity)) {
            fprintf(stderr, "hashtable_put failed due to failed resize\n");
            return false;
        }
    }
    unsigned long hash;
    unsigned int free_idx;
    ProbeResult result = probe_free_idx(ht, key, &hash, &free_idx);
    if (result == PROBE_ERROR) {
        if (!hashtable_resize(ht, 2 * ht->capacity)) {
            fprintf(stderr, "Failed to resize/expand table after probe_free_idx exhaustion.\n");
            return false;
        }
        result = probe_free_idx(ht, key, &hash, &free_idx);
    }
    if (result == PROBE_KEY_FOUND) {
        memcpy((char *)ht->arr[free_idx].value, (char *)value, ht->element_size);
        return true;
    }

    // this is the PROBE_KEY_NOT_FOUND case so allocatiosn and placement of the key/val must take place
    Hashentry *entry = &ht->arr[free_idx];
    size_t key_len = strnlen(key, MAX_KEY_LEN);
    if (key_len >= MAX_KEY_LEN) {
        fprintf(stderr, "Provided key is too long: MAX_KEY_LEN: %u, provided key len: %llu\n", MAX_KEY_LEN, key_len); 
        return false;
    }
    entry->key = (char *)malloc(key_len + 1);
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

    entry->state = ENTRY_USED;
    entry->stored_hash = hash;
    ht->count++;
    return true;
}

ProbeResult probe_used_idx(const Hashtable *ht, const char *key, unsigned int *used_idx) {
    if (ht->count == ht->capacity) {
        fprintf(stderr, "Cannot probe for next used index in Hashtable since count equals capacity.\n");
        return PROBE_ERROR;
    }
    unsigned long key_hash = hash_func(key);
    unsigned int start_idx = key_hash % ht->capacity;
    unsigned int curr_idx = start_idx;
    unsigned int x = 0, probe_cnt = 0;
    do {
        if (ht->arr[curr_idx].state == ENTRY_UNUSED) {
            return PROBE_KEY_NOT_FOUND;
        } else if (ht->arr[curr_idx].state == ENTRY_USED 
                   && ht->arr[curr_idx].stored_hash == key_hash 
                   && strncmp(key, ht->arr[curr_idx].key, MAX_KEY_LEN) == 0) {
            *used_idx = curr_idx;
            return PROBE_KEY_FOUND;
        }
        // if not an open slot or an existing filled slot w/matching key
        // we hit an ENTRY_DELETED state and continue to probe past it
        curr_idx = (start_idx + probe_offset(++x)) % ht->capacity;
        if (++probe_cnt >= ht->capacity) {
            break;
        }
    } while (curr_idx != start_idx);
    return PROBE_KEY_NOT_FOUND;
}

bool hashtable_contains(const Hashtable *ht, const char *key) {
    if (hashtable_empty(ht)) {
        fprintf(stderr, "hashtable_contains called on empty hashtable\n");
        return false;
    }
    unsigned int _;
    return probe_used_idx(ht, key, &_) == PROBE_KEY_FOUND;
}

bool hashtable_empty(const Hashtable *ht) {
    return ht->count == 0;
}

unsigned int hashtable_count(const Hashtable *ht) {
    return ht->count;
}

void hashtable_init_entry(Hashtable *ht, unsigned int entry_idx, EntryState state) {
    if (ht->arr[entry_idx].state == ENTRY_USED) {
        free(ht->arr[entry_idx].key);
        free(ht->arr[entry_idx].value);
    }
    ht->arr[entry_idx].state = state;
    ht->arr[entry_idx].stored_hash = 0;
    ht->arr[entry_idx].key = NULL;
    ht->arr[entry_idx].value = NULL;
}

void hashtable_remove(Hashtable *ht, const char *key) {
    if (hashtable_empty(ht)) {
        return;
    }
    unsigned int used_idx;
    if (probe_used_idx(ht, key, &used_idx) == PROBE_KEY_FOUND) {
        hashtable_init_entry(ht, used_idx, ENTRY_DELETED);
        ht->count--;
    }
}

void hashtable_clear(Hashtable *ht) {
    for (unsigned int i = 0; i < ht->capacity; i++) {
        hashtable_init_entry(ht, i, ENTRY_UNUSED);
    }
    ht->count = 0;
}

float hashtable_load_factor(const Hashtable *ht) {
    return ((float)ht->count / ht->capacity);
}

void *hashtable_get(const Hashtable *ht, const char *key) {
    unsigned int used_idx;
    ProbeResult result = probe_used_idx(ht, key, &used_idx);
    switch (result) {
    case PROBE_KEY_FOUND:
        return ht->arr[used_idx].value;
    case PROBE_KEY_NOT_FOUND:
        return NULL;
    case PROBE_ERROR: 
    default:
        fprintf(stderr, "Failed to perform get for Hashtable, probe_used_idx failed\n");
        return NULL;
    }
}

struct Hashentry *hashtable_to_items_array(const Hashtable *ht) {
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
        if (ht->arr[i].state == ENTRY_USED) {
            out_arr[x++] = ht->arr[i];
        }
    }
    return out_arr; 
}

void hashtable_stats(Hashtable *ht, char *message) {
    if (!ht) {
        fprintf(stderr, "hashtable_stats , nothing to print - the table pointer is NULL\n");
        return;
    }
    printf("%s count: %d, cap: %d, load factor: %f\n", message ? message : "",
         ht->count, ht->capacity, (float)ht->count/ht->capacity);
}
