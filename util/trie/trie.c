#include "trie.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define TRIE_BLOCK  16

Htrie * HtrieNew()
{
    Htrie * n = (Htrie*)malloc(sizeof(Htrie));
    memset(n,0,sizeof(Htrie));
    return n;
}

int HtrieRealloc(Htrie* h)
{
    if (0 == h->array){
        h->array = (void**)malloc(TRIE_BLOCK*sizeof(void*));
    }else{
        h->array = (void**)realloc(h->array, TRIE_BLOCK*sizeof(void*));
    }
    if (h->array==0){
        return -1;
    }
    int base = (int)(h->max) * TRIE_BLOCK;
    int i=0;
    for (; i<TRIE_BLOCK; i++){
        h->array[i+base] = 0;
    }
    h->max++;
}

Htrie * HtrieAddNew(Htrie *h, int index, unsigned char k)
{
    int max = (int)(h->max)*TRIE_BLOCK;
    if (h->size >= max && -1 == HtrieRealloc(h)){
        return 0;
    }
    max = (int)(h->max)*TRIE_BLOCK;
    if (index >= max){
        return 0;
    }
    void * p = h->array[index];
    int i = index+1;
    for (; i<=h->size; i++){
        void * q = h->array[i];
        h->array[i] = p;
        p = q;
    }
    Htrie * r = HtrieNew();
    r->k = k;
    h->array[index] = (void*)r;
    h->size++;
    return r;
}

Htrie * HtrieInsert(Htrie *h, unsigned char k)
{
    if (0==h->size){
        return HtrieAddNew(h, 0, k);
    }
    unsigned char s = 0;
    unsigned char e = (unsigned char)(h->size - 1);
    unsigned char d = e;
    if (e > k){
        d = k;
    }
    while (s<=d){
        Htrie* st = (Htrie*)(h->array[s]);
        if (st->k == k){
            return st;
        }
        Htrie* dt = (Htrie*)(h->array[d]);
        if (dt->k == k){
            return dt;
        }
        if (k < st->k){
            return HtrieAddNew(h, s, k);
        }
        if (k > dt->k){
            return HtrieAddNew(h, d+1, k);
        }
        unsigned char i = (d-s)/2 + s;
        if (i == s){
            return HtrieAddNew(h, s+1, k);
        }
        Htrie* it = (Htrie*)(h->array[i]);
        if (it->k == k){
            return it;
        }
        if (it->k > k){
            d = i;
        }else{
            s = i;
        }
    }
    return 0;
}


Htrie * HtrieSearch(Htrie *h, unsigned char k)
{
    if (0==h->size){
        return 0;
    }
    unsigned char s = 0;
    unsigned char e = (unsigned char)(h->size - 1);
    unsigned char d = e;
    if (e > k){
        d = k;
    }
    while (s<=d){
        Htrie* st = (Htrie*)(h->array[s]);
        if (st->k == k){
            return st;
        }
        Htrie* dt = (Htrie*)(h->array[d]);
        if (dt->k == k){
            return dt;
        }
        if (k < st->k || k> dt->k || s == d){
            return 0;
        }
        unsigned char i = (d-s)/2 + s;
        Htrie* it = (Htrie*)(h->array[i]);
        if (it->k == k){
            return it;
        }
        if (it->k > k){
            d = i;
        }else{
            s = i;
        }
    }
    return 0;
}

int trieSet(Htrie *h, const char* key, int keyLen, void *data)
{
    if (0==h){
        return -1;
    }
    Htrie* p = HtrieInsert(h, key[0]);
    if (0==p){
        return -1;
    }
    if (keyLen==1){
        p->data = data;
        return 0;
    }
    return trieSet(p, ++key, --keyLen, data);
}

void* trieGet(Htrie *h, const char* key, int keyLen)
{
    if (0==h){
        return 0;
    }
    Htrie* p = HtrieSearch(h, key[0]);
    if (0==p){
        return 0;
    }
    if (keyLen==1){
        return p->data;
    }
    return trieGet(p, ++key, --keyLen);
}

void* trieGetAndDel(Htrie *h, const char* key, int keyLen)
{
    if (0==h){
        return 0;
    }
    Htrie* p = HtrieSearch(h, key[0]);
    if (0==p){
        return 0;
    }
    if (keyLen==1){
        void *c = p->data;
        p->data = 0;
        //if (p->size == 0 && p->) //free()
        return c;
    }
    return trieGet(p, ++key, --keyLen);
}

int main(int argv, char** argc)
{
    printf("start...\n");
    Htrie * head = HtrieNew();
    printf("start..1\n");
    const char * test = "1234567890";
    trieSet(head, test, 10, (void*)test);
    printf("start..2\n");
    const char * p = (const char*)trieGet(head, test, 10);
    printf("start..3\n");
    if (p){
        printf("%s\n", p);
    }
}

