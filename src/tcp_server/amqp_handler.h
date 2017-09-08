#ifndef __AMQP_HANDLER_H
#define __AMQP_HANDLER_H

#include "async_net.h"
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

class AmqpConfig
{
public:
    AmqpConfig(){};
    ~AmqpConfig(){};
    std::vector<AMQP_CONFIG*> m_conf;
};

class AmqpConn;

class AmqpCallBack
{
public:
    AmqpCallBack(){};
    virtual ~AmqpCallBack(){};
    virtual void SendData(std::string& msg, uint64_t id, int fd){};
    virtual void OnConnError(AMQP::Connection * c){};
    virtual void OnChannelReady(AMQP::Channel* c){};
    virtual void OnMessage(AmqpConn* ptr, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){};
};

class AmqpChannelBack
{
public:
    AmqpChannelBack(){};
    virtual ~AmqpChannelBack(){};
    virtual void OnMessage(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){};
};

class AmqpChannel
{
public:
    AmqpChannel(AMQP::Connection * c, AMQP_QUEUE* queue, AmqpChannelBack * back, const std::string & ex){
        m_ex = ex;
        m_back = back;
        m_queue = queue;
        m_ch = new AMQP::Channel(c);
        m_ch->onError([this](const char *message) {
            ChannelError(message);
        });
        m_ch->onReady([this]() {
            ChannelReady();
        });
    };
    void ChannelError(const char *message){
        LOG(ERROR)<<"ChannelError :"<<message;
    };
    void ChannelReady(){
        m_ch->declareExchange(m_ex).onSuccess([this]() {
            LOG(INFO)<<"declareExchange ok :"<<m_ex;
        });
        m_ch->declareQueue(m_queue->m_queue).onSuccess([this]() {
            m_ch->consume(m_queue->m_queue).onReceived([this](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {
                if (m_back){
                    m_back->OnMessage(message, deliveryTag, redelivered);
                }
            });
        });
        for (size_t i=0; i<m_queue->m_bind_keys.size(); i++){
            m_ch->bindQueue(m_ex, m_queue->m_queue, m_queue->m_bind_keys[i]).onSuccess([this]() {
                LOG(INFO)<<"bindQueue ok:"<<m_queue->m_queue;
            });
        }
    };
    ~AmqpChannel(){
        delete m_ch;
    };
    AmqpChannelBack*   m_back;
    AMQP::Channel*  m_ch;
    AMQP_QUEUE*     m_queue;
    std::string     m_ex;
};

class AmqpConn :public AMQP::ConnectionHandler, public AmqpChannelBack
{
public:
    AmqpConn(AmqpCallBack *p, AMQP_CONFIG* conf, int fd, uint64_t id){
        m_back = p;
        m_fd = fd;
        m_id = id;
        m_connection = new AMQP::Connection(this, AMQP::Login(conf->m_username.c_str(), conf->m_passwd.c_str()), conf->m_vhost.c_str());
        for (size_t i=0; i<conf->m_queue.size(); i++){
            AmqpChannel * channel = new AmqpChannel(m_connection, conf->m_queue[i], this, conf->m_ex);
            m_channel.push_back(channel);
        }
    };
    ~AmqpConn(){
        for (size_t i=0; i<m_channel.size(); i++){
            delete m_channel[i];
        }
        delete m_connection;
    };
    virtual void OnMessage(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        if (m_back){
            m_back->OnMessage(this, message, deliveryTag, redelivered);
        }
    };
    virtual void onData(AMQP::Connection* connection, const char* data, size_t len){
        LOG(INFO)<<"onData:"<<connection<<" size:"<<len;
        if (m_back){
            std::string msg(data, len);
            m_back->SendData(msg, m_id, m_fd);
        }
    };
    virtual void onConnected(AMQP::Connection* connection){
        LOG(INFO)<<"onConnected:"<<connection;
    };
    virtual void onError(AMQP::Connection* connection,
                             const char* message){
        LOG(ERROR)<<"onError:"<<connection<<" "<<message;
    };
    virtual void onClose(AMQP::Connection* connection){
        LOG(ERROR)<<"onClose:"<<connection;
    };
    uint64_t         m_id;
    int              m_fd;
    AMQP::Connection  *m_connection;
    std::vector<AmqpChannel*> m_channel;
    AmqpCallBack*    m_back;
    AMQP_CONFIG*     m_cfg;
};

class AmqpHandler :public AsyncNetCallBack, public AmqpCallBack
{
public:
    AmqpHandler(AmqpConfig* cfg){
        m_config = cfg;
    };
    virtual ~AmqpHandler(){
        
    };
    int Login() {
        return Login(m_config);
    }
    int Login(AmqpConfig* cfg){
        if (!cfg){
            return -1;
        }
        for (size_t i=0; i< cfg->m_conf.size(); i++){
            if (Login(cfg->m_conf[i])<0){
                return -1;
            }
        }
        return 0;
    };
    int Login(AMQP_CONFIG* conf) {
        if (!conf){
            return -1;
        }
        if (m_handler.ConnectServer(this, conf->m_ip.c_str(), conf->m_port)<0){
            return -1;
        }
        AmqpConn * conn = new AmqpConn(this, conf, m_fd, m_id);
        m_mmp[m_id] = conn;
        return 0;
    };
private:
    virtual void OnConnect(uint64_t id, int fd){
        m_id = id;
        m_fd = fd;
    };
    virtual size_t ReadMsgOk(const char* buff, size_t len, uint64_t id, int fd){
        std::map<uint64_t, AmqpConn*>::iterator it = m_mmp.find(id);
        if (it != m_mmp.end()){
            LOG(INFO)<<"Connection:"<<it->second->m_connection<<" parse:"<<len<<" fd:"<<fd<<" id:"<<id;
            return it->second->m_connection->parse(buff, len);
        }else{
            LOG(ERROR)<<"ReadMsgOk close:"<<fd;
            m_handler.CloseFd(id, fd);
        }
        return 0;
    };
    virtual void CloseConn(uint64_t id, int fd){
        LOG(ERROR)<<"CloseConn fd:"<<fd;
        close(fd);
        m_id = 0;
        m_fd = -1;
    };
    virtual void SendData(std::string & msg, uint64_t id, int fd){
        LOG(INFO)<<"SendData fd:"<<fd<<" id:"<<id<<" size:"<<msg.length();
        m_handler.SendMsg(msg, id, fd);
    };
    virtual void OnMessage(AmqpConn* ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        LOG(INFO)<<"OnMessage:"<<ch<<" size:"<<message.message().size()<<" tag:"<<deliveryTag<<" redelivered:"<<redelivered<<" "<<message.message();
    };
    AsyncNet         m_handler;
    AmqpConfig*      m_config;
    std::map<uint64_t, AmqpConn*>  m_mmp;
    uint64_t         m_id;
    int              m_fd;
};
#endif

