static int Pe16[256][2] = {
    {0,0},{0,1},{0,2},{0,3},
    {0,4},{0,5},{0,6},{0,7},
    {0,8},{0,9},{0,10},{0,11},
    {0,12},{0,13},{0,14},{0,15},
    {1,0},{1,1},{1,2},{1,3},
    {1,4},{1,5},{1,6},{1,7},
    {1,8},{1,9},{1,10},{1,11},
    {1,12},{1,13},{1,14},{1,15},
    {2,0},{2,1},{2,2},{2,3},
    {2,4},{2,5},{2,6},{2,7},
    {2,8},{2,9},{2,10},{2,11},
    {2,12},{2,13},{2,14},{2,15},
    {3,0},{3,1},{3,2},{3,3},
    {3,4},{3,5},{3,6},{3,7},
    {3,8},{3,9},{3,10},{3,11},
    {3,12},{3,13},{3,14},{3,15},
    {4,0},{4,1},{4,2},{4,3},
    {4,4},{4,5},{4,6},{4,7},
    {4,8},{4,9},{4,10},{4,11},
    {4,12},{4,13},{4,14},{4,15},
    {5,0},{5,1},{5,2},{5,3},
    {5,4},{5,5},{5,6},{5,7},
    {5,8},{5,9},{5,10},{5,11},
    {5,12},{5,13},{5,14},{5,15},
    {6,0},{6,1},{6,2},{6,3},
    {6,4},{6,5},{6,6},{6,7},
    {6,8},{6,9},{6,10},{6,11},
    {6,12},{6,13},{6,14},{6,15},
    {7,0},{7,1},{7,2},{7,3},
    {7,4},{7,5},{7,6},{7,7},
    {7,8},{7,9},{7,10},{7,11},
    {7,12},{7,13},{7,14},{7,15},
    {8,0},{8,1},{8,2},{8,3},
    {8,4},{8,5},{8,6},{8,7},
    {8,8},{8,9},{8,10},{8,11},
    {8,12},{8,13},{8,14},{8,15},
    {9,0},{9,1},{9,2},{9,3},
    {9,4},{9,5},{9,6},{9,7},
    {9,8},{9,9},{9,10},{9,11},
    {9,12},{9,13},{9,14},{9,15},
    {10,0},{10,1},{10,2},{10,3},
    {10,4},{10,5},{10,6},{10,7},
    {10,8},{10,9},{10,10},{10,11},
    {10,12},{10,13},{10,14},{10,15},
    {11,0},{11,1},{11,2},{11,3},
    {11,4},{11,5},{11,6},{11,7},
    {11,8},{11,9},{11,10},{11,11},
    {11,12},{11,13},{11,14},{11,15},
    {12,0},{12,1},{12,2},{12,3},
    {12,4},{12,5},{12,6},{12,7},
    {12,8},{12,9},{12,10},{12,11},
    {12,12},{12,13},{12,14},{12,15},
    {13,0},{13,1},{13,2},{13,3},
    {13,4},{13,5},{13,6},{13,7},
    {13,8},{13,9},{13,10},{13,11},
    {13,12},{13,13},{13,14},{13,15},
    {14,0},{14,1},{14,2},{14,3},
    {14,4},{14,5},{14,6},{14,7},
    {14,8},{14,9},{14,10},{14,11},
    {14,12},{14,13},{14,14},{14,15},
    {15,0},{15,1},{15,2},{15,3},
    {15,4},{15,5},{15,6},{15,7},
    {15,8},{15,9},{15,10},{15,11},
    {15,12},{15,13},{15,14},{15,15}
};
#include "hdc_trie.h"
#include <glog/logging.h>
HTrie::HTrie():m_data(0)
{
    for (int i=0; i<16; i++){
        m_node[i] = 0;
    }
}

HTrie::~HTrie()
{
    for (int i=0; i<16; i++){
        if (m_node[i]){
            delete m_node[i];
        }
    }
}

