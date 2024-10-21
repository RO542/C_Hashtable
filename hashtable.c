#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hashtable.h"
#include <assert.h>

bool hashtable_init(Hashtable *ht, const size_t value_size, const unsigned int base_capacity) {
    if (!ht) {
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
        ht->arr[i].state = ENTRY_UNUSED;
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

ProbeResult probe_free_idx(const Hashtable *ht, const char *key_str, unsigned long *out_hash, unsigned int *out_idx) {
    if (ht->count == ht->capacity) {
        fprintf(stderr, "Hashtable is full, unable to add new elements.\n");
        return PROBE_ERROR;
    }
    if (strlen(key_str) >= MAX_KEY_LEN) {
        fprintf(stderr, "Provided key is too long.\n");
        return PROBE_ERROR;
    }

    unsigned long hash = hash_func(key_str);
    unsigned int start_idx = hash % ht->capacity;
    unsigned int curr_idx = start_idx;
    unsigned int x = 0, probe_cnt = 0;
    int first_deleted_idx = -1;
    Hashentry *arr = ht->arr;

    do {
        EntryState state = arr[curr_idx].state;
        if (state == ENTRY_UNUSED) {
            *out_hash = hash;
            *out_idx = (first_deleted_idx != -1) ? first_deleted_idx : curr_idx;
            return PROBE_KEY_NOT_FOUND;
        } else if (state == ENTRY_DELETED) {
            if (first_deleted_idx == -1) {
                first_deleted_idx = curr_idx;
            }
        } else if (state == ENTRY_USED && arr[curr_idx].stored_hash == hash && strcmp(arr[curr_idx].key, key_str) == 0) {
            *out_idx = curr_idx;
            return PROBE_KEY_FOUND;
        }
        curr_idx = (start_idx + probe_offset(++x)) % ht->capacity;
        if (++probe_cnt >= ht->capacity) {
            break;
        }
    } while (curr_idx != start_idx);

    if (first_deleted_idx != -1) {
        *out_hash = hash;
        *out_idx = first_deleted_idx;
        return PROBE_KEY_NOT_FOUND;
    }

    fprintf(stderr, "Hashtable is full, unable to find free index.\n");
    return PROBE_ERROR;
}

bool hashtable_resize(Hashtable *ht, unsigned int desired_capacity) {
    if (desired_capacity <= ht->capacity) {
        float desired_load_factor = (float)ht->count / desired_capacity;
        if (desired_load_factor >= TARGET_LOAD_FACTOR) {
            fprintf(stderr, "The desired capacity passed to resize is too low to contain all current elements\n");
            fprintf(stderr, "The current table will be maintained\n");
            return false;
        }
    }
    unsigned int old_cap = ht->capacity;
    unsigned int new_cap = desired_capacity;
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
    if ((ht->count + 1)/(float)ht->capacity >= TARGET_LOAD_FACTOR) {
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
        fprintf(stderr, "Provided key is too long: MAX_KEY_LEN: %llu, provided key len: %llu\n", MAX_KEY_LEN, key_len); 
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
    hashtable_stats(ht, "put");
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
        if (ht->arr[curr_idx].state == ENTRY_UNUSED) {
            return PROBE_KEY_NOT_FOUND;
        } else if (ht->arr[curr_idx].state == ENTRY_USED 
                   && ht->arr[curr_idx].stored_hash == hash 
                   && strcmp(key, ht->arr[curr_idx].key) == 0) {
            *used_idx = curr_idx;
            return PROBE_KEY_FOUND;
        }
        // else the state is DELETED so we keep probing over
        curr_idx = (start_idx + probe_offset(++x)) % ht->capacity;
        if (++probe_cnt >= ht->capacity) {
            break;
        }
    } while (curr_idx != start_idx);
    return PROBE_KEY_NOT_FOUND;
}

bool hashtable_contains(const Hashtable *ht, const char *key) {
    unsigned int _;
    return probe_used_idx(ht, key, &_) == PROBE_KEY_FOUND;
}

bool hashtable_empty(const Hashtable *ht) {
    return ht->count == 0;
}


void hashtable_reinit_entry(Hashtable *ht, unsigned int entry_idx, EntryState state) {
    free(ht->arr[entry_idx].key);
    free(ht->arr[entry_idx].value);
    ht->arr[entry_idx].state = state;
    ht->arr[entry_idx].stored_hash = 0;
    ht->arr[entry_idx].key = NULL;
    ht->arr[entry_idx].value = NULL;
}

void hashtable_remove(Hashtable *ht, const char *key) {
    unsigned int used_idx;
    if (probe_used_idx(ht, key, &used_idx) == PROBE_KEY_FOUND) {
        printf("Found key to remove, input: %s -> found_key: %s\n", key, ht->arr[used_idx].key);
        hashtable_reinit_entry(ht, used_idx, ENTRY_DELETED);
        ht->count--;
    }
    hashtable_stats(ht, "Remove called");
}

void hashtable_clear(Hashtable *ht) {
    for (unsigned int i = 0; i < ht->capacity; i++) {
        hashtable_reinit_entry(ht, i, ENTRY_UNUSED);
    }
    ht->count = 0;
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
    printf("%s count: %d, cap: %d, load factor: %f\n", message,
         ht->count, ht->capacity, (float)ht->count/ht->capacity);
}

char *test_arr[1000] = {
    "k0TMfk70gwDM6E2o",
    "CZfxjdbdxUiLsb0E",
    "mr7eGFtWMhe6zrZP",
    "Pa3YowTqM6UvXjhN",
    "1Ow5sOEWBK2wDCAT",
    "PSq2iSGZmDhSqQeN",
    "4Es6f5MOUHCsChfz",
    "qoxJbdy4I1u8808V",
    "Gt6OBuxpp1VxuWO9",
    "MHjLdx08cx1CymQS",
    "b27SNvbHLdJ21svs",
    "sYPXVxDcwISUy3sy",
    "lWspJYlQu1Nxn2q1",
    "LsXWqTQeI1cPYZmF",
    "Ccg0ANsk87ykoiwf",
    "T2BWebAwKnRRmvGP",
    "smsvv9tOSLX9a4IJ",
    "104YlW0xXY2i0hwX",
    "DY6zQxK54ljpkGBW",
    "TQgJoitf4qlzXvL3",
    "E9FvtwzvqqiZ7uYm",
    "oMdR6yyWixDWqyoD",
    "zcphhMVLeeua7UUo",
    "3ISFKRhyUliEMJKV",
    "erRDhhelaJG8G7ka",
    "VvIljln2BM0Qiora",
    "KcvAq0FAot5EujJn",
    "S0sGpptU8jV02qMF",
    "qrJw2ax1hLnD81zF",
    "3dBl6eXzVEdBq0bM",
    "CQtRWsf0sGCsB17p",
    "WqUbvI3692k6p4A1",
    "in2FiPWEvUTuisug",
    "UfhueN2CBxaZoQ9A",
    "hgkMl3ye6C3Hreb9",
    "57p6UFCqF6Nu3zHv",
    "p6MZT6dkpAyRDWbh",
    "8b7th0qx1zvr3fT8",
    "6bFlMAj5Lg24D8se",
    "T9rqYdO7cWKH6ntc",
    "ZpMRUwRIpX0rUB4j",
    "C2fODO4OTZSCnhef",
    "ZQ8fo42CGuxc8P3C",
    "4Xvefg2JiHYHuDeV",
    "eITbq7qENd7kjkpq",
    "CJbQwVz5Av1GQroE",
    "AWPSlakd2IBJqZCJ",
    "AeyG4ttt3PQxDAjG",
    "GljVkvFVHuGvX2lG",
    "gbg22zxtO16u8ugn",
    "wYOJBzNq3hkKyLtN",
    "WUDExlJkp08xWrjH",
    "Y1Q7UALQhPExGJaW",
    "vuhEDcavZFaoxbbR",
    "CxgidDPN4unORcZr",
    "tqnknmtihUEC7d95",
    "3jVzLGNP9vEl0wK9",
    "jClRYmYC26N2oiTW",
    "0oWFbWVeH1D1dnBy",
    "40rfRdqJhhbLHOF9",
    "gpjzqgJbt8flEPTC",
    "zu0drHPpGNF8Qblh",
    "RrlMvYFkpt38plpa",
    "nNzBwIKEnj0xexE7",
    "5JGmXxg7bzCsBhg4",
    "FhI5sLfgWP2Ju8Xq",
    "wU7gynUMXFyhsBuH",
    "dr8YYhDbphFXceSp",
    "jH5ESOQzlAVuqVL3",
    "Ff1z7KBUpPxhRwPQ",
    "nVB9KDj9Hpo14aRo",
    "nJF2WwvYl1cBkYmm",
    "FbMJ3WA69gMlXJyZ",
    "Zor1bhV04lkipr2h",
    "I84jOox3UxwllE2d",
    "9N9hiJDxWnkJuWnv",
    "TKmJCdoUf7vCnFSH",
    "R34USz7f4kQbxLH2",
    "nJiyKyA6r6lDn4fp",
    "ClHdOtgmf9Lxp8AT",
    "69XJMeeeSY0WXMUg",
    "gjVNAK4TjtSWDrOA",
    "wHJwNzoKhHhLmAG5",
    "JGWbUv1KAoQl6trA",
    "eCQ1jqtuhtQ4kyPv",
    "Y3HRsV00Mh5JQu3I",
    "X64r3odu7T749gnk",
    "65QBMIz5WwWHVEaI",
    "ykNxtr0y1lH4u8nF",
    "FOAdYLFD6hDmJsi7",
    "XOMwuTSy6MxFehFs",
    "FvMVQpjLmVbYten3",
    "v8xkcm8ZeqHIpvYn",
    "wud68XZznEIK4yXr",
    "zsAXhzsKEKrMFtyD",
    "JsfMg1JoN6LbmcAC",
    "JoLx0OxqkjSXJgV4",
    "A7LR12UqEeJpa8Cr",
    "TeUHcADLmukLE2Cv",
    "3UUyZE25osIqlGXm",
    "gjqGGi9YE2Pq4Cab",
    "bacaO7gOee61IKq5",
    "dd1J0iIfNgMwdb5b",
    "PlMt9CG3RXt8n1Sr",
    "C71wLAgcQv6cKddp",
    "pvpVL9iAcS0r54kp",
    "whNdXXRcw7R9V4r9",
    "ANfTsgiqYbXkHep0",
    "tblvs23ZEFbwtWHH",
    "klfY0CzIsG2lAFTX",
    "snsYNBmLGtabAGqu",
    "5YPjVMEz3PdGadOK",
    "XsVm1VxKUO41phK1",
    "FPQhGEhZg41pVvV5",
    "jlbr37yFQ3uKvdpc",
    "yoIuIr9BX4dxzmpQ",
    "hQ1aMMdlTf5qeHTW",
    "mmDudZvkOT0iEMWE",
    "H8QXh6uGeFKTICxb",
    "K2fiJwBvl9HW00X7",
    "Kd68rIklwgCqLIhm",
    "Dp0L87v8noKUJFzU",
    "0L9W8guOeyn9g9Og",
    "12kxpsGOIiRClJPE",
    "UBOr2NNCGq0Q4Sda",
    "B4fFmZ8etCpK4MtM",
    "pUdi1fYJpoEOyUFO",
    "RT5HvIM8nWCR3TFD",
    "ZcTes1Xfik9PdEnO",
    "Jl9LWDiBV4uK9ogt",
    "jIVxNN6mtFu0rX47",
    "sRlHGT55GERNYvaf",
    "pQivHEL8iqmJVJ8p",
    "QbpQcRPDryKP73jE",
    "N9Q1C0I3coM8YXRd",
    "ZUPyziCjYjATmeHy",
    "N1zGPUvV2dx2xN2F",
    "vz56J7nUksgYD26u",
    "v440J8RswoHTAP4r",
    "c7xxOfNDw8geaENG",
    "p8DTUCEXpOovlyRX",
    "yGfNeilIQDuHOENF",
    "kv7VFdx6KhKoKohg",
    "aDcp6LTimqUVQgTl",
    "hj8zrSLfFUgQNbSR",
    "T58fKejcF02owNX7",
    "tiSI67YoGSOZdykY",
    "oIByNCtXgVoRjPlh",
    "QIdOYcp0X1vJKBiQ",
    "ewTswwcYluDYOPhY",
    "3aHRGuLphi027iOf",
    "dY59jtF0KPpBwmIa",
    "eiiT3B1jq5df5FEl",
    "vjVxlLZA5Y8oruaH",
    "HY7niU3vJCF8ebF6",
    "E4IuAKD3A8RWjlzG",
    "LzYqpvE1dAITWHgq",
    "3RBgGfk9kwCzIMj8",
    "PhEvpsxSGBA7IwJ6",
    "zz7oK1zXkuUoPP51",
    "q0yYwfUmJrErwJyN",
    "8tZLr3vRA4HtGcQI",
    "SyyhcEQMnKVURSMF",
    "lWlxunC0K1krGC28",
    "x50YAQp5ayCW1cVu",
    "5iHo8bVK77NxbWNs",
    "OrBnt6eMyLVY8dDX",
    "nvrr8xWiIylTW0dd",
    "ehaJPvxDACImmaeA",
    "wreMF8GLZXMuCU3G",
    "hhBSvNx0Han93aAV",
    "GD8ZDqGqjr8olXUm",
    "WagWpkeAOFJrhDdt",
    "opAmfTRWKE7OuNWU",
    "yU1se1I84JrxZuLj",
    "HZ9zaRU83uJOSD32",
    "x5MKjdqxqfhYiyI0",
    "bJkzEKv946biUdkD",
    "CmXRz8Cl9RtMyEbP",
    "TWIFdNIt9RTXa2n5",
    "7OQGdKUvyYnbw2zX",
    "SJNNmw1hFmdQrPzo",
    "mR3XtaXhYQPxXsHC",
    "ZDJLc3Vv8UWNIGvR",
    "LRbfacWFP9QXmkmj",
    "2bzsQNnQ6RYGQLjV",
    "rLpxqgPlP9zFAcAU",
    "39RcWlGKXwRCykB6",
    "YJcpKsozeQuuZ8I4",
    "MPj7d54MH8Qwtl05",
    "WNRQaYExw9Tyt6ke",
    "JO7KaCX8dJq0HpOG",
    "IMsz07ylvbN8TZT8",
    "KTmHGoo0DkTgq6Jw",
    "0ssE34mfg4Lt3j43",
    "jkr31UaQjbPIfxIu",
    "leM63BTiozIiFCTk",
    "QDAwmEVOLievftGP",
    "8EA5kvqidkvcMzhh",
    "1AZpDMhzwrxEq83d",
    "yn3pVEKAxqMWLvHm",
    "rawjjGKgvRr82g3l",
    "C8B2Wme79kJAbcIs",
    "5dx0BR36CxIyINYZ",
    "MOGRX9oBWwvW6XXT",
    "3YLQt5vQ3cVPVy45",
    "formqdthavXc0B80",
    "IJbfoz1jHsSJUZJR",
    "iMeK9iEwQaqG825f",
    "uHAw94irN1k4BtOm",
    "M5kEth2h3Asaj8e6",
    "LDGOd9H4PQkC1JTm",
    "qkI8KOyKT65939Ia",
    "TeHc9hZMTpN6bnIa",
    "1FHuDDUzNDewUglf",
    "P9LsfA5L80uXwF0I",
    "XilCfDe7BiMO9KUO",
    "0o8g1rILtMe5atv5",
    "1XAIOrUKtmvrUNzM",
    "1egNn7xqZgnUlM02",
    "uY5WQQlTxBfeW3Ou",
    "XIRbRh9ufvvywrz4",
    "4lJIpIXKKkHc4hLS",
    "ahQpwXuGz9qYGcgr",
    "e1AQ0BhSIeYacjfX",
    "yjkdO3offRsW6q6W",
    "K1Xq26KRv8sothKr",
    "zjouLLTF32OKda44",
    "svHdaLXey2gSYUYE",
    "HVyWOprnrMvn8Fj8",
    "dPEW0XAOCYYctIcj",
    "X9gchtQMHo9A5m81",
    "wtANhR4c2DiT8kup",
    "uuQfpyXUDHLLCikG",
    "xggIRHiUOWQuTepk",
    "5DQEPeXCsBONaQFR",
    "TFZh61gyi6g1AYI9",
    "TL4vSOfMVmPOfKfd",
    "wzEUUkKvV1zuujHX",
    "FkHU9rZ87a3492G9",
    "Dd3DFS3ExQh1ThBc",
    "uxqcGZaJWEmqlVWG",
    "QYDTeP82CvpZTzZ7",
    "S3X326AOGgfMGBQ8",
    "jj5GkpZWcjf30Tgg",
    "TD5J26Pj5ouPmUN2",
    "UpBAkUah9SEuMshF",
    "VtycjCZOGZDv3Bzo",
    "SBLdXmPzaDu0u9zH",
    "zoUIVznVNmdPq7nK",
    "VO5RhcFgaCF1pBDd",
    "gMfVdL9gVRco5exG",
    "jT1Rz6ETenjdq2K0",
    "nrJKFwCqfJY2F8xE",
    "xoldhNbFwm55dXRH",
    "tNZ4kXyU12wjjeNc",
    "6iYF1hUcPWkLgAH8",
    "zUQy9s2uTfcIKKyT",
    "GUmlB4k6r03aHnlu",
    "PDMWUd6sdhRuwuqw",
    "3kJvpHActJ6A83ld",
    "pyMcJOZQQ9GyqPFY",
    "mkOzkwMh4zvqQ1qo",
    "09rwrG7X6TV6zf0A",
    "euieczjyIqc3pOTM",
    "7W9a5dP4JEvpHR7k",
    "XAGQngtlyuwJ5msO",
    "7tb1zQnzlhQUAsaA",
    "Sel2SR64o2Qe47px",
    "hh9N147k7vpJ96Pd",
    "17ZcAJ3GFpAh5AZr",
    "jeqUrF4egXgSvcFF",
    "9HCx2dCWWos5kiYH",
    "APE061whTMWjQ7PH",
    "jO6sbA9UR4OTLGFJ",
    "SJlZ7Qq2VZBqQ5hR",
    "gECATQxWlKkRy5Dl",
    "9kuvHQa3S4MkwSvS",
    "iWNZ3y6qVtCnCLm9",
    "iCAJqDVy4GTNjOWQ",
    "usPZvIohxK8aSqcq",
    "5xRXcpo0xNUYEhyH",
    "JuUYXhLv2OlGqJPv",
    "MCrR5SDK7xRXwrLS",
    "gGidzrgr0oBfyyzY",
    "vELG21qV8qSR5Lbd",
    "PV4y4w3nqcXfwnet",
    "oEnKhldaoQ5dG1ka",
    "baaAI5y1Q0WkOOIg",
    "wbBJPXT2MVIelIU8",
    "0o9CKb3wKkaF8Ckh",
    "zdbnioHZzd4e1IG6",
    "2CG89HXUgHVwSyck",
    "WELAgFfXwRg1w1pg",
    "WV5uhGZs0WVsCSJB",
    "jOPEWA6GsRnCTyHD",
    "1Htn6hE7RFFg978p",
    "tFPnQRccq0qVtYBZ",
    "fDHWr2sR2kn38YS6",
    "z0qWL4e6c3wjPETj",
    "H3IjiC37pyroGQCm",
    "Be6vMVVEI1WcFdet",
    "cB3fSnEp0rj152N0",
    "XRVRbjGUvZcyLpqL",
    "8LCs7X5TCZpzjlga",
    "bUoHLxPhIuzUoIQC",
    "Da3Frg85BRermfJP",
    "q5KmvoOpmVDbXH41",
    "VQ48LiNZKRW2TblV",
    "RP4yLLt5v4jMRXd9",
    "DGVR284DeDMQFYDG",
    "bgTWdbeuVrXhLzh4",
    "rHKj2B6cWdN8RBTD",
    "dsJ52ndPIVRaov3F",
    "WlEfODfJoVQeRvhQ",
    "EFRjVUqa8E4LBSvd",
    "Ojj5437ERJ3yrRE1",
    "ULvnxlETUVlHZjih",
    "LfaWX3UWci5POBe7",
    "wzOhHd61ImYIWl40",
    "TY7N2mfWK57IOX3z",
    "q8g5hx6JMKY8evZi",
    "k27ZMAHoRI98gH7p",
    "nYDRsROtWknAG0Ua",
    "Jc5sTtorarHHy58j",
    "YWliKCfxtMaS22XY",
    "3XNDFSUR7kHQtlIb",
    "WqQClbk8uHJcY4A7",
    "u4J1owZBtFjhb2I6",
    "jBjLkNDnK5wevCTk",
    "seBOLZjlDOfbmSsE",
    "butvhjGmjuR999AH",
    "KNwsuZKuejrRIfEj",
    "d6X7LfYSe1hr0I0U",
    "9tsloa7Vkg766dl2",
    "eCrzsdE3QyJStfYG",
    "x80kNK8i2VAexAyB",
    "zpzlbI728Iivxttz",
    "l1MQ4AAmrdoz1fo8",
    "2tLooVwOkhaGluPD",
    "57pBRY4SQKg8LOK7",
    "Qaf6dXgOtIvc0EzD",
    "KVVbg4INeo4Ke4Yh",
    "dOwL7apc5SnLeBNu",
    "vl2QIbRfQpvTR4Si",
    "Ij6wSDP9F3xa6MC1",
    "zOcSmsDFiOZ5Slyf",
    "93vTW6SC5Iv9Kgl8",
    "uHbhCVl30Ybj4hnh",
    "9qASCHvgW1TNjOuC",
    "9grLTRxdF0OP06MV",
    "qltfutoTUKszFQw4",
    "er9TZjjYHwvChBRK",
    "TYfPCZIMBzURkUsm",
    "4sj5hgjdIqu1hLl5",
    "izNJZqcUDpitijr6",
    "MEJgFB84YvbwCkCo",
    "fO23nxcJmvxN3cHT",
    "ehAytXXq0DvlI4dL",
    "HG6VgaHVlAjdEFMr",
    "VDsBy0lppSt1Nd2B",
    "9nSD1Uqm7sEoVbUX",
    "FajP9ZtiKtzKKvFB",
    "8Dx8sfV8S0bXRBye",
    "9aFrocdZz4AjmZfU",
    "0jq4RjbzMMqMKQ0s",
    "2wUvO7aTWcBuv93L",
    "IfUouMji4tzv17mc",
    "LdPzwhJnsbtE1RWl",
    "892yr2UTjTNDxYuy",
    "z8ZUoBNurviBNBNy",
    "OjxrdveIrKKUwZ4S",
    "2dXRdtydPLsKKHZc",
    "KJWu9kITAY90a9LA",
    "qATU6bwKW5FBI3i3",
    "Ae2AApZUwKkVCWjC",
    "CgrsUI9FpOW3TNj4",
    "ugwqA6oRV8Fj88io",
    "88raZ7LLN03vrZMZ",
    "5fRciQ6UG5UXqdjw",
    "rK5s2ta2eg0Palc7",
    "KnphjsjbGWjSuORl",
    "FDLfISwGXFkCyEMA",
    "O91g2NlBuj0F2EcT",
    "NnXbLoGMfnK0lpNk",
    "7U4S0aXw70mA37I3",
    "sCS72zj5x9YjceA4",
    "oa2J2S0N5Zezz22x",
    "Pl7Md56wHGWazgqf",
    "6iSFi9IfcsBNgzQi",
    "Y2rf5SM5uaR85NRa",
    "5j30T8kd2CdnOfJr",
    "Vlx1DqdTBvIfIgTA",
    "qzJLlIJxZV8arqLl",
    "MkBdJVaoE9ZTuQKV",
    "VunnOAtV5cFX50Zg",
    "30LH5cUjTYAmPJL5",
    "BgkRFZ4hkBpIgiSg",
    "VTg1uNQZl8iIDGGt",
    "MYEYcPQuMGoB6rim",
    "DAK963HBiNhlIu2E",
    "YZR3EyYws2DWXJOm",
    "WKgdcCz6TYO7agSS",
    "RglKjZoHfZlw1hlb",
    "SkxXNz96pX2KbxUJ",
    "6VuN3i7cEOyhI28N",
    "B7fKdRkbAZpCYspP",
    "VzqV9KyNOmtElOWt",
    "p16mnSIgBm9uVPBP",
    "fvs4CwWueqAEYdoY",
    "IrPAxuzcup4VtxbC",
    "DKsm7SDGUtpCpJ2Z",
    "SAhYvPqIw8mv8f5O",
    "wtueBEPZC9AME6v6",
    "pVT63TPjtGPkJwwq",
    "8CzT23u6qzgshpaR",
    "oL26MheAcp3ualOr",
    "tfFE9SrDzVspncUz",
    "tPmRUV7LLksODTi1",
    "dDqYMt4jMhVV5zxW",
    "T3wB39U3229E3wPL",
    "w6Eu0MlGWgSqqvFR",
    "EJjjbcHaMkUeQxPn",
    "sGkTGkjKV0cjvWm3",
    "9h5wy8yqCVXMS0WQ",
    "fWhrKq72d0WJzgQB",
    "MqBveWaJHCCWNqjS",
    "5fawneSPlCEkhwI7",
    "LQUd8JMubjJmjOpR",
    "egDhhwwlamo9cqRJ",
    "WIcnVr37fksUdjuD",
    "7PcSbzYTUs6ZB9wP",
    "lZ8YEPoa5aulVL3H",
    "1M43hRDEOOweqBnL",
    "C8NEJWA2M62sYI6l",
    "8w4avLCvDDUQ8TNa",
    "M5LSDZ6H93INexzn",
    "ZDyU8m9Ay9Vw5AgT",
    "VKxMWG0dNtIFcPAK",
    "227AHcGuaLBRSUvQ",
    "yhxjNe12XqZcjMN1",
    "hu1zUBn23byvC34m",
    "ujlYasfyEUfkwk3m",
    "YwdLDAKEQBNV05ml",
    "P9G8I6z4ktSqHmAK",
    "OcEeLm0FgA0LHGKl",
    "yNQksrahMGxJKAA5",
    "NC6a3wCqQHLEpcwi",
    "HvwIMY7oIKkW1QdW",
    "OeGAbOKXMVH2CMqj",
    "t8R4On5nDFTdHr6T",
    "epW6y8sFkmNF72qX",
    "qJWqYtDkXDsNAyzz",
    "CKBS6jNFwezwnCCb",
    "uzh4zO10fSbQhgdh",
    "dOvRlSywkaFP9k2s",
    "KWp9mxjMcxkxeq11",
    "OuNG3sBlDvtD9LX3",
    "ED2ZZcyRCLE8EnAh",
    "TH8SxpRXWRZ5RuCj",
    "8eVPpQMy2ED3RZn4",
    "tYv4X24uNLFebKdL",
    "ZQXiNkdexzSyQINt",
    "lnLi9icUHcMT4nRm",
    "GQHU5ZBAAisPJc6D",
    "gVfkk4Sqp9mQkWYG",
    "M7qTlrP1G91HDJzM",
    "IMmYohnEGjBseJuw",
    "RZ2wv6RphHmV9B7Q",
    "Q0vTxto2E6VZ0hSw",
    "YukJnD1LHwyalY2q",
    "Jp9pGTPCNpG1gaA6",
    "cUeGF2gRranI7V6L",
    "FduKPKxVF7zkLcpT",
    "SvXTRvDnU04OdsXL",
    "c6HdwYmkyIWlVBaJ",
    "xWTrTWmQNUYLHyWp",
    "vUBSwP8yTX88gfPF",
    "m0pW5PP08wVMDKxU",
    "3cKKMrnkNrfviX4F",
    "wzpJPkk2PIgoQSi4",
    "PYKj0GiAR7OC5eWF",
    "a6vLQlDUaRGdcHbi",
    "g78nDGlnDDa0DqsX",
    "8Ppi99WfzlGK4vRR",
    "F82xrqIl4wf0z2ZO",
    "qOAsaLavMeRxd393",
    "9pJT8UrVi8WYIZRJ",
    "heIeG4UvcMV1SxcK",
    "MlFel2oSEfPjwEys",
    "xmLMMZKGv6PZ47qa",
    "ydFsdGwboqfLciIp",
    "NMIeaKKKELjBxahB",
    "legjXsuct3eHQMSX",
    "KAq6YL9PTovjWAzX",
    "3YU8OmCg6Vb9nbGt",
    "Xo5B1dkY7HUUHFC9",
    "VmTcIz9JZ2D4LXge",
    "sP1V8kpcfao5wJyF",
    "4A2N78zqflMgtr4s",
    "x68sKqfYyX8yDOyI",
    "0j0h1dZqCv5cMjli",
    "lSbte0buQONt0p8T",
    "mHvFrN5qvtsCizid",
    "8pgRF0KvZHKYUGC7",
    "VLVqm7KdF4Cq97tO",
    "LS8Pn4rD6Kck9YdC",
    "nfjj9YYokApDwiC8",
    "eHvETYV7pHjDsV5w",
    "IT81iWd5Cf8cqB7B",
    "tofHDlbH6NR1aeO9",
    "s3FxU0Ci9tCLHeBY",
    "vGbAhj01yK4XWrM8",
    "3VQe67atVK2Nu3Tu",
    "t3tb0XLX75MB9nQS",
    "GCziGE0X83lmFMeP",
    "USmVTk4gkA5J7C8d",
    "5O8Q27SgwR8s6OlE",
    "2mq8f3u5jiBB1hUD",
    "I0mzEGetuK9qpwMa",
    "hWMaWwk9lOKcUosL",
    "tgB9CjlLWdaMeby1",
    "rdJ59qGoBZjwnogV",
    "d4nF8182NNmCzzuS",
    "p6a8U6tCIZ8hTZ2H",
    "5YMHYZ6ZpI1mhbiA",
    "SvZwZGR45DfKIHE0",
    "pYfNqwaNdDYd9fZ1",
    "01zYA48Tfz447OvE",
    "hPJ0ZNYfQFOyjOG2",
    "avsENBbprZe1UMSP",
    "2CU1optflzmFnigw",
    "6qfE3AXDyyHVvDpH",
    "H6lSq9ceJYCnOr3p",
    "7LjRosQnD501AFE3",
    "iqwWCEJjsLGODdWP",
    "NJvLZkYxWscWmPFP",
    "czJvhRjuN8XKDfFB",
    "NGnrgJT48CvBxkem",
    "TLABVzPFDyc561KQ",
    "ytPT9uGT7cC9lM26",
    "bG9L1IKrFKb5y4Eu",
    "SgXIRpim9bLxzqz5",
    "ZXuWKiiBAAA7YBRP",
    "zcasp3eF0n3eLvaP",
    "sTOvykpELirMGCtO",
    "edFpjDrWAtRwoPmP",
    "mBMhcXV72U2C9hjh",
    "nOW9Fvb1IY3sZbcq",
    "MGwR67af2LcpwGr0",
    "wmImbChnEuOcHGd4",
    "35D9tPgwJksvoV7Y",
    "BQRK8JHjNfpFMrCm",
    "eY4RZ2b73Szf6edT",
    "ZPXwsxp3k7s5KtJV",
    "IRnXrdXFgyik5Io9",
    "zPBcpX17RHBhst8Z",
    "PKiZRUMN9kLsIkTx",
    "5Py1os0JbSxM4byl",
    "gVF6QXVRN6foKgfJ",
    "AJYERojzM3lwoqta",
    "PkZDD9zilzoU0bPQ",
    "ucm9Vohhbk9Xd00D",
    "trncbV2ioD8c0Zjy",
    "GDa2irGWLxcEFqkE",
    "D5A7KQAVtvRSO6bg",
    "X8vzxB38EAFSpDZn",
    "w6yI5LfhvGlFq73K",
    "9p72S69KPV4e23Vd",
    "ADsvCZ7iMkoI8VCH",
    "VC935VUmZ7Ftb1gT",
    "iQ5P31OtPanYTPpv",
    "OykcifSv96jZr14u",
    "qlA2Qpn4yHLeEdLd",
    "g9y5KKpSibfCeeM5",
    "mAhOIAuKvgjTZBKt",
    "c0Gw7cBrwACyAmKo",
    "e4aczdGQEF4PdN5X",
    "7beIFuKrhBv5aUoB",
    "Fbb4apIZ3yQnNGOk",
    "8fCsw4y1rU2FdiF0",
    "XNFr2aJzu51xgnc9",
    "R3bUfxA8gYGdGuPf",
    "yMr3PJyv56Llg3GH",
    "CbaWjwKXuPImcBbn",
    "gC5T9nxwI52DXCTv",
    "ikVI6TvuhgLJ0fWb",
    "Aj61XNDaugrFgEDM",
    "Puk0yXO8DXoy3owD",
    "nsxfDLdPdy8mis7r",
    "fGdE9tpWgA5XkjIl",
    "NIZt6Qlq2u16QTmS",
    "fwq1giuUlZUYzvPL",
    "asYmrmfAuWXbdX4A",
    "dDD7bcCPvK3RfMTQ",
    "PlZohctWuExSQ2mj",
    "XlGOKUHXOVA8A2mB",
    "pvZuBjfHi13O0Yuh",
    "GD7rTJpMqc8EUa3e",
    "lpVbERKSDiYkUd9k",
    "pni1ofxVjeYdHpqa",
    "upNRlqcczOfI1ifP",
    "mxF7QDDCL89A9qrU",
    "HDNEm7qU13rBPVYZ",
    "1XRyRTSxoGGeiV73",
    "VhjljZAFwzU32kAb",
    "tkIsQBJtTaIhMUoW",
    "Vf4I50c8bZRZX0mQ",
    "9737tNSQuQliqg4s",
    "QfeGO67eJQQHPNOx",
    "pO2PDil0s7XXHLJv",
    "0quhgwvTKDpmBAXg",
    "kdEbH3ToCy3zewq9",
    "E4SBtA5FJtd0U76k",
    "04wWBfZBQug6JpjU",
    "z6N0FWqqsA1DWqSx",
    "xFRJs1c5lc39NjuK",
    "B5nVda06mA4qgHLw",
    "QNK9VLIBXO4cRvyp",
    "wVeaFNpq5MbWgj0b",
    "1thxuj5MNn5gbWJ2",
    "r4BaBLP0udpbCCCM",
    "dJxESCIPNXCFKlxV",
    "6PFoG3a4bAjvL3aa",
    "iZ7rdp3c1g1JufjE",
    "4QqK257x1OjNVb0E",
    "4RhsBwuQGGxr63i0",
    "i9cLMk9aHzLoehdU",
    "vcvCCzs9zgmLiS1M",
    "8lGAwjvjxBJfRh4M",
    "Poa0cdOLZk5PDYAI",
    "oNzisa9NohbQllwq",
    "ShH8Msp9yTs8YJVO",
    "rm7YEzJNMS5XQW0q",
    "bqHI1B6gBVYqVW5f",
    "2sWzWswPzhM1zLjw",
    "tb18oQ5v3q8bwFfz",
    "u8lNbOq9dGFHFYj6",
    "aOPffnMtXOBiHBTD",
    "EQsQlTVZBiLnBAnR",
    "C4K9Za8QJuKNo4xa",
    "rdxiNrf3PchaXWYy",
    "i3lKyPRCG5BOLDdO",
    "gKD2K4QOJv6cJU2O",
    "EAf5jkStXItv5Cyy",
    "LWulyjr0Q2l2k7Od",
    "zsk9Strm0TnTI8sf",
    "yXQzc7SBGd2JTRfm",
    "2tJtf8YdOCfZ3cFM",
    "5QVvOv93b9VFw3Kx",
    "GmQxugUUyr6hbTu9",
    "fgOT0J7Ue0HvQrqV",
    "vxWphw0VAnoPFOif",
    "CB9NGygw6sNIspFk",
    "0Mn4ZvoONI5S5IL0",
    "d2NgR8UvviRkWZnM",
    "ETtkqhwwx0QjLhwz",
    "NfGp5qTrqTb8QzBT",
    "f4306UyhyvcnDZmD",
    "NeBulmVTD8t0lXhJ",
    "slw7E96xit6XMwQ1",
    "4G7nB3vKoSpuImvS",
    "Rdu13JCvZ0kNIUge",
    "2T8CUA7c7avlBCST",
    "UdZodvkkApaxLuaR",
    "aXJ1Zl8Yzle70tTN",
    "oCD9F0wUSe1kSY30",
    "UAFDt21jiTC0Ft8J",
    "BIyDRYaxrxuOrRAH",
    "V0M55Ql1vuRinLEx",
    "du0k1RNrtRtJOQCF",
    "Uztz0ocYBjH3u94M",
    "kzIm8ftP6gOL5qWP",
    "oszyQllksB1HZp0a",
    "8NMoY9HhauGDIRid",
    "zyVLY4blJsfnhL6Q",
    "V9Vqo8UfhtPgrpaJ",
    "KEZtXdgVRtDlD0xy",
    "Y9Ka4YLBRlIFoBf5",
    "0bLsRdeB0JQs4nUR",
    "C08LOuCA8bDRfLrq",
    "7APAbBSEIFXPamBf",
    "pVHi1dFoVRvxhilf",
    "mqtng6NPlCqfAIXQ",
    "gfcQsLXzrwcMegfm",
    "5vbe2MtRr2VRbvm5",
    "IpYkPTNm54kd55hX",
    "Zn6acAtHKBQQMLtE",
    "uaLaVuptc8iDlTSL",
    "IlMgbsZZN7EoSj9Q",
    "c4H8hbLlxlkwaHnP",
    "ukSMJc7DM1xCyEQY",
    "w6pqUpVovzRE2lze",
    "y7KM15RxWdXLKakh",
    "Vysv15daR6Y7kE5N",
    "IKF5lPJ4sWEQefYt",
    "EX4odxskj6hLmt6A",
    "XZX0bpbUJIMGBpWM",
    "nijAmx0rHdGrvrnX",
    "2Jh3YbHYICfDSgHM",
    "lb7wvn5vD0Wl0GWY",
    "57qHzhreDkE24K0t",
    "cW8KeU6TuDxnMZpU",
    "vjJTXNzAo6TfUu6w",
    "ruzJG7LZYzGv1tAp",
    "uVqIoQGfnFgd9rMl",
    "DQEN352F06iRAqQd",
    "w8B1qeNnYNZPEcd9",
    "QEr7qIS5ktnup7fc",
    "ygXkZPd7NIqqOp8D",
    "NaW1sLISONcvAF8h",
    "hXY8MBrsJ4IS0yfd",
    "y2cMSC7Ypf546hpy",
    "xUIMbdgh0XddeCK1",
    "xi7VWgHgQwbiQ4Mq",
    "oLG8KRfFCSMN4l6e",
    "NRnN9FG5671TpIBq",
    "d8737BbmgJsFZN6T",
    "DUlSIYpZVqIruyN1",
    "ZlIiheyk6wzo68Z2",
    "uWOvSP8EVXOOcXxz",
    "ooeDGjU88zSUQnl6",
    "EQlVyMhyrYt0H56t",
    "DF9sKLolX8c62k1Z",
    "Ukk76tJIyPewZSel",
    "4TwnMren5LRQKb8f",
    "2Dwrs8Fh1GaANOUD",
    "sfuGozTR71OioRTu",
    "335e5vXrA0tJ4Bhj",
    "0cKZjW5dQ9BNXlJL",
    "KbSyVPqysaSpMnCP",
    "DBL4CRda1P8tYjNz",
    "hsQPPJhnnoVL3GYV",
    "uCdvP848BYXKBDPZ",
    "kSmwl8R0WDaZonjQ",
    "2RqwVCclIMJjvygU",
    "0gWcJ8yroeD01zNE",
    "HFYNKPI44UNb3YTv",
    "kQbApNHjlSKrD2hZ",
    "SeSi22okHuQKyzG6",
    "4lHdezZUZHOFaD2h",
    "wAY23abxjMM1OfT9",
    "bSPErMpc5zR2HR1E",
    "G6lTkAKVhgp12awC",
    "RjXTzk32Z4fwQwOL",
    "ZJLHerMBbmQm61cS",
    "D5PmHp2zGtsFWafx",
    "Hn7Dj4O11ChFsYVk",
    "0xVplkBKbHGKngFt",
    "R39QJz3GdRXZoHhi",
    "Ua5CNJSp0nE5gjiI",
    "Q9GLEQjsAfZYI6JO",
    "WqCwB9wB15Y6RP36",
    "xP14LKqwrlgWsV4P",
    "VgL8EOzwUTSGh5dG",
    "5lNZ9EtvlW7PnPv1",
    "eLUt6nBR61kTXrCA",
    "q7ZHJTrWdIo7y8fR",
    "aBOliFeeNsDUgJcS",
    "4Cs2ELJ36bKUTjIk",
    "pRdIyEY60le9h4dO",
    "pxayNuQFqEnY98Lt",
    "LewBaoI2faqvFn9m",
    "q2rvJuSGLz9E71F8",
    "bkXirwdoKFUWJXBp",
    "bTD0HXnjuJ3zlu4B",
    "sQrFKvJH1oiThEBC",
    "pMmi6p4duA3tj0Wo",
    "weOwJr0vlH2Gfbq9",
    "HKoFXM816ALiAX0H",
    "U6nxUeVOHvFvRjqg",
    "40cFsFMVI9Pm6XbK",
    "ID4RHbh8pajMOYLK",
    "MBeEgh8sAJcgyobU",
    "oh6BhUIpg69OYtHk",
    "7G0Iho2S4jE5xe6N",
    "w0OOvxqDDq3LxmLF",
    "zem85Iq9j7WRpSgo",
    "WDjlOyNPxJyWuLhZ",
    "KW5eY2qryW6OMtAi",
    "9HA28GvO8jzpaNrx",
    "sSt79mtAIXpG07Ye",
    "OhDcMXYxbJS6o6lt",
    "tI4PLuzF3CZVUkJH",
    "uD2jPnhbxqBQVvIP",
    "6svyCi4G4n9j3Xcq",
    "lSKqqsntjQvaTKGu",
    "Ulzlu4ri97bJsGwS",
    "8j1ejFZ3RWqSiPx5",
    "aYSSCBsCTgmA8C9c",
    "ULJywEDY9lGUJbyI",
    "KEyzuoUoZJil8P4P",
    "Z9S4hakKibwfTy7v",
    "TxLMNxX9akArBIVw",
    "MQv8JNdzQ4bxVQE6",
    "7AuwaowjXrZnjPlW",
    "BKAA8Ln3DJg57HCg",
    "yEoTfcaISSaxRPE9",
    "9MlpedN3H3ChGQAI",
    "oqDvW0mR8ZD5JZwD",
    "xmeHlnBtSRS6u49g",
    "mSaq5BCJSLWuTtdU",
    "Inzw4uoKo3VvpOwN",
    "glcUIgJJvgGSNRFQ",
    "pX8UankHggXWP8hX",
    "LnyyydDbblMyAGpe",
    "5OZr6CpCfddUM5CC",
    "VX4acSHMVW7cWA3J",
    "zhfuxmcsVIfqxuMT",
    "cxqfpOs5ewRP04uA",
    "VdmkQzebdO6dvRsM",
    "3LBWDohOZw2so5ap",
    "nLU1Di7kKop0iEvL",
    "Oq5cmTRqbTTq0GtK",
    "Y6gXQMxGWmoiFSXP",
    "058M5CNJRuEsUGL1",
    "YXgghElEEXOOKl04",
    "BTel3VJdZDwIseA0",
    "4DdBJcjL5Doaw9QN",
    "kMITuh2YPCSQ5jk6",
    "2yYujfEDPpf8stj0",
    "1OhmdvTqeESFGozQ",
    "UWEZTiqrB5UZznQu",
    "6CsW1UouZJ3jKeyz",
    "zga748wOZ9qSCPCn",
    "QRKrZv9oDlDj7Dp9",
    "ydZFfUh6b7BOhuHu",
    "5QjPFkA3uCVr4srh",
    "E6jasbosXSTGd3Ab",
    "lCKixgOTLkGjEACm",
    "XfXIRT5kZr5SXifA",
    "rnUCH3MBgy2IfvxM",
    "JQbeE2SzvomUlQul",
    "tFzxVr1lCGcxXhDb",
    "DrLWUhiMzndMPjZX",
    "UZaca30Uqtq1BevU",
    "vxo1RWSUlDjnTEpw",
    "dP6PoSpQLvD5rJ7a",
    "Oedrtl2lE1Euq97n",
    "VZp036k5QI8dvlp9",
    "PxGilSlgZVwQrmCe",
    "WOEWNbqV8FsWvt07",
    "ABTIAo0KWEQoR318",
    "Jm51YeOHiUwnWpMQ",
    "7wbRuGLqxd34Jy1z",
    "hzkSyeJ4uZEfpPfZ",
    "u9hYrFW5MhSvFmg8",
    "2u1dhwKakhPQQ7NB",
    "WeOyevzW0z9qMHpB",
    "caqgFB4ILhvj1STb",
    "eiJIDGMSnvsWBNmS",
    "d5sYRZL2K66WgrCb",
    "kRpmaCIvH9jhxITL",
    "IVE4PNZkBbwgZk9C",
    "RMaJeYBuxIYdO0kU",
    "GgxI68qKy0CSGSqk",
    "usxFEZCb9k2HvAAH",
    "bNGg0vLOiybb9ENs",
    "d30nkUu2ljDSelKR",
    "S0owyUXTkD2e2G49",
    "MBg8hVHhiyjT6B08",
    "zDJ24jQRUwGfIV17",
    "aU8kInAgGKBWd1OM",
    "f33nvnWAxMIsCeiE",
    "IFkoTgnysv0OR0Tq",
    "oKc9X2HUEjOh7fxS",
    "GBBbJGYLV1hyBibp",
    "QXjs2mXgzUBV9Ygn",
    "zrjPqQuf0nZ9m4NA",
    "1kmxC0I8LgYZrUzW",
    "AKJrTUozeTXnzzCS",
    "N0q52rby9AwWvxzQ",
    "BYSIWD0gGHFV8R0t",
    "6ssvwW1RjzIO4P1J",
    "AmoP2QL9tCJkCSAR",
    "YEpoAN6as6nij6gW",
    "shRuKJwDWG4bDDzD",
    "2Y1NHEXbLDE7c4Ts",
    "Bzchlw0tJhGdR6Ki",
    "xz5ntE1hvc4qet2s",
    "7gu6cAMyJycA8POc",
    "S4BVuQmQKwC8SCf0",
    "B7EUbYxxJLVze4CG",
    "UBlBg0y3Yp07p4WT",
    "u7dDaZkX2vvYJAaM",
    "Fsq0dFOrxoWvnPTE",
    "1HBtjepdUsk12GZG",
    "fOwy1MrvQwrFovm3",
    "6RUvgunW2hduhKFi",
    "IdIRJxjCM8ZJ94Xi",
    "9V3eFwMsM3fLEq2w",
    "myTeEV5JWVc2frID",
    "CV4AQbSWMf7Ei59r",
    "rEJa0TVsjoZbHOOw",
    "JmA26bN2izQ3U7iJ",
    "dE8C5XYkpdlV6kfB",
    "4LzvXhb7CAEKGFyJ",
    "9On454TYqQVJuEMT",
    "culW0TOaoU5DeQNc",
    "zb6CNBhsXXD3zgYG",
    "xJRsJsgGywRkfRyR",
    "YI9ZAYOb2IV2v9eo",
    "o9pdX2VqbsgFo6LY",
    "LAJoxmiun57HhTY6",
    "nhm2bnrhydHvC8AV",
    "eOHFwGyWEivW8tvF",
    "PQkH1toRyTIdpBvO",
    "cDRzHSzn6ZmOjH35",
    "wDJ8y0RLyTEPYpuC",
    "9Tyyq3P3VDyPdT1B",
    "UAriyWXRTo7Iz0fh",
    "khPSqI073aOmGIro",
    "lCkRqCMr9mE1jtCJ",
    "hM4e3FS7yvEL7UE8",
    "QMmo596DAGSWlz7E",
    "yirNC5TXfkScyect",
    "reWA6E2sQklBwU1h",
    "dtdoiRuX3QUX5xM6",
    "eCuRaIqTdiqqgCtl",
    "F80GoHgocVhXa7Tw",
    "sE0eODpADFZqPtl3",
    "VxS20UHECBWzbJfc",
    "tv6KWxAa3hbgdcWs",
    "l0T0HVaJuRWkujeQ",
    "wH3yYsuY9o1qomY6",
    "w2Bc5YAP9XqoIZ0R",
    "eoxydDxA4GRoLOIL",
    "uhvP53mPkz5NOogc",
    "zR3a70zFSzLkF10N",
    "HZETuwp6WNN7qAXK",
    "g1BPtruV3F3Uus5e",
    "lSE2CtgE51XM8fWw",
    "2sPHQ2X256qYAQi2",
    "RN9xy0FUOiIn23Ij",
    "FSeE8VtYvIaAygXk",
    "5GzOI8zOSdJ6i0XL",
    "uxtfBfYd3kepSMJT",
    "OQhtVmWAe2BN8Wew",
    "7EpEhcTWjz1g1UhH",
    "6RIbO1CbNJTvAuEm",
    "n8ce7bzcOhlwry1q",
    "kxjHaHpTB80tMFSe",
    "yQCwGrZmlsnXFDiM",
    "ov4dqzAwI0enu0Lr",
    "HrcvzS7rqJ6b1iGK",
    "kafMjkPOfGF2K9zG",
    "QGj9Mp71x9tj0j9o",
    "mPiNRULDXAHiFj30",
    "esN9togA4cXmxN01",
    "cGHbBx6dZPa90nok",
    "66evF17Fgw2wEYn8",
    "8zXNMAfIyxPd10Pl",
    "m8YOoCmdDShEtkGC",
    "BRFSmIPyE6OpKPmz",
    "kVTz9QH12HX3whli",
    "94MGuqtPbV6kQowb",
    "gTzRGWpP1KY5klVL",
    "zuYLhCe0cnABWYPM",
    "2PthEGoBkC7hsJuF",
    "xWvdWXDOBX5kZWBm",
    "fHOS3badHZTky5mj",
    "U48JP3IXtyo2q5tQ",
    "zVAF0iX19IXW6boF",
    "ncPZaS7sSQv0IDRF",
    "vpSzMvB1AWx9jNkJ",
    "aAEY79sI2aklr5ZE",
    "cFNwZUs4tstGNdmo",
    "OgEFumtTY77CzJQm",
    "1xCS63UtXNx8C4MA",
    "grrDclKU7BrcZLF3",
    "040BQXfPkWKfdZC8",
    "UAKFvm2SCOpnYrak",
    "9EZgSB9S9Fw0MoD7",
    "X55PiIjxIm1WOJGr",
    "ybGWXgHsvYx5Ykit",
    "A9h3HC2ipJHhGbgI",
    "NoekegVTZLt4kms3",
    "j4q8vZfsqzUsq7Hh",
    "SU2SZtI7NSKL8868",
    "2J4MiOrGfIKqjU1L",
    "nzMmiKJDh3coMQt7",
    "Qn7uJggvlmyux101",
    "P3XfVNgVISPwa06D",
    "2MY4awb4fsiZQ9j7",
    "Lrk1EXtnYFqVw8N9",
    "M7sn9rf7xqXysBuR",
    "xRveiyEDOU2QMcKa",
    "mxs0myd9RARpQJqA",
    "wPhpddNbFYORbH52",
    "FD2R9z7WQ3wfwNtu",
    "utXoifuZcQMqq4Sh",
    "hVKwrkue6bdm51WA",
    "B4SQFUPgKUgEFHhf",
    "VIl3UA87VF3X0pft",
    "PhEBXRdx4v3UTpoF",
    "KWlUuZGVWLP1zWID",
    "83MiWjXg68KZdLJe",
    "MKoLaja28zpVvFJz",
    "KuXOT5LQXVGql87y",
    "adeCvRTxxAbOXER",
    };
   

int main () {
    Hashtable *ht = hashtable_create(sizeof(int), 1);
    assert(hashtable_empty(ht) == true);

    int num_to_test = 1000;
    for (int i = 0; i < num_to_test; i++) {
        assert(hashtable_put(ht, test_arr[i], &i) == true);
    }

    for (int i = 0; i < num_to_test; i++) {
        assert(hashtable_contains(ht, test_arr[i]) == true);
    }
    hashtable_stats(ht, "Just put 1000 elements total, put and contains working");




    hashtable_deinit(ht);
    free(ht);
}