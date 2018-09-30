#ifndef  __H_NET_SERVER_H
#define  __H_NET_SERVER_H
#include "net_connect.h"
#include "notify_queue.h"
#include "net_dispatcher.h"

class NetServer :public NotifyQueue,
                 public NetConnectCallBack,
                 public NetMessageCallBack
{
public:
    NetServer(int num, int interval);
    virtual ~NetServer(){};
    virtual void OnTimeByInterval();
    virtual void OnMessage(NetConnect* conn);
    virtual void OnClose(NetConnect* conn);
    virtual void OnConnect(NetConnect* conn);
    int     StartServer(const char* ip, int port, int max);
    void    Close(uint64_t id);
    void    Send(uint64_t id, std::string & msg);
private:
    virtual void OnCloseConn(NetConnect* conn);
    virtual void OnConnect(int fd, uint64_t id);
private:
    NetDispatcher                    m_disp;
};

#endif