void* HTrie::Get1(const char* ptr, unsigned int len, int id)
{
    if (len==0){
        return m_data;
    }
    if (!m_node[id]){
        return 0;
    }
    return m_node[id]->Get(++ptr, --len);
}

void* HTrie::Get(const char* ptr, unsigned int len)
{
    if (len==0){
        return m_data;
    }
    unsigned char c = (unsigned char)ptr[0];
    if (!m_node[Pe16[c][0]]){
        return 0;
    }
    return m_node[Pe16[c][0]]->Get1(ptr, len, Pe16[c][1]);
}

void  HTrie::Set(const char* ptr, unsigned int len, void* data)
{
    if (0==data){
        return;
    }
    _Set(ptr, len, data);
}

void  HTrie::_Set1(const char* ptr, unsigned int len, void* data, int id)
{
    if (!m_node[id]){
        m_node[id] = new HTrie();
    }
    m_node[id]->_Set(++ptr, --len, data);
}

void  HTrie::_Set(const char* ptr, unsigned int len, void* data)
{
    if (len == 0){
        m_data = data;
        return;
    }
    unsigned char c = (unsigned char)ptr[0];
    if (!m_node[Pe16[c][0]]){
        m_node[Pe16[c][0]] = new HTrie();
    }
    m_node[Pe16[c][0]]->_Set1(ptr, len, data, Pe16[c][1]);
}

void* HTrie::_GetDel1(const char* ptr, unsigned int len, bool & del, int id)
{
    if (!m_node[id]){
        for (int i=0; i<16; i++){
            if (m_node[i]){
                del = false;
            }
        }
        return 0;
    }
    bool _del = true;
    void * _data = m_node[id]->_GetDel(++ptr, --len, _del);
    if (_del){
        delete m_node[id];
        m_node[id] = 0;
    }
    for (int i=0; i<16; i++){
        if (m_node[i]){
            del = false;
        }
    }
    return _data;
}

void* HTrie::_GetDel(const char* ptr, unsigned int len, bool & del)
{
    if (len==0){
        for (int i=0; i<16; i++){
            if (m_node[i]){
                del = false;
            }
        }
        return m_data;
    }
    unsigned char c = (unsigned char)ptr[0];
    if (!m_node[Pe16[c][0]]){
        for (int i=0; i<16; i++){
            if (m_node[i]){
                del = false;
            }
        }
        return 0;
    }
    bool _del = true;
    void * _data = m_node[Pe16[c][0]]->_GetDel1(ptr, len, _del, Pe16[c][1]);
    if (_del){
        delete m_node[Pe16[c][0]];
        m_node[Pe16[c][0]] = 0;
    }
    for (int i=0; i<16; i++){
        if (m_node[i]){
            del = false;
        }
    }
    return _data;
}

void* HTrie::GetDel(const char* ptr, unsigned int len)
{
    if (len==0){
        return m_data;
    }
    unsigned char c = (unsigned char)ptr[0];
    if (!m_node[Pe16[c][0]]){
        return 0;
    }
    bool _del = true;
    void *_data = m_node[Pe16[c][0]]->_GetDel1(ptr, len, _del, Pe16[c][1]);
    if (_del){
        delete m_node[Pe16[c][0]];
        m_node[Pe16[c][0]] = 0;
    }
    return _data;
}

void* HTrie::_GetDel(bool & del)
{
    for (int i=0; i <16; i++)
    {
        if (m_node[i]){
            bool _del = false;
            void * _data = m_node[i]->_GetDel(_del);
            if (_del){
                delete m_node[i];
                m_node[i]=0;
            }
            return _data;
        }
    }
    del = true;
    return m_data;
}

void* HTrie::GetDel()
{
    for (int i=0; i <16; i++)
    {
        if (m_node[i]){
            bool _del = false;
            void * _data = m_node[i]->_GetDel(_del);
            if (_del){
                delete m_node[i];
                m_node[i]=0;
            }
            return _data;
        }
    }
    return m_data;
}
