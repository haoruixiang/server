#ifndef  _H_NET_MESSAGE_H
#define  _H_NET_MESSAGE_H

#include "net_connect.h"
#include "notify_queue.h"

#include <map>

class NetMessageCallBack
{
public:
    virtual void OnTimeByInterval();
    virtual void OnMessage(NetConnect* conn);
    virtual void OnClose(NetConnect* conn);
    virtual void OnConnect(NetConnect* conn);
};

class NetManager : public NotifyQueue, 
                   public NetConnectCallBack
{
public:
    NetManager():m_back(0){};
    NetManager(NetMessageCallBack* back):m_back(back){};
    ~NetManager(){};
    void setBack(NetMessageCallBack* back){
        m_back = back;
    };
    void Send(uint64_t id, std::string &msg);
    void AddFd(int fd, uint64_t id);
    void Close(uint64_t id);
    
    void doAddFd(int fd, uint64_t id);
    void doClose(uint64_t id);
    void doSend(uint64_t id, std::string &msg);
    virtual void OnTimeOut(){
        if (m_back){
            m_back->OnTimeByInterval();
        }
    };
private:
    virtual void OnRecv(NetConnect* conn);
    virtual void OnCloseConn(NetConnect* conn);

    NetMessageCallBack * m_back;
    std::map<uint64_t, NetConnect*>  m_conns;
};

class NetDispatcher
{
public:
    NetDispatcher(int num, NetMessageCallBack* back, int interval)
    {
        m_procs = new NetManager[num];
        for (int i=0; i<num; i++){
            m_procs[i].setBack(back);
            m_procs[i].Start(interval);
        }
        m_num = num;
    }
    virtual ~NetDispatcher()
    {
        if (m_procs){
            delete [] m_procs;
            m_procs = 0;
        }
    }
    void AddFd(int fd, uint64_t id){
        m_procs[id%m_num].AddFd(fd, id);
    }
    void Close(uint64_t id){
        m_procs[id%m_num].Close(id);
    }
    void Send(uint64_t id, std::string &msg){
        m_procs[id%m_num].Send(id, msg);
    }
private:
    NetManager*  m_procs;
    int          m_num;
};

#endif
