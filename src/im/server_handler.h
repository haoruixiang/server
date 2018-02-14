#ifndef __SERVER_HANDLER_H
#define __SERVER_HANDLER_H
#include "async_net.h"

class ConnSession
{
public:
    ConnSession(){
        m_status = 0;
    };
    ~ConnSession(){};
    int Status(){
        return m_status;
    };
    bool IsLogin(){
        return m_status == 1;
    };
    void SetLogin(){
        m_status = 1;
    }
private:
    int  m_status; 
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
        int rsize = 0;
        int bHeart = 0;
        do{
            if (len< sizeof(MsgHead)){
                break;
            }
            MsgHead * ph = (MsgHead*)buff;
            if (ph->m_type == IM_HEART){
                rsize += sizeof(MsgHead);
                buff += sizeof(MsgHead);
                bHeart = 1;
                len -= sizeof(MsgHead);
                continue;
            }
            if ((len - sizeof(MsgHead)) < ph->m_len){
                LOG(WARNING)<<"recv incomplete pkg slen:"<<len
                    <<" tlen:"<<ph->m_len
                    <<" version:"<<ph->m_version
                    <<" type:"<<ph->m_type
                    <<" identifier:"<<ph->m_identifier;
                break;
            }
            rsize += sizeof(MsgHead);
            rsize += ph->m_len;
            len -= sizeof(MsgHead);
            len -= ph->m_len;
            buff += sizeof(MsgHead);
            switch (ph->m_type){
            case IM_MSG_REQ:
                MsgReq(buff, ph->m_len);
                break;
            case IM_MSG_RSP:
                MsgRsp(buff, ph->m_len);
                break;
            case IM_OP_REQ:
                OpReq(buff, ph->m_len);
                break;
            case IM_OP_RSP:
                OpRsp(buff, ph->m_len);
                break;
            default:
                LOG(ERROR)<<"recv unknown msg";
                //close socket
            }
        }while(true);
        if (bHeart){
            //response heart
        }
        return rsize;
    };
    virtual void OnConnect(uint32_t tid, AsyncConn* conn){
        ConnSession * session = new ConnSession()
        conn->SetContext(session);
    };
    virtual void CloseConn(uint32_t tid, AsyncConn * conn){
        ConnSession * session = (ConnSession*)conn->GetDelContext();
        if (session->IsLogin()){
            DelSession(tid, session);
        }
        delete session;
    };
    virtual void OnTimeOut(){
        //on time out
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

