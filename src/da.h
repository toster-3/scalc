// da.h -- simple dynamic arrays in c
//
// works on structs with the form:
// struct {
//     char *buf;
//     size_t len;
//     size_t cap;
//     ... anything else you want
// };

#ifndef DA_H_
#define DA_H_

#ifndef DA_INIT_CAP
#define DA_INIT_CAP 256
#endif

#include <stdio.h>
#include <stdlib.h>

#define da_reserve(xs, expected_cap) \
    do { \
        if ((expected_cap) > (xs)->cap) { \
            if ((xs)->cap == 0) { \
                (xs)->cap = DA_INIT_CAP; \
            } \
            while ((expected_cap) > (xs)->cap) { \
                (xs)->cap *= 2; \
            } \
            (xs)->buf = realloc((xs)->buf, (xs)->cap * sizeof(*(xs)->buf)); \
            if (!(xs)->buf) { \
                perror("realloc"); \
                exit(2); \
            } \
        } \
    } while (0)

#define da_append(xs, x) \
    do { \
        da_reserve((xs), (xs)->len + 1); \
        (xs)->buf[(xs)->len++] = (x); \
    } while (0)

#define da_free(xs) free((xs).buf)

#define da_append_many(xs, ys, len) \
    do { \
        da_reserve((xs), (xs)->len + (len)); \
        memcpy((xs)->buf + (xs)->len, (ys), (ys) * sizeof(*(xs)->buf)); \
        (xs)->len += (len); \
    } while (0)

#define da_last(xs) (xs)->buf[(xs)->len - 1]

#endif
