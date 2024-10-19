
This repository contains a basic Open Addressing Hashtable implementation in C.
It is currently designed to allow quick lookup of a value based on based on a key as a null terminated string.
The default hash function used is djb2.
The use of either linear or quadratic probing can be selected via a macro in hashtable.h.
The maximum key length(default 256 bytes) can be adjusted via a macro as well as the target load factor(default 0.65).


Possible Ideas to research or implement:
    make it so the hash_func is used as a function pointer and is user swappable but djb2 by default
    make the hash function work on any random assortment of bytes 
    make sure the hashes returned by hash_func are u64s and add to documentation that all hash functions must return u64/size_t
    consider changing the OFFSET macro to a an inlined function
    consider using a macro to wrap init or create so sizeof isn't needed
