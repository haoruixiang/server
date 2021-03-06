#ifndef __SERVER_HANDLER_H
#define __SERVER_HANDLER_H
#include "amqp_handler.h"
#include "shmq_handler.h"

class NetAddr
{
public:
    std::string ip;
    int port;
};
class ServerConfig
{
public:
    ServerConfig(){};
    ~ServerConfig(){};
    AmqpConfig m_amqp;
    std::vector<NetAddr> m_addrs;
    int     m_io_threads;
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
    virtual size_t OnMessage(uint32_t tid, AsyncConn* conn, const char* buff, size_t len){
        /*NetConn* n = m_conns.Get(id);
        NetServerOp * op
	    if (!n){
	        n = m_conns.Add(id, fd);
	    }
	    SendMsgToBack(buff, n, first);*/
        return 0;
    };
    virtual void OnConnect(uint32_t tid, AsyncConn* conn){
        //new act, add 
    };
    virtual void CloseConn(uint32_t tid, AsyncConn * conn){
        DelSession(conn->GetFd(), conn->GetId());
    };
    virtual void OnMessage(AmqpConn* ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        //do back message
    };
    virtual void OnTimeOut(){
    };
private:
    void PushMessage(uint32_t uid, const char* buff, size_t len){
        std::map<uint64_t, int> router;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::map<uint32_t, std::map<uint64_t, int>>::iterator iter = m_sessions.find(uid);
            if (iter != m_sessions.end()){
                std::map<uint64_t, int>::iterator it = iter->second.begin();
                for (; it!=iter->second.end(); it++){
                    router[it->first] = it->second;
                }
            }   
        }
        std::map<uint64_t, int>::iterator it = router.begin();
        for (; it != router.end(); it++){
            SendToClientLoop(it->second, it->first, buff, len);
        }
    };
    void DelSession(int fd, uint64_t id){
        
    };
    void SendToClientLoop(int fd, uint64_t id, const char* buff, size_t len){
        //
    };
    AmqpHandler      m_mq;     //mq
    AsyncNet  	     m_net;    //tcp
    ShmqHandler      m_shmq;
    std::mutex       m_mutex;
    std::map<uint32_t, std::map<uint64_t, int>> m_sessions;
};

#endif

