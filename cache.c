#include "uffd.h"

// Evicts the cache entry from the given index and returns it
void *evict_cache_entry(int index, void *cache[], int *pnentries) {
    void *ret = cache[index];
    for (int i = index + 1; i < *pnentries; ++i) {
        cache[i - 1] = cache[i]; 
    }
    cache[--(*pnentries)] = NULL;
    return ret;
}

// Adds a cache entry while maintaining LRU order. If eviction is required
// to accommodate new entry, returns evicted entry, else returns 0
void *add_cache_entry(void *new_entry, void *cache[], int *pnentries) {
    void *ret = NULL;
    if (*pnentries == 0)
        cache[(*pnentries)++] = new_entry;
    else if (*pnentries < CACHE_SIZE) {
        // check if entry already exists
        int found = 0;
        for (int i = 0; i < *pnentries; ++i) {
            if (cache[i] == new_entry) {
                // if exists, push it to the end since its not LRU
                cache[(*pnentries)++] = evict_cache_entry(i, cache, pnentries);
                found = 1;
                break;
            }
        }
        if (!found) {
            cache[(*pnentries)++] = new_entry;
        }
    } else {// *nentries == CACHE_SIZE
        // if cache is full, evict LRU entry to accommodate new entry
        // by convention, the first entry is LRU
        ret = evict_cache_entry(0, cache, pnentries);
        cache[(*pnentries)++] = new_entry;
    }
    return ret;
}

void dump_cache(void *cache[], int nentries) {
    if (nentries == 0) {
        fprintf(stderr, "<EMPTY_CACHE>\n");
    } else {
        fprintf(stderr, "(%p", cache[0]);
        for (int i = 1; i < nentries; ++i) {
            fprintf(stderr, ", %p", cache[i]);
        }
        fprintf(stderr, ")\n");
    }
}