#include <stdint.h>
#include <cstddef>

struct AVLNode {
  uint32_t depth = 0;   // subtree height
  uint32_t cnt = 0;     // subtree size
  AVLNode *left = NULL;
  AVLNode *right = NULL;
  AVLNode *parent = NULL;
};

static void avl_init(AVLNode *node){
    node->depth = 1;
    node->cnt = 1;
    node->left = node->right = node->parent = NULL;
}

static uint32_t avl_depth(AVLNode *node){
    return node ? node->depth : 0;
}

static uint32_t avl_cnt(AVLNode *node){
    return node ? node->cnt : 0;
}

static uint32_t max(uint32_t left, uint32_t right){
    return left>right ? left : right;
}

// maintain the count and depth field 
static void avl_update(AVLNode *node){
    node->depth = 1 + max(avl_depth(node->left), avl_depth(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

static AVLNode *rot_left(AVLNode *node) {
    AVLNode *new_root = node->right;  // The right child of the current node becomes the new root of the subtree.
    if (new_root->left) {  // If the new root has a left child,
        new_root->left->parent = node;  // set the current node as the parent of the new root's left child.
    }
    node->right = new_root->left;  // The current node's right child is now the left child of the new root.
    new_root->left = node;  // The current node becomes the left child of the new root.
    new_root->parent = node->parent;  // The new root takes the parent pointer of the current node.
    node->parent = new_root;  // The current node's parent pointer now points to the new root.
    avl_update(node);  // Update the height and balance factor of the current node.
    avl_update(new_root);  // Update the height and balance factor of the new root.
    return new_root;  // Return the new root of the subtree.
}


static AVLNode *rot_right(AVLNode *node){
    AVLNode *new_root = node->left;
    if(new_root->right){
        new_root->right->parent = node;
    }
    node->left = new_root->right;
    new_root->right = node;
    new_root->parent = node->parent;
    node->parent = new_root;
    avl_update(node);
    avl_update(new_root);
    return new_root;
}

// the left subtree is too deep
static AVLNode *avl_fix_left(AVLNode *root){
    if(avl_depth(root->left->left) < avl_depth(root->left->right)){
        root->left = rot_left(root->left);  // rule 2
    }
    return rot_right(root);
}

// check this if wrong
static AVLNode *avl_fix_right(AVLNode *root){
    if(avl_depth(root->right->right) < avl_depth(root->right->left)){
        root->right = rot_right(root->right);  // rule 2
    }
    return rot_left(root);
}

static AVLNode *avl_fix(AVLNode *node){
    while(true){
        avl_update(node);
        uint32_t l = avl_depth(node->left);
        uint32_t r = avl_depth(node->right);
        AVLNode **from = NULL;
        if(AVLNode *p = node->parent){
            from = (p->left == node) ? &p->left : &p->right;
        }
        if(l == r + 2){
            node = avl_fix_left(node);
        } else if(l + 2 == r){
            node = avl_fix_right(node);
        }
        if(!from){
            return node;
        }
        *from = node;
        node = node->parent;
    }
}

static AVLNode *avl_del(AVLNode *node) {
    if (node->right == NULL) {
        // Case 1: No right subtree, replace the node with the left subtree
        // link the left subtree to the parent
        AVLNode *parent = node->parent;
        if (node->left) {
            // Update the parent pointer of the left child
            node->left->parent = parent;
        }
        if (parent) {
            // If the node to be deleted has a parent, update the parent's child pointer
            (parent->left == node ? parent->left : parent->right) = node->left;
            // Fix the AVL tree from the parent node upwards
            return avl_fix(parent);
        } else {
            // If the node to be deleted is the root, return the left child as the new root
            return node->left;
        }
    } else {
        // Case 2: Node has a right subtree, find the successor
        AVLNode *victim = node->right;
        while (victim->left) {
            // Find the leftmost (smallest) node in the right subtree
            victim = victim->left;
        }
        // Detach the successor node
        AVLNode *root = avl_del(victim);
        // Swap the successor's data with the node to be deleted
        *victim = *node;
        // Update the parent pointers of the successor's children
        if (victim->left) {
            victim->left->parent = victim;
        }
        if (victim->right) {
            victim->right->parent = victim;
        }
        if (AVLNode *parent = node->parent) {
            // If the node to be deleted has a parent, update the parent's child pointer
            (parent->left == node ? parent->left : parent->right) = victim;
            // Return the root of the fixed AVL tree
            return root;
        } else {
            // If the node to be deleted is the root, return the successor as the new root
            return victim;
        }
    }
}

AVLNode *avl_offset(AVLNode *node, int64_t offset){
    int64_t pos = 0;
    while(offset != pos){
        if(pos < offset && pos + avl_cnt(node->right) >= offset){
            // the target is inside the right subtree
            node = node->right;
            pos += avl_cnt(node->left) + 1;
        } else if(pos > offset && pos - avl_cnt(node->left) <= offset){
            // the target is inside the left subtree
            node = node->left;
            pos -= avl_cnt(node->right) + 1;
        } else {
            // go to parent
            AVLNode *parent = node->parent;
            if(!parent){
                return NULL;    // out of range
            }
            if(parent->right == node){
                pos -= avl_cnt(node->left) + 1;
            } else {
                pos += avl_cnt(node->right) + 1;
            }
            node = parent;
        }
    }
    return node;
}
