#pragma once
#include <stdlib.h>

#ifndef TARGET_LOAD_FACTOR
#define TARGET_LOAD_FACTOR 0.65
#endif 

#ifndef MAX_KEY_LEN
#define MAX_KEY_LEN 256 // bytes
#endif

#ifdef QUAD_PROBING
#define OFFSET(x) (x * x)
#else
#define OFFSET(x) (x)
#endif

typedef enum EntryState {
    UNUSED,
    USED,
    DELETED // tombstone
    // NOT_FOUND could have been here
} EntryState;

typedef enum ProbeResult {
    PROBE_SUCCESS,
    PROBE_NOT_FOUND,
    PROBE_ERROR // table full 
} ProbeResult;

typedef struct Hashentry {
    char *key; 
    void *value; 
    unsigned int stored_hash; 
    enum EntryState state;
} Hashentry;

typedef struct Hashtable {
    unsigned int capacity;
    unsigned int count;
    size_t element_size;
    Hashentry *arr; // internal array of Hashentries
} Hashtable;

bool hashtable_init(Hashtable *ht, const size_t value_size, const unsigned int base_capacity);
void hashtable_deinit(Hashtable *ht);
struct Hashtable *hashtable_create(size_t element_size, unsigned int new_cap);
bool hashtable_put(Hashtable *ht, const char *key, void *value);
bool hashtable_resize(Hashtable *ht);

ProbeResult probe_used_idx(const Hashtable *ht, const char *key, unsigned int *used_idx);

// returns a pointer to an array of structs for of all entries currently USED in the Hashtable
// caller is responsible for freeing the array pointer when done with it
// this is a shallow copy
struct Hashentry *hashtble_toArray(const Hashtable *ht);