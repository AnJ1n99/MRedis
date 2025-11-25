#include "../include/avl.h"
#include <algorithm>
#include <cstdint>
#include "assert.h"

static uint32_t avl_update(AVLNode* node) {
    // 高度的变化从子节点传播到父节点
    return 1 + std::max(avl_height(node->left), avl_height(node->right));
}
     // 单旋（旋转操作传入的均是最低层的失衡节点
     // 3+4 算法更为统一，但因为 Redis 性能问题，选择更传统的双旋）
// zig 
static AVLNode* rot_left(AVLNode* node) {
    AVLNode* parent = node->parent;
    AVLNode* new_node = node->right;
    AVLNode *inner_node = new_node->left;
    
    node->right = inner_node;
    if (inner_node) {
        inner_node->parent = node;
    }

    new_node->parent = parent;
    new_node->left = node;
    node->parent = new_node;

    avl_update(node);
    avl_update(new_node);
    return new_node;
}

// zag 
static AVLNode* rot_right(AVLNode* node) {
    AVLNode* parent = node->parent;
    AVLNode* new_node = node->left;
    AVLNode *inner_node = new_node->right;
    
    node->left = inner_node;
    if (inner_node) {
        inner_node->parent = node;
    }

    new_node->parent = parent;
    new_node->right = node;
    node->parent = new_node;

    avl_update(node);
    avl_update(new_node);
    return new_node;
}

static AVLNode* avl_fix_left(AVLNode* node) {
    // 判断旋转方向 LL/LR
    if (avl_height(node->left->left) < avl_height(node->left->right)) {
        node->left = rot_left(node->left); 
    }
    // main rotate
    return rot_right(node);
}

static AVLNode* avl_fix_right(AVLNode* node) {
    // RR/ RL
    if (avl_height(node->right->left) > avl_height(node->right->right)) {
        node->right = rot_right(node->right);
    }
    return rot_left(node);
}

// fix imbalanced nodes and maintain invariants until the root is reached
AVLNode* avl_fix(AVLNode* node) {
    while (true) {
        AVLNode** from = &node; // save the subtree
        AVLNode* parent = node->parent;
        if (parent) { // not a root node
            // attach the fixed subtree to the parent
            from = parent->left == node ? &parent->left : &parent->right;
        }

        avl_update(node);

        // 检查左右子树高度差是否为 2（失衡）
        int32_t diff = avl_height(node->left) - avl_height(node->right);
        if (diff == 2) {
            node = avl_fix_left(node);  // 左侧过高，进行左平衡修复
        } else if (diff == -2) {
            node = avl_fix_right(node); // 右侧过高，进行右平衡修复
        }
        // 将修复后的子树挂回父节点
        *from = node;

        if (!parent) 
            break;

        node = parent;
    }
    return node;
}

// detach a node where 1 of its children is empty
static AVLNode* avl_del_easy(AVLNode* node) {
    assert(!node->left || !node->right);    // at most 1 child
    AVLNode* child = node->left ? node->left : node->right; // can be NULL
    AVLNode* parent = node->parent;

    // update the child's parent pointer
    if (child) {
        child->parent = parent;
    }

    if (!parent) {
        return child;
    }

    AVLNode **from = parent->left == node ? &parent->left : &parent->right;
    *from = child;

    return avl_fix(parent);
}

// detach a node and returns the new root of the tree

AVLNode* avl_del(AVLNode* node) {
    // the easy case of 0 or 1 child
    if (!node->left || !node->right) {
        return avl_del_easy(node);
    }

    // find the successor  这里要懂得中序遍历的规律
    AVLNode* victim = node->right;
    while (victim->left) {
        victim = victim->left;
    }

    // detach the successor 
    AVLNode *root = avl_del_easy(victim);
    // swap the successor 
    *victim = *node;
    if (victim->left) {
        victim->left->parent = victim;
    }
    if (victim->right) {
        victim->right->parent = victim;
    }

    AVLNode** from = &root;
    AVLNode* parent = node->parent;
    if (parent) {
        from = parent->left == node ? &parent->left : &parent->right;
    }

    *from = victim;
    return root;
}
