#ifndef  _H_NET_CONNECT_H
#define  _H_NET_CONNECT_H
#include "netbuff.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ev.h>

class NetConnect;

class NetConnectCallBack
{
public:
    NetConnectCallBack(){};
    virtual ~NetConnectCallBack(){};
    virtual void OnRecv(NetConnect* conn){};
    virtual void OnCloseConn(NetConnect* conn){};
    virtual void OnConnect(int fd, uint64_t id){};
};

int init_fd(int fd);

class NetConnect
{
public:
    NetConnect(struct ev_loop* loop,uint64_t id, int fd, NetConnectCallBack* back):
        m_send_state(0),
        m_back(back),
        m_id(id),
        m_fd(fd),
        m_loop(loop)
    {
    };
    virtual ~NetConnect();
    void StartAccept();
    void AddSend(const std::string & msg);
    void StartRecv();
    uint64_t Id(){
        return m_id;
    };
    const char * ReadPeek(){
        return m_read_buff.Peek();
    }
    size_t ReadLen(){
        return m_read_buff.ReadableBytes();
    }
    void  ReadRetrieve(size_t len){
        m_read_buff.Retrieve(len);
    }
private:
    void Read();
    void Write();
    void Accept();
    static void ReadCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        NetConnect * p = (NetConnect*)(ev->data);
        if (p){
            p->Read();
        }
    };
    static void WriteCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        NetConnect * p = (NetConnect*)(ev->data);
        if (p){
            p->Write();
        }
    };
    static void AcceptCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        NetConnect * p = (NetConnect*)(ev->data);
        if (p){
            p->Accept();
        }
    };
private:
    uint8_t             m_send_state;
    NetConnectCallBack  *m_back;
    uint64_t            m_id;
    int                 m_fd;
    struct ev_loop*     m_loop;
    HNetBuff    m_read_buff;
    HNetBuff    m_send_buff;
    ev_io       m_rwatcher;
    ev_io       m_swatcher;
};

#endif
