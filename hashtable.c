#include "hashtable.h"

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash/xxhash.h"

bool hashtable_init(Hashtable *ht, const size_t key_size, const size_t value_size, const unsigned int base_capacity) {
    if (!ht) {
        fprintf(stderr, "Hashtable is NULL, unable to initialize.\n");
        return false;
    }
    if (key_size < 1 || value_size < 1 || base_capacity < 1) {
        fprintf(stderr, "To use hashtable_init both the value_size for the type, and capacity must be positive\n");
        return false;
    }
    ht->count = 0;
    ht->capacity = base_capacity;
    ht->key_size = key_size;
    ht->value_size = value_size; // size of the stored elements themselves in bytes not the Hashentries
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

struct Hashtable *_hashtable_create(size_t key_size, size_t value_size, unsigned int new_cap) {
    Hashtable *ht = (Hashtable *)malloc(sizeof(Hashtable));
    if (!ht) {
        fprintf(stderr, "Failed to allocate memory for ht during hashtable_create\n");
        return NULL;
    }
    if (!hashtable_init(ht, key_size, value_size, new_cap)) { 
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

uint64_t djb2(const void *key, size_t key_size) {
   uint64_t hash = 5381;
   const char *ptr = (char *)key;
   const char *end = ptr + key_size;
   for (int c = *ptr++; ptr < end; c = *ptr++) {
       hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
   }
   return hash;
}

// unsigned long hash_func(const void *key, size_t key_size) {
uint64_t hash_func(const void *key, size_t key_size) {
    return XXH64(key, key_size, 0);
    // return djb2(key, key_size);
}

bool is_prime(unsigned int x) {
    if (x <= 1) return false;
    if (x == 2) return true;
    for (unsigned int i = 3; (i * i < x); i += 2) {
        if (x % i == 0) {
            return false;
        }
    }
    return true;
}

unsigned int next_prime(unsigned int x) {
    if (x <= 2) return 2;
    if (is_even(x)) x++;
    while (!is_prime(x)) {
        x += 2;
    }
    return x;
}

inline bool is_even(int x) {
    return x % 2 == 0;
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
    Hashentry *old_arr = ht->arr;

    unsigned int new_cap = next_prime(desired_capacity);
    Hashentry *new_arr = (Hashentry *)malloc(sizeof(Hashentry) * new_cap);
    if (!new_arr) {
        fprintf(stderr, "failed to allocate new larger internal \
            array for hashtable during resize\n");
        return false;
    }
    ht->arr = new_arr;
    ht->capacity = new_cap;
    for (unsigned int i = 0; i < new_cap; i++) {
        // this is done so probing works correctly
        // it expects some ENTRY_STATE for each entry upfront 
        hashtable_init_entry(ht, i, ENTRY_UNUSED);
    }

    for (unsigned int i = 0; i < old_cap; i++) {
        Hashentry old_entry = old_arr[i];
        if (old_entry.state != ENTRY_USED) {
            continue;
        }
        unsigned int new_start_idx = old_entry.stored_hash % ht->capacity;
        unsigned int ret_idx;
        ProbeResult res = probe_free_idx(ht, old_entry.key, old_entry.stored_hash, new_start_idx, &ret_idx);
        assert(res != PROBE_ERROR);
        memcpy(&ht->arr[ret_idx], &old_entry, sizeof(Hashentry));
    }
    free(old_arr);
    return true;
}


ProbeResult probe_free_idx(
    const Hashtable *ht,
    const void *key,
    const uint64_t key_hash,
    const unsigned int start_idx,
    unsigned int *out_idx
) {
    if (ht->count == ht->capacity) {
        fprintf(stderr, "Hashtable is full, unable to add new elements.\n");
        return PROBE_ERROR;
    }

    unsigned int curr_idx = start_idx;
    unsigned int x = 0;
    int first_deleted_idx = -1;
    Hashentry *arr = ht->arr;
    bool probe_exhausted = false;

    while (!probe_exhausted) {
        Hashentry entry = ht->arr[curr_idx];
        switch (entry.state) {
        case ENTRY_UNUSED: 
            *out_idx = (unsigned int)(first_deleted_idx != -1 ? first_deleted_idx : curr_idx);
            return PROBE_KEY_NOT_FOUND;
        case ENTRY_DELETED: 
            if (first_deleted_idx == -1) {
                first_deleted_idx = curr_idx;
            }
            break;
        case ENTRY_USED:
            if (arr[curr_idx].stored_hash == key_hash && memcmp(arr[curr_idx].key, key, ht->key_size) == 0) {
                *out_idx = curr_idx;
                return PROBE_KEY_FOUND;
            }
            break;
        default:
            assert(false);
        }
        
        curr_idx = (start_idx + probe_offset(++x)) % ht->capacity;
        if (x >= ht->capacity) {
            probe_exhausted = true;
            break;
        }
    }

    if (first_deleted_idx != -1) { // only a deleted index was found, use it
        *out_idx = (unsigned int)first_deleted_idx;
        return PROBE_KEY_NOT_FOUND;
    }

    fprintf(stderr, "Hashtable is full, unable to find free index.\n");
    return PROBE_ERROR;

}
bool hashtable_put(Hashtable *ht, const void *key, void *value) {
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
    uint64_t hash = hash_func(key, ht->key_size);
    unsigned int start_idx = hash % ht->capacity;
    unsigned int free_idx;
    ProbeResult result = probe_free_idx(ht, key, hash, start_idx, &free_idx);
    if (result == PROBE_ERROR) {
        if (!hashtable_resize(ht, 2 * ht->capacity)) {
            fprintf(stderr, "Failed to resize/expand table after probe_free_idx exhaustion.\n");
            return false;
        }
        result = probe_free_idx(ht, key, hash, start_idx, &free_idx);
    }
    if (result == PROBE_KEY_FOUND) {
        memcpy((char *)ht->arr[free_idx].value, (char *)value, ht->value_size);
        return true;
    }

    // this is the PROBE_KEY_NOT_FOUND case, allocation and placement of the key/val must take place
    Hashentry *entry = &ht->arr[free_idx];
    entry->key = malloc(ht->key_size);
    if (!entry->key) {
        fprintf(stderr, "failed to allocate memory for HashEntry key\n");
        return false;
    }
    memcpy(entry->key, key, ht->key_size);

    entry->value = malloc(ht->value_size);
    if (!entry->value) {
        fprintf(stderr, "failed to allocate memory for HashEntry value/data \n");
        free(entry->key);
        return false;
    }
    memcpy(entry->value, value, ht->value_size);

    entry->state = ENTRY_USED;
    entry->stored_hash = hash;
    ht->count++;
    return true;
}

ProbeResult probe_used_idx(const Hashtable *ht, const void *key, unsigned int *used_idx) {
    if (ht->count == ht->capacity) {
        fprintf(stderr, "Cannot probe for next used index in Hashtable since count equals capacity.\n");
        return PROBE_ERROR;
    }
    // unsigned long key_hash = hash_func(key, ht->key_size);
    uint64_t key_hash = hash_func(key, ht->key_size);
    unsigned int start_idx = key_hash % ht->capacity;
    unsigned int curr_idx = start_idx;
    unsigned int x = 0;
    
    do {
        Hashentry entry = ht->arr[curr_idx];
        switch (entry.state) {
        case ENTRY_UNUSED:
            return PROBE_KEY_NOT_FOUND;

        case ENTRY_USED: {
            if (entry.stored_hash == key_hash && memcmp(key, entry.key, ht->key_size) == 0) {
                *used_idx = curr_idx;
                return PROBE_KEY_FOUND;
            }
            break;
        }

        case ENTRY_DELETED: // pass over tombstones 
            break;

        default:
            printf("unreachable ENTRY STATE in probe_used_idx");
            assert(false);
        } 

        // reaching here means a probe must take place
        if (++x >= ht->capacity) {
            return PROBE_KEY_NOT_FOUND;
        }
        curr_idx = (start_idx + probe_offset(x)) % ht->capacity;
    } while (curr_idx != start_idx);
    return PROBE_KEY_NOT_FOUND;
}

bool hashtable_contains(const Hashtable *ht, const void *key) {
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

// if the input state is ENTRY_DELETED it means it is being reinitialzied
// and a key and value in this Entry need to be freed
// otherwise this function is being called tto intialize a truly new Entry and gets ENTRY_UNUSED
// this is not called by hashtable_put since by the time put is called it should have already been initialize
// either by the init function or resize function which initializes a new table during resizing
void hashtable_init_entry(Hashtable *ht, unsigned int entry_idx, EntryState state) {

    if (state == ENTRY_DELETED) {
        free(ht->arr[entry_idx].key);
        free(ht->arr[entry_idx].value);
    }
    ht->arr[entry_idx].state = state;
    ht->arr[entry_idx].stored_hash = 0;
    ht->arr[entry_idx].key = NULL;
    ht->arr[entry_idx].value = NULL;
}

void hashtable_remove(Hashtable *ht, const void *key) {
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

void *hashtable_find(const Hashtable *ht, const void *key) {
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


void hashtable_get(const Hashtable *ht, const void *key, void *out_value) {
    unsigned int used_idx;
    if (probe_used_idx(ht, key, &used_idx) == PROBE_KEY_FOUND) {
        memcpy((char *)out_value, ht->arr[used_idx].value, ht->value_size);
    }
}


void hashtable_stats(Hashtable *ht, char *message) {
    if (!ht) {
        fprintf(stderr, "hashtable_stats , nothing to print - the table pointer is NULL\n");
        return;
    }
    printf("%s count: %d, cap: %d, load factor: %f\n", message ? message : "",
         ht->count, ht->capacity, (float)ht->count/ht->capacity);
}


const Hashentry* HTIterator_start(HTIterator *iterator, Hashtable *ht) {
    if (!iterator || !ht) {
        fprintf(stderr, "A valid Iterator pointer and hashtable pointer are needed for HTIterator_init");
        return NULL;
    }
    iterator->ht = ht;
    iterator->curr_idx = 0;
    return HTIterator_next(iterator);
}

const Hashentry* HTIterator_next(HTIterator *iterator) {
    while (iterator->curr_idx < iterator->ht->capacity) {
        if (iterator->ht->arr[iterator->curr_idx].state == ENTRY_USED) {
            Hashentry *entry = &iterator->ht->arr[iterator->curr_idx];
            iterator->curr_idx++;
            return entry;
        }
        iterator->curr_idx++;
    }
    return NULL;
}
