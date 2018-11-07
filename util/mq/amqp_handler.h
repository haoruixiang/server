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
    AmqpConnCallBack();
    ~AmqpConnCallBack();
    virtual void AmqpSendData(std::string& msg, uint64_t id){};
    virtual void AmqpOnConnError(uint64_t id){};
    virtual void AmqpOnChannelReady(AMQP::Channel* c){};
    virtual void AmqpOnMessage(uint64_t id, int ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){};
};

class AmqpChannelBack
{
public:
    AmqpChannelBack(){};
    virtual ~AmqpChannelBack(){};
    virtual void OnMessage(int ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){};
    virtual void OnError(const char* message){};
};

class AmqpChannel
{
public:
    AmqpChannel(AMQP::Connection * c, AMQP_QUEUE* queue, AmqpChannelBack * back, const std::string & ex, int flage = 0, int index = 0);
    virtual ~AmqpChannel();
    void Init();
    void Error(const char* message);
    void Ready();
    bool ack(uint64_t deliveryTag);
    bool Publish(const std::string &exchange, const std::string &routingKey, const std::string &message);
    bool Publish(const std::string &exchange, const std::string &routingKey, const AMQP::Message &message);
    AmqpChannelBack* m_back;
    AMQP::Channel*   m_ch;
    AMQP_QUEUE*      m_queue;
    std::string      m_ex;
    int              m_flage;
    int              m_index;
};

class AmqpConn :public AMQP::ConnectionHandler,
                public AmqpChannelBack
{
public:
    AmqpConn(AmqpConnCallBack *p, AMQP_CONFIG* conf, uint64_t id);
    AmqpConn();
    virtual ~AmqpConn();
    void  add(AmqpConnCallBack *p, AMQP_CONFIG* conf, uint64_t id);
    void  Close();
    void  Init(int flage = 0);
    int   Parse(const char* buff, int len);
    bool  State();
    bool  ack(int ch, uint64_t deliveryTag);
    bool  Publish(int ch, const std::string &exchange, const std::string &routingKey, const std::string &message);
    bool  Publish(int ch, const std::string &exchange, const std::string &routingKey, const AMQP::Message &message);
    uint64_t Id(){return m_id;};
    bool  IsTimeOut(int tv){
        if (m_connection && time(0) - m_read_time > tv){
            return true;
        }
        return false;
    };
    AMQP_CONFIG* getConf(){
        return m_conf;
    };
private:
    virtual uint16_t onNegotiate(AMQP::Connection *connection, uint16_t interval);
    virtual void onHeartbeat(AMQP::Connection *connection);
    virtual void OnMessage(int ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered);
    virtual void OnError(const char* message);
    virtual void onData(AMQP::Connection* connection, const char* data, size_t len);
    virtual void onConnected(AMQP::Connection* connection);
    virtual void onError(AMQP::Connection* connection, const char* message);
    virtual void onClose(AMQP::Connection* connection);
    
    AmqpConnCallBack* m_back;
    AMQP_CONFIG*      m_conf;
    uint64_t          m_id;
    AMQP::Connection  *m_connection;
    std::vector<AmqpChannel*> m_channel;
    time_t            m_read_time;
    time_t            m_connect_time;
    std::mutex        m_lock;
};

#endif

