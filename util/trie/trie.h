#pragma once
#ifdef __cplusplus
extern "C" {
#endif 
/*
 字典树
 */
typedef struct {
    unsigned char k;
    unsigned char max;
    unsigned short size;
    void ** array;
    void *  data;
} Htrie;

int trieSet(Htrie* h, const char* key, int keyLen, void *data);
void* trieGet(Htrie* h, const char* key, int keyLen);
void* trieGetAndDel(Htrie* h, const char* key, int keyLen);

#ifdef __cplusplus
}
#endif
