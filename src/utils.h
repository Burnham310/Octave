#ifndef UTILS_H_
#define UTILS_H_

#include "backend.h"
#define THROW_EXCEPT() exit(1)
#define eprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define report(lexer, off, fmt, ...)                                                       \
    do                                                                                     \
    {                                                                                      \
        Loc __loc = off_to_loc(lexer->src, lexer->src_len, off);                           \
        eprintf("%s:%zu:%zu: " fmt "\n", lexer->path, __loc.row, __loc.col, ##__VA_ARGS__); \
    } while (0)
#define WARNING(lexer, off, fmt, ...)                                                               \
    do                                                                                              \
    {                                                                                               \
        Loc __loc = off_to_loc(lexer->src, lexer->src_len, off);                                    \
        eprintf("warning: %s:%zu:%zu " fmt "\n", lexer->path, __loc.row, __loc.col, ##__VA_ARGS__); \
    } while (0)
#define ASSERT(bool, task) \
    do                     \
    {                      \
        if (!bool)         \
        {                  \
            task;          \
            return 1;      \
        }                  \
    } while (0)

#endif
