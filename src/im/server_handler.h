#ifndef __SERVER_HANDLER_H
#define __SERVER_HANDLER_H
#include "async_net.h"

class ConnSession
{
public:
    ConnSession(){
        m_status = 0;
        m_uid = 0;
    };
    ~ConnSession(){};
    int Status(){
        return m_status;
    };
    bool IsLogin(){
        return m_status == 1;
    };
    void SetLogin(uint32_t uid){
        m_status = 1;
        m_uid = uid;
    };
    uint32_t Uid(){
        return m_uid;
    };
private:
    int  m_status;
    uint32_t m_uid;
};

enum{
    IM_ERROR,
    IM_HEART,
    IM_MSG_REQ,
    IM_MSG_RSP,
    IM_OP_REQ,
    IM_OP_RSP
};

typedef  struct {
    unsigned int  m_version:8;     //
    unsigned int  m_type:8;        //1:request 2:response 3:heart
    unsigned int  m_identifier:24; //
    unsigned int  m_len:24;        //
}MsgHead;

class NetServerHandler :public AsyncNetCallBack
{
public:
    NetServerHandler(){
    };
    virtual ~NetServerHandler(){
    };
    int Start(const char* ip, int port, int max){
        return m_net.StartAcceptServer(this, ip, port, max);
    };
    virtual size_t OnMessage(uint32_t tid, AsyncConn* conn, const char* buff, size_t len){
        return Process(tid, conn, buff, len);
    };
    virtual void OnConnect(uint32_t tid, AsyncConn* conn){
        ConnSession * session = new ConnSession();
        conn->SetContext(session);
    };
    virtual void CloseConn(uint32_t tid, AsyncConn * conn){
        ConnSession * session = (ConnSession*)conn->GetDelContext();
        if (session->IsLogin()){
            DelSession(session, conn->GetId(), conn->GetFd());
        }
        delete session;
    };
    virtual void OnTimeOut(){
        //on time out
    };
private:
    void MsgReq(const char* buff, int len);
    size_t Process(uint32_t tid, AsyncConn* conn, const char* buff, size_t len);
    void PushMessage(uint32_t uid, const char* buff, size_t len);
    void DelSession(ConnSession* session, uint64_t id, int fd);
    void SetSessionRoute(uint32_t uid, ConnSession* session, uint64_t id, int fd);
    void SendToClientLoop(int fd, uint64_t id, const char* buff, size_t len);
    AsyncNet  	     m_net;    //tcp
    std::mutex       m_mutex;
    std::map<uint32_t, std::map<uint64_t, int>> m_sessions;
};

#endif

