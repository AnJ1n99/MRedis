#pragma once
#include <stddef.h>
#include <stdint.h>

/*
 * container_of - get the containing struct from a member pointer
 * @ptr: the pointer to the member.
 * @type: the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */

#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })

  // FNV Hash
  // Do not use hashtable hash functions for anything
  // because they are not strong enough

inline uint64_t str_hash(const uint8_t* data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i] * 0x01000193);
    }
    return h;
}