#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <assert.h>

#ifndef TARGET_LOAD_FACTOR
#define TARGET_LOAD_FACTOR 0.65
#endif 

#ifdef QUAD_PROBING
static inline unsigned int probe_offset(unsigned int x) {return x * x;}
#else 
static inline unsigned int probe_offset(unsigned int x) {return x;}
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
    void *key; 
    void *value; 
    // unsigned int stored_hash; 
    uint64_t stored_hash; 
    EntryState state;
} Hashentry;

typedef struct Hashtable {
    unsigned int capacity;
    unsigned int count;
    size_t key_size;
    size_t value_size; // size of the stored value associated to a key
    Hashentry *arr; // internal array of Hashentries
} Hashtable;
//TODO: macro to check if key strings 
// initialize an empty hashtable, meant to work on a stack allocated hashtable or preallocated hashtable
bool hashtable_init(Hashtable *ht, const size_t key_size, const size_t value_size, const unsigned int base_capacity);

// de-initialize an empty hashtable, all internal memory related to Entries and their keys, values
// are freed if they were allocated
void hashtable_deinit(Hashtable *ht);

// wrapper macro around _hashtable_create() which allocates a pointer for a hashtable 
// with the desired capacity passed in, the first argument is the type and second 
// the desired capacity
#define hashtable_create(key_type, val_type , new_cap) _hashtable_create(sizeof(key_type), sizeof(val_type), new_cap);
struct Hashtable *_hashtable_create(size_t key_size, size_t element_size, unsigned int new_cap);


// if the EntryState passed is ENTRY_DELETED then the key/value of an entry will be freed since they were previously allocated
// otherwise the new entry state is ENTRY_UNUSED in which case no alloc or frees happen here since that is the job of hashtable_put
void hashtable_init_entry(Hashtable *ht, unsigned int entry_idx, EntryState state);

bool hashtable_put(Hashtable *ht, const void* key, void *value);

void *hashtable_find(const Hashtable *ht, const void *key);

void hashtable_get(const Hashtable *ht, const void *key, void *out_value);

bool hashtable_resize(Hashtable *ht, unsigned int desired_capacity);

bool hashtable_empty(const Hashtable *ht);
unsigned int hashtable_count(const Hashtable *ht);

bool hashtable_contains(const Hashtable *ht, const void *key);

void hashtable_remove(Hashtable *ht, const void *key);

void hashtable_clear(Hashtable *ht);

float hashtable_load_factor(const Hashtable *ht);

// _hashtable_destroy macro to keep the interface consistent
// frees and NULLs all contained pointers keys, vals, table pointer itself
// mirrors hashtable_create in reverse
#define hashtable_destroy(ht_ptr) _hashtable_destroy(&ht_ptr);
void _hashtable_destroy(Hashtable **ht);

ProbeResult probe_used_idx(
    const Hashtable *ht,
    const void *key,
    unsigned int *used_idx
);

/**
 * This is a helper function for hashtable_put.
 * Given a starting index, and a hash plus key for comparison the table is scanned for Hashentry with state ENTRY_UNUSED/ENTRY_DELETED.
 * UNUSED indices Entries are preferred, if the input key matches an existing entry it's index is returned so hashtable_put can replace the value.
 * Ultimately this function will return a PROBE_RESULT where KEY_FOUND means updating an existing slot/entry and NOT_FOUND means an unused but, 
 * soon to be in use slot.
 */
ProbeResult probe_free_idx(
    const Hashtable *ht,
    const void *key,
    const uint64_t key_hash,
    const unsigned int start_idx,
    unsigned int *out_idx
); 


// prints basic table stats: count, capacity, load factor // TODO: add memory usage counter here too
void hashtable_stats(Hashtable *ht, char *message);

bool is_even(int x);
unsigned int next_prime(unsigned int x);
bool is_prime(unsigned int x);

uint64_t djb2(const void *key, size_t key_size);


typedef struct HTIterator {
    unsigned int curr_idx;
    const Hashtable *ht;
} HTIterator;