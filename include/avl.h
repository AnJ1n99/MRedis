#pragma once

#include "stddef.h"
#include "stdint.h"
#include <cstddef>
#include <cstdint>
#include <sys/types.h>


struct AVLNode {
    AVLNode* parent;
    AVLNode* left;
    AVLNode* right;
    uint32_t height = 0;       // auxiliary data for AVL tree
};

// 将节点设置为叶子状态，高度设为 1
inline void avl_init(AVLNode* node) {
    node->left = node->right = node->parent = NULL;
    node->height = 1;
}

inline uint32_t avl_height(AVLNode* node) {
    return node ? node->height : 0;
}

// API 
AVLNode* avl_fix(AVLNode* node);
AVLNode* avl_del(AVLNode* node);
AVLNode* *avl_offset(AVLNode* node, int64_t offset);


