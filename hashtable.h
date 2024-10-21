#pragma once
#include <stdlib.h>

#ifndef TARGET_LOAD_FACTOR
#define TARGET_LOAD_FACTOR 0.65
#endif 

#ifndef MAX_KEY_LEN
#define MAX_KEY_LEN 256 // bytes
#endif

#ifdef QUAD_PROBING
static inline unsigned int probe_offset(unsigned int x) {
    return x * x;
}
#else 
static inline unsigned int probe_offset(unsigned int x) {
    return x;
}
#endif

typedef enum EntryState {
    ENTRY_UNUSED,
    ENTRY_USED,
    ENTRY_DELETED // tombstone
} EntryState;

typedef enum ProbeResult {
    PROBE_KEY_FOUND,
    PROBE_KEY_NOT_FOUND,
    PROBE_ERROR
} ProbeResult;

typedef struct Hashentry {
    char *key; 
    void *value; 
    unsigned int stored_hash; 
    EntryState state;
} Hashentry;

typedef struct Hashtable {
    unsigned int capacity;
    unsigned int count;
    size_t element_size; // size of the stored value associated to a key
    Hashentry *arr; // internal array of Hashentries
} Hashtable;


bool hashtable_init(Hashtable *ht, const size_t value_size, const unsigned int base_capacity);
void hashtable_deinit(Hashtable *ht);
struct Hashtable *hashtable_create(size_t element_size, unsigned int new_cap);

bool hashtable_put(Hashtable *ht, const char *key, void *value);
bool hashtable_get(const Hashtable *ht, const char *key, void *out_element);
void hashtable_remove(Hashtable *ht, const char *key);
bool hashtable_resize(Hashtable *ht);

struct Hashentry *hashtable_toArray(const Hashtable *ht);
ProbeResult probe_used_idx(const Hashtable *ht, const char *key, unsigned int *used_idx);
ProbeResult probe_free_idx(const Hashtable *ht, const char *key_str, unsigned long *out_hash, unsigned int *out_idx);
