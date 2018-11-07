#ifndef  __HDC_TRIE_H
#define  __HDC_TRIE_H
class HTrie
{
public:
    HTrie();
    ~HTrie();
    void* Get(const char* ptr, unsigned int len);
    void  Set(const char* ptr, unsigned int len, void* data);
    void  _Set(const char* ptr, unsigned len, void* data);
    void* GetDel(const char* ptr, unsigned int len);
    void* GetDel();
private:
    void* _GetDel1(const char* ptr, unsigned int len, bool & del, int id);
    void  _Set1(const char* ptr, unsigned int len, void* data, int id);
    void* Get1(const char* ptr, unsigned int len, int id);
    void* _GetDel(const char* ptr, unsigned int len, bool & del);
    void* _GetDel(bool & del);
    void*   m_data;
    HTrie*  m_node[16];
};
#endif
