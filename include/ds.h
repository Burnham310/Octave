#ifndef DS_H_
#define DS_H_

#include <stddef.h>

typedef struct
{
    const char *ptr;
    size_t len;
} String;

// Dynamic Array ---
#define da_free(da)           \
    do                        \
    {                         \
        if ((da).cap > 0)     \
        {                     \
            (da).cap = 0;     \
            free((da.items)); \
        }                     \
    } while (0);
#define da_move(da, dest_slice)    \
    do                             \
    {                              \
        dest_slice.ptr = da.items; \
        dest_slice.len = da.size;  \
        da.cap = 0;                \
        da.size = 0;               \
        da.items = NULL;           \
    } while (0);
#define da_append(da, x)                                                        \
    do                                                                          \
    {                                                                           \
        if ((da).cap == 0)                                                      \
        {                                                                       \
            (da).cap = 10;                                                      \
            (da).items = calloc((da).cap, sizeof((da).items[0]));               \
        }                                                                       \
        if ((da).cap <= (da).size)                                              \
        {                                                                       \
            (da).cap *= 2;                                                      \
            (da).items = realloc((da).items, (da).cap * sizeof((da).items[0])); \
        }                                                                       \
        (da).items[(da).size++] = (x);                                          \
    } while (0)
#define da_append_slice(da, slice) \
    do { \
	if ((da).cap == 0) (da).cap = 5; \
	while ((da).cap < (da).size + (slice).len) { \
	    (da).cap *= 2; \
	} \
	(da).items = realloc((da).items, (da).cap * sizeof((da).items[0])); \
	memcpy((da.items) + (da).size, (slice).ptr, (slice).len * sizeof((da).items[0])); \
	(da).size += (slice).len; \
    } while (0)
// ---
#define SliceOf(ty) ty##Slice
#define ArrOf(ty) ty##Arr
#define make_slice(ty) \
    typedef struct     \
    {                  \
        ty *ptr;       \
        size_t len;    \
    } SliceOf(ty)
#define make_arr(ty) \
    typedef struct   \
    {                \
        ty *items;   \
        size_t cap;  \
        size_t size; \
    } ArrOf(ty)

// typedef struct HashmapEntry
// {
//
//     String key;
//     void *value;
//     struct HasmmapEntry* next;
// } HashmapEntry;
//
// typedef struct
// {
//     HashmapEntry **buckets;
//     int size;
// } Hashmap;

// Hashmap hm_init(size_t size);
// int hm_insert(Hashmap *hashmapm, String str, void *value);
// void *hm_search(Hashmap *hashmap, String str);
// void hm_delete(Hashmap *hashmap, String str);
// void hm_free(Hashmap *hashmap);

#endif
