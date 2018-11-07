#ifndef  _H_NET_MESSAGE_H
#define  _H_NET_MESSAGE_H

#include "net_connect.h"
#include "notify_queue.h"
#include "hdc_trie.h"
#include <map>

class NetMessageCallBack
{
public:
    NetMessageCallBack();
    ~NetMessageCallBack();
    virtual void OnTimeByInterval(int tid){};
    virtual void OnMessage(int tid, NetConnect* conn){};
    virtual void OnClose(int tid, NetConnect* conn){};
    virtual void OnConnect(int tid, NetConnect* conn){};
    virtual void OnAccept(int fd, uint64_t id){};
    virtual void OnAsyncMessage(int tid, const std::string &msg){};
    virtual void OnAsyncMessage(int tid, uint32_t cmd, uint32_t sqid, uint64_t id, const std::string & msg){};
    virtual void OnPublish(uint64_t id, int ch, const std::string &exchange, const std::string &key, const std::string &message){};
};

class NetManager : public NotifyQueue, 
                   public NetConnectCallBack
{
public:
    NetManager():m_back(0),m_head(0){};
    NetManager(NetMessageCallBack* back):m_back(back),m_head(0){};
    virtual ~NetManager();
    void setBack(NetMessageCallBack* back){
        m_back = back;
    };
    bool Send(uint64_t id, std::string &msg);
    bool AddFd(int fd, uint64_t id);
    bool AddAccept(int fd, uint64_t id);
    bool Close(uint64_t id);
    bool AsyncMessage(uint64_t id, std::string &msg);
    bool AsyncMessage(uint32_t cmd, uint32_t sqid, uint64_t id, std::string & msg);
    bool Publish(uint64_t id, int ch, const std::string &exchange, const std::string &key, std::string &message);
    void* GetContext(uint64_t id);
    void doAddFd(int fd, uint64_t id);
    void doAddAccept(int fd, uint64_t id);
    void doClose(uint64_t id);
    void doSend(uint64_t id, std::string &msg);
    void doAsyncMessage(uint64_t id, std::string & msg);
    void doAsyncMessage(uint32_t cmd, uint32_t sqid, uint64_t id, std::string & msg);
    void doPublish(uint64_t id, int ch, const std::string &exchange, const std::string &key, std::string &message);
    virtual void OnTimeOut(){
        if (m_back){
            m_back->OnTimeByInterval(Sid());
        }
    };
private:
    virtual void OnRecv(NetConnect* conn);
    virtual void OnCloseConn(NetConnect* conn);
    virtual void OnConnect(int fd, uint64_t id);
    NetMessageCallBack * m_back;
    NetConnect*   m_head;
    HTrie         m_conns;
};

class NetDispatcher
{
public:
    NetDispatcher(int num, NetMessageCallBack* back, int interval, int sid)
    {
        m_procs = new NetManager[num];
        for (int i=0; i<num; i++){
            m_procs[i].setBack(back);
            m_procs[i].Start(interval,sid+i);
        }
        m_num = num;
    }
    virtual ~NetDispatcher();
    bool AddFd(int fd, uint64_t id){
        return m_procs[id%m_num].AddFd(fd, id);
    }
    bool Close(uint64_t id){
        return m_procs[id%m_num].Close(id);
    }
    void doClose(uint64_t id){
        m_procs[id%m_num].doClose(id);
    };
    void doSend(uint64_t id, std::string &msg){
        m_procs[id%m_num].doSend(id, msg);
    }
    bool Send(uint64_t id, std::string &msg){
        return m_procs[id%m_num].Send(id, msg);
    }
    bool AsyncMessage(uint64_t id, std::string &msg){
        return m_procs[id%m_num].AsyncMessage(id, msg);
    }
    bool AsyncMessage(uint32_t cmd, uint32_t sqid, uint64_t id, std::string & msg){
        return m_procs[id%m_num].AsyncMessage(cmd, sqid, id, msg);
    }
    bool BroadcastMessage(const std::string & msg){
        for (int i=0; i<m_num; i++){
            std::string body(msg);
            if (!m_procs[i].AsyncMessage(0, body)){
                return false;
            }
        }
        return true;
    }
    void* GetContext(uint64_t id){
        return m_procs[id%m_num].GetContext(id);
    }
private:
    NetManager*  m_procs;
    int          m_num;
};

#endif
