#ifndef  _H_AMQP_HANDLER_H
#define  _H_AMQP_HANDLER_H

#include "net_dispatcher.h"

#include <amqpcpp.h>

typedef struct {
    std::string      m_queue;
    std::vector<std::string> m_bind_keys;
}AMQP_QUEUE;

typedef struct {
    std::string      m_ip;
    int              m_port;
    std::string      m_username;
    std::string      m_passwd;
    std::string      m_vhost;
    std::string      m_ex;
    std::vector<AMQP_QUEUE*> m_queue;
}AMQP_CONFIG;

class AmqpConnCallBack
{
public:
    virtual void SendData(std::string& msg, uint64_t id);
    virtual void OnConnError(uint64_t id);
    virtual void OnChannelReady(AMQP::Channel* c);
    virtual void OnMessage(uint64_t id, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered);
};

class AmqpConn :public AMQP::ConnectionHandler
{
public:
    AmqpConn(AmqpConnCallBack *p, AMQP_CONFIG* conf, uint64_t id);
    virtual ~AmqpConn();
    void  Clear();
    void  Close();
    void  Init(int flage = 0);
    int   Parse(const char* buff, int len);
private:
    virtual uint16_t onNegotiate(AMQP::Connection *connection, uint16_t interval){
        return 5;
    }
    virtual void onHeartbeat(AMQP::Connection *connection) {
        connection->heartbeat();
    };
    virtual void OnMessage(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        if (m_back){
            m_back->OnMessage(m_id, message, deliveryTag, redelivered);
        }
    };
    virtual void onData(AMQP::Connection* connection, const char* data, size_t len){
        if (m_back){
            std::string msg(data, len);
            m_back->SendData(msg, m_id);
        }
    };
    virtual void onConnected(AMQP::Connection* connection){
        m_read_time = time(0);
    };
    virtual void onError(AMQP::Connection* connection, const char* message){
        if (m_back){
            m_back->OnConnError(m_id);
        }
    };
    virtual void onClose(AMQP::Connection* connection){
        if (m_back){
            m_back->OnConnError(m_id);
        }
    };
    uint64_t         m_id;
    AMQP::Connection  *m_connection;
    //std::vector<AmqpChannel*> m_channel;
    AmqpConnCallBack*    m_back;
    AMQP_CONFIG*     m_conf;
    time_t           m_read_time;
    time_t           m_connect_time;
};

class AmqpHandler : public NetMessageCallBack,
                    public AmqpConnCallBack
{
public:
    AmqpHandler():m_manger(this){
    };
    ~AmqpHandler();
    void Start();
    int  AddAmqp(AMQP_CONFIG* conf);
private:
    virtual void SendData(std::string& msg, uint64_t id);
    virtual void OnConnError(uint64_t id);
    virtual void OnChannelReady(AMQP::Channel* c);
    virtual void OnMessage(uint64_t id, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered);
private:
    virtual void OnTimeByInterval();
    virtual void OnMessage(NetConnect* conn);
    virtual void OnClose(NetConnect* conn);
    virtual void OnConnect(NetConnect* conn);
private:
    NetManager  m_manger;
    uint64_t    m_id;
    std::map<uint64_t, AmqpConn*> m_conns;
};

#endif

