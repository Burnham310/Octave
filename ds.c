#include "ds.h"
#include <stdio.h>

static unsigned int hash(OctaveStr *str, unsigned int n)
{
    unsigned long hash = 5381;
    for (size_t i = 0; i < str->len; ++i)
    {
        hash = ((hash << 5) + hash) + (unsigned char)str->data[i];
    }
    return hash % n;
}

Hashmap *hm_init(size_t size)
{
    Hashmap *hashmap = malloc(sizeof(Hashmap));
    hashmap->size = size;
    hashmap->buckets = malloc(sizeof(HashmapBucket *) * size);

    for (int i = 0; i < size; ++i)
        hashmap->buckets[i] = NULL;

    return hashmap;
}

void hm_insert(Hashmap *hashmap, OctaveStr *str, void *value)
{
    int index = hash(str, hashmap->size);

    // if (hashmap->buckets[index] != NULL) {
    //     hashmap->buckets[index]
    // }

    HashmapEntry *entry = malloc(sizeof(HashmapEntry));
    entry->key = str;
    entry->value = value;

    hashmap->buckets[index] = entry;
    return 0;
}
void *hm_search(Hashmap *hashmap, OctaveStr str);
void hm_delete(Hashmap *hashmap, OctaveStr str);
void hm_free(Hashmap *hashmap);

// make_arr(OctaveStr);

// ArrOf(OctaveStr) cc = {0};

// da_append(cc, (OctaveStr){
//                   .data = "12",
//                   .len = 21,
//               },);