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
    virtual void OnConnError(AmqpConn * c){};
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
    AmqpChannel(AMQP::Connection * c, AMQP_QUEUE* queue, AmqpChannelBack * back, const std::string & ex, int flage = 0){
        m_ex = ex;
        m_back = back;
        m_queue = queue;
        m_flage = flage;
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
        });
        m_ch->declareQueue(m_queue->m_queue).onSuccess([this]() {
            m_ch->consume(m_queue->m_queue, m_flage).onReceived([this](const AMQP::Message &message, 
                uint64_t deliveryTag, bool redelivered) {
                if (m_back){
                    m_back->OnMessage(message, deliveryTag, redelivered);
                }
            });
        });
        for (size_t i=0; i<m_queue->m_bind_keys.size(); i++){
            m_ch->bindQueue(m_ex, m_queue->m_queue, m_queue->m_bind_keys[i]).onSuccess([this]() {
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
    int             m_flage;
};

class AmqpConn :public AMQP::ConnectionHandler, public AmqpChannelBack
{
public:
    AmqpConn(AmqpCallBack *p, AMQP_CONFIG* conf, int fd, uint64_t id, int flage){
        m_connection = 0;
        m_back = p;
        m_conf = conf;
        m_connect_time = time(0);
        Init(fd, id, flage);
    };
    ~AmqpConn(){
        Clear();
    };
    void Clear(){
        for (size_t i=0; i<m_channel.size(); i++){
            delete m_channel[i];
        }
        if (m_connection){
            delete m_connection;
        }
        m_connection = 0;
        m_channel.clear();
    };
    void Init(int fd, uint64_t id, int flage = 0){
        Clear();
        m_fd = fd;
        m_id = id;
        m_connection = new AMQP::Connection(this, AMQP::Login(m_conf->m_username.c_str(), m_conf->m_passwd.c_str()), m_conf->m_vhost.c_str());
        for (size_t i=0; i<m_conf->m_queue.size(); i++){
            AmqpChannel * channel = new AmqpChannel(m_connection, m_conf->m_queue[i], this, m_conf->m_ex, flage);
            m_channel.push_back(channel);
        }
    };
    virtual uint16_t onNegotiate(AMQP::Connection *connection, uint16_t interval){
        return 5;
    }
    virtual void onHeartbeat(AMQP::Connection *connection) {
        connection->heartbeat();
    };
    virtual void OnMessage(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        if (m_back){
            m_back->OnMessage(this, message, deliveryTag, redelivered);
        }
    };
    virtual void onData(AMQP::Connection* connection, const char* data, size_t len){
        if (m_back){
            std::string msg(data, len);
            m_back->SendData(msg, m_id, m_fd);
        }
    };
    virtual void onConnected(AMQP::Connection* connection){
    };
    virtual void onError(AMQP::Connection* connection, const char* message){
        LOG(ERROR)<<"onError:"<<connection<<" "<<message;
        if (m_back){
            m_back->OnConnError(this);
        }
    };
    virtual void onClose(AMQP::Connection* connection){
        LOG(ERROR)<<"onClose:"<<connection;
        if (m_back){
            m_back->OnConnError(this);
        }
    };
    void Close(){
        for (size_t i=0; i<m_channel.size(); i++){
            delete m_channel[i];
        }
        if (m_connection){
            m_connection->close();
            delete m_connection;
        }
        m_connection = 0;
        m_channel.clear();
    };
    uint64_t         m_id;
    int              m_fd;
    AMQP::Connection  *m_connection;
    std::vector<AmqpChannel*> m_channel;
    AmqpCallBack*    m_back;
    AMQP_CONFIG*     m_conf;
    time_t           m_read_time;
    time_t           m_connect_time;
};

class AsyncAmqpCallBack
{
public:
    AsyncAmqpCallBack(){};
    virtual ~AsyncAmqpCallBack(){};
    virtual void OnMessage(AmqpConn* ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
    };
};

class AmqpHandler :public AsyncNetCallBack, public AmqpCallBack
{
public:
    AmqpHandler(AmqpConfig* cfg = 0){
        m_config = cfg;
    };
    virtual ~AmqpHandler(){
        std::map<uint64_t, AmqpConn*>::iterator it = m_mmp.begin();
        for (;it != m_mmp.end(); it++){
            it->second->Close();
            delete it->second;
        }
        m_mmp.clear();
    };
    int Login(AmqpConfig* cfg, int flage, AsyncAmqpCallBack* back){
        m_back = back;
        m_flage = flage;
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
private:
    int Login(AMQP_CONFIG* conf) {
        if (!conf){
            return -1;
        }
        if (m_handler.ConnectServer(this, conf->m_ip.c_str(), conf->m_port)<0){
            LOG(ERROR)<<"ConnectServer "<<conf->m_ip<<" "<<conf->m_port<<" false";
            return -1;
        }
        AmqpConn * conn = new AmqpConn(this, conf, m_fd, m_id, m_flage);
        m_mmp[m_id] = conn;
        return 0;
    };
    virtual void OnConnect(uint32_t tid, AsyncConn* conn){
        m_id = conn->GetId();
        m_fd = conn->GetFd();
    };
    virtual size_t OnMessage(uint32_t tid, AsyncConn* conn, const char* buff, size_t len){
        LOG(INFO)<<"OnMessage"<<conn<<" len:"<<len<< " id:"<<conn->GetId();
        std::map<uint64_t, AmqpConn*>::iterator it = m_mmp.find(conn->GetId());
        if (it != m_mmp.end()){
            it->second->m_read_time = time(0);
            return it->second->m_connection->parse(buff, len);
        }else{
            LOG(ERROR)<<"OnMessage not fount id:"<< conn->GetId();
            m_handler.CloseFd(conn->GetId(), conn->GetFd());
        }
        return 0;
    };
    virtual void CloseConn(uint32_t tid, AsyncConn* conn){
        LOG(INFO)<<"CloseConn:"<<conn<<" "<<conn->GetFd();
        std::map<uint64_t, AmqpConn*>::iterator ct = m_mmp.find(conn->GetId());
        if (ct != m_mmp.end()){
            m_losts.push_back(ct->second);
            m_mmp.erase(ct);
        }
        close(conn->GetFd());
        m_id = 0;
        m_fd = -1;
    };
    virtual void OnConnError(AmqpConn* ch){
        if (ch){
            LOG(INFO)<<"close:"<<ch->m_id<<" "<<ch->m_fd;
            m_handler.CloseFd(ch->m_id, ch->m_fd);
        }
    };
    virtual void SendData(std::string & msg, uint64_t id, int fd){
        LOG(INFO)<<"SendData:"<<msg.size();
        m_handler.SendMsg(msg, id, fd);
    };
    virtual void OnMessage(AmqpConn* ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered){
        LOG(INFO)<<"OnMessage:"<<ch<<std::string(message.body(),message.bodySize());
        if (m_back){
            m_back->OnMessage(ch, message, deliveryTag, redelivered);
        }
    };
    virtual void OnTimeOut(uint32_t tid){
        time_t t = time(0);
        std::map<uint64_t, AmqpConn*>::iterator it = m_mmp.begin();
        for (;it != m_mmp.end(); it++){
            if (t - it->second->m_read_time > 10){
                LOG(ERROR)<<"time out close fd:"<<it->second->m_fd<<" id:"
                        <<it->second->m_id<<" time:"<<it->second->m_read_time;               
                m_handler.CloseFd(it->second->m_id, it->second->m_fd);
                continue;
            }
        }
        do{
            bool b = true;
            std::list<AmqpConn*>::iterator ct = m_losts.begin();
            for (; ct != m_losts.end(); ct++){
                if (t - (*ct)->m_connect_time >3){
                    (*ct)->m_connect_time = t;
                    if (m_handler.ConnectServer(this, (*ct)->m_conf->m_ip.c_str(), (*ct)->m_conf->m_port)<0){
                        continue;
                    }
                    (*ct)->Init(m_fd, m_id, m_flage);
                    m_mmp[m_id] = (*ct);
                    b = false;
                    m_losts.erase(ct);
                    break;
                }
            }
            if (b){
                break;
            }
        }while(true);
    };
    AsyncNet         m_handler;
    AmqpConfig*      m_config;
    std::map<uint64_t, AmqpConn*>  m_mmp;
    std::list<AmqpConn*>    m_losts;
    uint64_t         m_id;
    int              m_fd;
    int              m_flage;
    AsyncAmqpCallBack * m_back;
};
#endif

