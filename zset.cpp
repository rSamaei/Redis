#include <assert.h>
#include <string.h>
#include <stdlib.h>
// proj
#include "zset.h"
#include "common.h"

enum {
    T_STR = 0,      // string 
    T_ZSET = 1,     // sorted set
};

// // the structure for a key
// struct Entry {
//     struct HNode node;
//     std::string key;
//     uint32_t type = 0;
//     std::string val;    // string
//     ZSet *zset = NULL;  // sorted set
// };

// a helper structure for the hashtable lookup
struct HKey {
    HNode node;
    const char *name = NULL;
    size_t len = 0;
};

static ZNode *znode_new(const char *name, size_t len, double score) {
    ZNode *node = (ZNode *)malloc(sizeof(ZNode) + len);  // Allocate memory for ZNode including variable length name
    avl_init(&node->tree);  // Initialize AVL tree node
    node->hmap.next = NULL;  // Initialize HNode next pointer
    node->hmap.hcode = str_hash((uint8_t *)name, len);  // Hash the name
    node->score = score;  // Set the score
    node->len = len;  // Set the length of the name
    memcpy(&node->name[0], name, len);  // Copy the name into the node
    return node;  // Return the new node
}

static uint32_t min(size_t lhs, size_t rhs) {
    return lhs < rhs ? lhs : rhs;
}

static bool hcmp(HNode *node, HNode *key) {
    ZNode *znode = container_of(node, ZNode, hmap);  // Get ZNode from HNode
    HKey *hkey = container_of(key, HKey, node);  // Get HKey from HNode
    if (znode->len != hkey->len) {
        return false;  // Lengths must match
    }
    return 0 == memcmp(znode->name, hkey->name, znode->len);  // Compare names
}

ZNode *zset_lookup(ZSet *zset, const char *name, size_t len) {
    if (!zset->tree) {  // If the tree is empty
        return NULL;
    }
    HKey key;
    key.node.hcode = str_hash((uint8_t *)name, len);  // Hash the name
    key.name = name;  // Set the name
    key.len = len;  // Set the length
    HNode *found = hm_lookup(&zset->hmap, &key.node, &hcmp);  // Lookup in the hash map
    return found ? container_of(found, ZNode, hmap) : NULL;  // Return the found node or NULL
}

// compare by the (score, name) tuple
static bool zless(AVLNode *lhs, double score, const char *name, size_t len) {
    ZNode *zl = container_of(lhs, ZNode, tree);  // Get ZNode from AVLNode
    if (zl->score != score) {
        return zl->score < score;  // Compare scores
    }
    int rv = memcmp(zl->name, name, min(zl->len, len));  // Compare names
    if (rv != 0) {
        return rv < 0;
    }
    return zl->len < len;  // Compare lengths if names are equal
}

static bool zless(AVLNode *lhs, AVLNode *rhs) {
    ZNode *zr = container_of(rhs, ZNode, tree);  // Get ZNode from AVLNode
    return zless(lhs, zr->score, zr->name, zr->len);  // Compare using the zless function
}

bool zset_add(ZSet *zset, ZNode *node) {
    AVLNode *cur = NULL;            // current node
    AVLNode **from = &zset->tree;   // incoming pointer to the next node
    while (*from) {
        cur = *from;  // Move down the tree
        from = zless(&node->tree, cur) ? &cur->left : &cur->right;  // Decide to go left or right
    }
    *from = &node->tree;  // Place the new node
    node->tree.parent = cur;  // Set parent pointer
    zset->tree = avl_fix(&node->tree);  // Fix the AVL tree balance
}

static void zset_update(ZSet *zset, ZNode *node, double score) {
    if (node->score == score) {
        return;  // No update needed if scores are equal
    }
    zset->tree = avl_del(&node->tree);  // Delete the node from the AVL tree
    node->score = score;  // Update the score
    zset_add(zset, node);  // Re-insert the node
}

// deletion by name
ZNode *zset_pop(ZSet *zset, const char *name, size_t len) {
    if (!zset->tree) {  // If the tree is empty
        return NULL;
    }

    HKey key;
    key.node.hcode = str_hash((uint8_t *)name, len);  // Hash the name
    key.name = name;  // Set the name
    key.len = len;  // Set the length
    HNode *found = hm_pop(&zset->hmap, &key.node, &hcmp);  // Pop from the hash map
    if (!found) {
        return NULL;
    }

    ZNode *node = container_of(found, ZNode, hmap);  // Get ZNode from HNode
    zset->tree = avl_del(&node->tree);  // Delete from the AVL tree
    return node;  // Return the popped node
}

void znode_del(ZNode *node) {
    free(node);  // Free the memory allocated for the node
}

ZNode *zset_query(ZSet *zset, double score, const char *name, size_t len) {
    AVLNode *found = NULL;
    for (AVLNode *cur = zset->tree; cur; ) {
        if (zless(cur, score, name, len)) {
            cur = cur->right;  // Move right
        } else {
            found = cur;  // Possible match
            cur = cur->left;  // Move left
        }
    }
    return found ? container_of(found, ZNode, tree) : NULL;  // Return found node or NULL
}

ZNode *znode_offset(ZNode *node, int64_t offset) {
    AVLNode *tnode = node ? avl_offset(&node->tree, offset) : NULL;  // Find node at offset
    return tnode ? container_of(tnode, ZNode, tree) : NULL;  // Return found node or NULL
}
