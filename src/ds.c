#include "ds.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"


// #define STBDS_SIZE_T_BITS           ((sizeof (size_t)) * 8)
// #define STBDS_ROTATE_LEFT(val, n)   (((val) << (n)) | ((val) >> (STBDS_SIZE_T_BITS - (n))))
// #define STBDS_ROTATE_RIGHT(val, n)  (((val) >> (n)) | ((val) << (STBDS_SIZE_T_BITS - (n))))
// static unsigned int hash(String str, size_t seed)
// {
//   size_t value = seed;
//   for (size_t i = 0; i < str.len; ++i)
//      value = STBDS_ROTATE_LEFT(value, 9) + (unsigned char) str.ptr[i];
// 
//   // Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
//   value ^= seed;
//   value = (~value) + (value << 18);
//   value ^= value ^ STBDS_ROTATE_RIGHT(value,31);
//   value = value * 21;
//   value ^= value ^ STBDS_ROTATE_RIGHT(value,11);
//   value += (value << 6);
//   value ^= STBDS_ROTATE_RIGHT(value,22);
//   return value+seed;
// 
// }
// 
// Hashmap hm_init(size_t size)
// {
//     Hashmap hashmap = {};
//     hashmap.size = size;
//     hashmap.buckets = malloc(sizeof(HashmapEntry *) * size);
// 
//     for (int i = 0; i < size; ++i)
//         hashmap.buckets[i] = NULL;
// 
//     return hashmap;
// }
// 
// int hm_insert(Hashmap *hashmap, String str, void *value)
// {
//     int index = hash(str, hashmap->size);
// 
//     // if (hashmap->buckets[index] != NULL) {
//     //     hashmap->buckets[index]
//     // }
// 
//     HashmapEntry *entry = malloc(sizeof(HashmapEntry));
//     entry->key = str;
//     entry->value = value;
// 
//     hashmap->buckets[index] = entry;
//     return 0;
// }
// void *hm_search(Hashmap *hashmap, String str);
// void hm_delete(Hashmap *hashmap, String str);
// void hm_free(Hashmap *hashmap);

// make_arr(Slice);

// ArrOf(Slice) cc = {0};

// da_append(cc, (Slice){
//                   .data = "12",
//                   .len = 21,
//               },);
