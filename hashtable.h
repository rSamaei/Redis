#pragma once

#include <stddef.h>
#include <stdint.h>

struct HNode {
    HNode *next = NULL;
    uint64_t hcode = 0; // cached hash value
};

struct HTab {
    HNode **tab = NULL;
    size_t mask = 0;
    size_t size = 0;
};

// the real hashtable interface
struct HMap {
    HTab ht1;   //newer
    HTab ht2;   //older
    size_t resizing_pos = 0;
};

void h_init(HTab *htab, size_t);
void h_insert(HTab *htab, HNode *node);
HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *));
HNode *h_detach(HTab *htab, HNode **from);
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_help_resizing(HMap *hmap);
size_t hm_size(HMap *hmap);
HNode *hm_pop(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));