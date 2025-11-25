#include "RHashMap.h"
#include "common.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

const size_t k_max_msg = 32 << 20;  // likely larger than the kernel buffer

typedef std::vector<uint8_t> Buffer; // 字节缓冲区
// append to the back
static void buf_append(Buffer &buf, const uint8_t *data, size_t len) {
	buf.insert(buf.end(), data, data + len);
}

// explicitly stored state
struct Conn {
	int fd = -1;
	// application's intention, for the event loop
	bool want_read  = false;
	bool want_write = false;
	bool want_close = false;
	// buffered input and output
	Buffer incoming;
	Buffer outgoing;
};

// global states
static struct {
  	HMap db; // top level HashTable
} g_data;

// KV pair for the top-level hashtable
struct Entry {
	struct HNode node;
	std::string key;
	std::string val;
};

// value type
enum class ValueType : uint8_t {
	T_INT = 0,
	T_STR = 1,  // string
	T_ZSET = 2, // sorted set
};

enum class ErrCode : uint8_t {
	ERR_UNKNOWN = 1, // unknown command
	ERR_TOO_BIG = 2, // response too big
	ERR_BAD_TYP = 3, // unexpected value type
	ERR_BAD_ARG = 4, // bad arguments
};

// data types of serialized data
enum class Tag : uint8_t {
	TAG_NIL = 0, // nil
	TAG_ERR = 1, // error code + msg
	TAG_STR = 2, // string
	TAG_INT = 3, // int64
	TAG_DBL = 4, // double
	TAG_ARR = 5, // array
};

// help functions for the serialization
static void buf_append_u8(Buffer &buf, uint8_t data) {
	buf.push_back(data);
}

static void buf_append_u32(Buffer &buf, uint32_t data) {
  	buf_append(buf, (const uint8_t *)&data, 4); // assume little endian
}

/*
		append serialized data types to the back
	@tag : 1 byte
	@len : 4 bytes
	@str : any bytes
	@array : number of data types
		@MODE : TLV
*/
static void out_nil(Buffer &out) {
	buf_append_u8(out, static_cast<uint8_t>(Tag::TAG_NIL));
}

static void out_str(Buffer &out, const char *s, size_t len) {
	buf_append_u8(out, static_cast<uint8_t>(Tag::TAG_STR));
	buf_append_u32(out, (uint32_t)len);
	buf_append(out, (const uint8_t *)s, len);
}

static void out_int(Buffer &out, int64_t val) {
	buf_append_u8(out, static_cast<uint8_t>(Tag::TAG_INT));
	buf_append_i64(out, val);
}

static void out_arr(Buffer &out, uint32_t n) {
	buf_append_u8(out, static_cast<uint8_t>(Tag::TAG_ARR));
	buf_append_u32(out, n);
}

// for lookup
struct LookUpKey {
	struct HNode node; //  hashtable node
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
	key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
	// hashtable lookup
	HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
	if (!node) {
		return out_nil(out);
	}
	// copy the value
	Entry *ent = container_of(node, Entry, node);
	if (ent->val.type != ValueType::T_STR) {
		return out_err(out, ErrCode::ERR_BAD_TYP, "not a string value");
	}
	return out_str(out, ent->val.data(), ent->val.size());
}

static void do_set(std::vector<std::string> &cmd, Buffer &out) {
	return out_nil(out);
}

static void do_del(std::vector<std::string> &cmd, Buffer &out) {
  return out_int(out, node ? 1 : 0);
}

//               add 'keys' command ->  KEYS 命令是 Redis 中用于查找符合给定模式的所有 key 的命令 

// 将数据库中每个条目的键（key）写入输出缓冲区
static bool cb_keys(HNode *node, void *arg) {
	Buffer &out = *(Buffer *)arg;  // 传递的上下文，此处为指向 Buffer 的指针，用于写入响应
	const std::string &key = container_of(node, Entry, node)->key;
	out_str(out, key.data(), key.size());
	return true;
}

static void do_keys(std::vector<std::string> &, Buffer &out) {
	out_arr(out, (uint32_t)hm_size(&g_data.db));
	hm_foreach(&g_data.db, &cb_keys, (void*)&out);
}

static void response_begin(Buffer &out, size_t *header) {
	*header = out.size();                       // message header pos
	buf_append_u32(out, 0);          // reserve space
}

static size_t response_size(Buffer &out, size_t header) {
	return out.size() - header - 4;
}

static void response_end(Buffer &out, size_t header) {
    size_t msg_size = response_size(out, header);

    if (msg_size > k_max_msg) {
        out.resize(header + 4);
        our_err(out, ErrCode::ERR_TOO_BIG, "response is too big");
        msg_size = response_size(out, header);
    }
    // 消息长度写入输出缓冲区的头部位置
    uint32_t len = (uint32_t)msg_size;
    memcpy(&out[header], &len, 4);
}

// process 1 request if there is enough data
static bool try_one_request(Conn *conn) {
	// the size of response is unknown so we keep a head
	// to do ???
	size_t header_pos = 0;
	response_begin(conn->outgoing, &header_pos); // save the header pos
	do_request(cmd, conn->outgoing);
	response_end(conn->outgoing, header_pos);
}

