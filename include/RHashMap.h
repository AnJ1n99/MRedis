// Define the intrusive list node
#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

const size_t k_max_load_factor = 8;

// hashtable node, should be embedded into the payload
struct HNode {
    HNode *next = NULL;
    uint64_t hcode = 0;
};

// Define the fixed-size hashtable
struct HTab {
    HNode **tab = NULL; // array of slots
    size_t mask = 0;    // power of 2 array size
    size_t size = 0;    // number of keys
};

// HMap由两个固定的哈希表组成，一个新表和一个旧表
struct HMap {
    HTab newer;
    HTab older;
    size_t migrate_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
// invoke the callback on each node until it returns false
void   hm_foreach(HMap *hmap, bool (*f)(HNode *node, void *), void *arg);
size_t hm_size(HMap *hmap);