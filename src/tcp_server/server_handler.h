#ifndef __SERVER_HANDLER_H
#define __SERVER_HANDLER_H
#include "amqp_handler.h"

class ServerConfig
{
public:
    ServerConfig(){};
    ~ServerConfig(){};
    AmqpConfig m_amqp;
};

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
    NetConn* Add(uint64_t id, int fd){
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

class NetServerOp
{
public:
    NetServerOp(int op, uint64_t id, int fd, bool first, HNetBuff* buff){
        m_op = op;
        m_id = id;
        m_fd = fd;
        m_first = first;
        //m_buff.swap(buff);
    };
    virtual ~NetServerOp(){
    };
    HNetBuff  m_buff;
    uint64_t  m_id;
    int       m_fd;
    bool      m_first;
    int       m_op;
};

class NetServerHandler :public AsyncNetCallBack, public AsyncAmqpCallBack
{
public:
    NetServerHandler(){
    };
    virtual ~NetServerHandler(){
    };
    int Start(ServerConfig* config){
        //m_net.StartAcceptServer(this, ip, port, max_net);
        return 0;
    };
    virtual size_t ReadMsgOk(const char* buff, size_t len, uint64_t id, int fd){
        /*NetConn* n = m_conns.Get(id);
        NetServerOp * op
	    if (!n){
	        n = m_conns.Add(id, fd);
	    }
	    SendMsgToBack(buff, n, first);*/
        return 0;
    };
    virtual void CloseConn(uint64_t id, int fd){
	    m_conns.Del(id, fd);
    };
    virtual void OnMessage(AmqpConn* ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        //do back message
    };
private:
    AmqpHandler      m_mq;     //mq
    AsyncNet  	     m_net;    //tcp
    NetConnHandler   m_conns;  
};

#endif

