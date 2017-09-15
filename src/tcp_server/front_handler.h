#ifndef __FRONT_HANDLER_H
#define __FRONT_HANDLER_H
#include "async_net.h"


class NetConn
{
public:
    NetConn(uint64_t id, int fd){m_id = id; m_fd = fd;};
    ~NetConn(){};
    uint64_t m_id;
    int m_fd;
};

class NetConnHandler
{
public:
    NetConnHandler(){};
    ~NetConnHandler(){};
    NetConn* Get(uint64_t id){
	std::lock_guard<std::mutex> _lock(m_mtx);
	std::map<uint64_t, NetConn*>::iterator it = m_conns.find(id);
	if (it == m_conns.end()){
	    return 0;
	}
	return it->second;
    };
    NetConn* GetAdd(uint64_t id, int fd){
	NetConn * n = new NetConn(id, fd);
	std::lock_guard<std::mutex> _lock(m_mtx);
	m_conns[id] = n;
	return n;
    };
    void Del(uint64_t id, int fd){
	std::lock_guard<std::mutex> _lock(m_mtx);
	std::map<uint64_t, NetConn*>::iterator it = m_conns.find(id);
        if (it != m_conns.end()){
	    delete it->second;
	    m_conns.erase(it);
        }
    };
    std::map<uint64_t, NetConn*>   m_conns;
    std::mutex          m_mtx;
};

class NetFrontHandler :public AsyncNet
{
public:
    NetFrontHandler(){};
    virtual ~NetFrontHandler(){};
    virtual void ReadMsgOk(HNetBuff* buff, uint64_t id, int fd){
        NetConn* n = m_conns.Get(id);
	    bool first = false;
	    if (!n){
	        n = m_conns.Add(id, fd);
	        first = true; 
	    }
	    SendMsgToBack(buff, n, first);
    };
    virtual void CloseConn(uint64_t id, int fd){
	    m_conns.Del(id, fd);
    };
private:
    NetConnHandler   m_conns;
};

#endif

