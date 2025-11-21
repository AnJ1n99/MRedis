#include "RHashMap.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "common.h"

typedef std::vector<uint8_t> Buffer; // 字节缓冲区

static struct {
	HMap db; // top level HashTable
} g_data;

enum class ErrCode : uint8_t {
    ERR_UNKNOWN = 1,    // unknown command
    ERR_TOO_BIG = 2,    // response too big
    ERR_BAD_TYP = 3,    // unexpected value type
    ERR_BAD_ARG = 4,    // bad arguments
};

// data types of serialized data
enum class Tag : uint8_t {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

// value type
enum class ValueType : uint8_t {
	T_INT = 0,
	T_STR = 1, // string
	T_ZSET = 2, // sorted set
};


// KV pair for the top-level hashtable
struct Entry {
    struct HNode node;
	std::string key;
	std::string val;
};

/*
	append serialized data types to the back
    @tag : 1 byte
    @len : 4 bytes
    @str : any bytes
    @array : number of data types
	@MODE : TLV
*/
static void out_nil(Buffer &out) {
	buf_append_u8(out, Tag::TAG_NIL);
}

static void out_str(Buffer &out, const char* s, size_t len) {
	buf_append_u8(out, Tag::TAG_STR);
	buf_append_u32(out, (uint32_t)len);
	buf_append(out, (uint8_t*)s, len);
}
// int && len is host 
static void out_int(Buffer &out, int64_t val) {
	buf_append_u8(out, Tag::TAG_INT);
	buf_append_i64(out, val);
}

static void out_arr(Buffer &out, uint32_t n) {
	buf_append_u8(out, Tag::TAG_ARR);
	buf_append_u32(out, n);
}

// for lookup
struct LookUpKey {
	struct HNode node;  //  hashtable node
	std::string key;
};

// equality comparison for the top-level hashstable(callback)
static bool entry_eq(HNode *lhs, HNode *rhs) {
	struct Entry *ent = container_of(lhs, struct Entry, node);
	struct LookUpKey *keydata = container_of(rhs, struct LookUpKey, node);
	return ent->key == keydata->key;
}

static void do_get(std::vector<std::string> &cmd, Buffer &out) {
	// dummy 'Entry that just have a key' just for the loopup
    LookUpKey key;
	key.key.swap(cmd[1]);
	key.node.hcode = str_hash((uint8_t*)key.key.data(), key.key.size());
	// hashtable lookup
	HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
	if (!node) {
		return out_nil(out);
	}
	// copy the value 
	Entry* ent = container_of(node, Entry, node);
	if (ent->val.type != ValueType::T_STR) {
		return out_err(out, ErrCode::ERR_BAD_TYP, "not a string value");
	}
	return out_str(out, ent->val.data(), ent->val.size());
}

static void do_set(std::vector<std::string> &cmd, Buffer &out) {
	return out_nil(out);
}

static void do_del(std::vector<std::string> &cmd, Buffer &out) {
	return out_int(out, node ? 1 : 0)
}
