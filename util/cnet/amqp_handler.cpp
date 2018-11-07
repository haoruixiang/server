#include "amqp_handler.h"
#include <glog/logging.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

AmqpConnCallBack::AmqpConnCallBack()
{
}
AmqpConnCallBack::~AmqpConnCallBack()
{
}

AmqpChannel::AmqpChannel(AMQP::Connection * c,
                        AMQP_QUEUE* queue,
                        AmqpChannelBack * back,
                        const std::string & ex, int flage, int index):m_back(back),
        m_ch(0),
        m_queue(queue),
        m_ex(ex),
        m_flage(flage),
        m_index(index)
{
    m_ch = new AMQP::Channel(c);
    m_ch->onError([this](const char *message) {
        Error(message);
    });
    m_ch->onReady([this]() {
        Ready();
    });
}

AmqpChannel::~AmqpChannel()
{
    if (m_ch){
        delete m_ch;
        m_ch = 0;
    }
}

void AmqpChannel::Error(const char* message)
{
    if (m_back){
        m_back->OnError(message);
    }
}

void AmqpChannel::Ready()
{
    m_ch->declareExchange(m_ex, (AMQP::ExchangeType)2, m_flage).onSuccess([this]() {
    });
    m_ch->declareQueue(m_queue->m_queue, 0x1).onSuccess([this]() {
        m_ch->consume(m_queue->m_queue, m_flage).onReceived([this](const AMQP::Message &message,
                    uint64_t deliveryTag, bool redelivered) {
            if (m_back){
                m_back->OnMessage(m_index, message, deliveryTag, redelivered);
            }
        });
    });
    for (size_t i=0; i<m_queue->m_bind_keys.size(); i++){
        m_ch->bindQueue(m_ex, m_queue->m_queue, m_queue->m_bind_keys[i]).onSuccess([this]() {
        });
    }
}

bool AmqpChannel::ack(uint64_t deliveryTag)
{
    if (m_ch){
        return m_ch->ack(deliveryTag);
    }
    return false;
}

bool AmqpChannel::Publish(const std::string &exchange, const std::string &routingKey, const std::string &message)
{
    if (m_ch){
        return m_ch->publish(exchange, routingKey, message);
    }
    return false;
}

bool AmqpChannel::Publish(const std::string &exchange, const std::string &routingKey, const AMQP::Message &message)
{
    if (m_ch){
        return m_ch->publish(exchange, routingKey, message);
    }
    return false;
}

AmqpConn::AmqpConn(AmqpConnCallBack *p, AMQP_CONFIG* conf, uint64_t id):
    m_back(p),
    m_conf(conf),
    m_id(id),
    m_connection(0)
{

}

AmqpConn::AmqpConn():m_back(0),m_conf(0),m_id(0),m_connection(0)
{
}

AmqpConn::~AmqpConn()
{
    if (m_connection){
        LOG(WARNING)<<"AmqpConn["<<m_id<<"] exit now";
        Close();
    }
}

void AmqpConn::add(AmqpConnCallBack *p, AMQP_CONFIG* conf, uint64_t id)
{
    m_back=p;
    m_conf=conf;
    m_id = id;
}

void AmqpConn::Close()
{
    for (size_t i=0; i<m_channel.size(); i++){
        delete m_channel[i];
    }
    if (m_connection){
        m_connection->close();
        delete m_connection;
    }
    m_connection = 0;
    m_channel.clear();
}

bool AmqpConn::State()
{
    return m_connection == 0 ? false: true;
}

void AmqpConn::Init(int flage)
{
    m_connection = new AMQP::Connection(this, 
            AMQP::Login(m_conf->m_username.c_str(), 
            m_conf->m_passwd.c_str()),
            m_conf->m_vhost.c_str());
    for (size_t i=0; i<m_conf->m_queue.size(); i++){
        AmqpChannel * channel = new AmqpChannel(m_connection, m_conf->m_queue[i], this, m_conf->m_ex, flage, i);
        m_channel.push_back(channel);
    }
}

int  AmqpConn::Parse(const char* buff, int len)
{
    m_read_time = time(0);
    return m_connection->parse(buff, len);
}

void AmqpConn::onConnected(AMQP::Connection* connection)
{
    m_read_time = time(0);
}

void AmqpConn::onError(AMQP::Connection* connection, const char* message)
{
    LOG(ERROR)<<message;
    if (m_back){
        m_back->AmqpOnConnError(m_id);
    }
}

void AmqpConn::OnError(const char* message)
{
    LOG(ERROR)<<"OnError:"<<message;
}

void AmqpConn::onClose(AMQP::Connection* connection)
{
    if (m_back){
        m_back->AmqpOnConnError(m_id);
    }
}

uint16_t AmqpConn::onNegotiate(AMQP::Connection *connection, uint16_t interval)
{
    return 5;
}

void AmqpConn::onHeartbeat(AMQP::Connection *connection)
{
    connection->heartbeat();
}

void AmqpConn::OnMessage(int ch, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
{
    if (m_back){
        m_back->AmqpOnMessage(m_id, ch, message, deliveryTag, redelivered);
    }
}

void AmqpConn::onData(AMQP::Connection* connection, const char* data, size_t len)
{
    if (m_back){
        std::string msg(data, len);
        m_back->AmqpSendData(msg, m_id);
    }
}

bool AmqpConn::ack(int ch, uint64_t deliveryTag)
{
    if (0 <= ch && (unsigned int)ch < m_channel.size()){
        return m_channel[ch]->ack(deliveryTag);
    }
    return false;
}

bool AmqpConn::Publish(int ch, const std::string &exchange, const std::string &routingKey, const std::string &message)
{
    if (0 <= ch && (unsigned int)ch < m_channel.size()){
        return m_channel[ch]->Publish(exchange, routingKey, message);
    }else{
        for (unsigned int i=0; i < m_channel.size(); i++){
            if (m_channel[i]->Publish(exchange, routingKey, message)){
                return true;
            }
        }
        return false;
    }
    return true;
}

bool AmqpConn::Publish(int ch, const std::string &exchange, const std::string &routingKey, const AMQP::Message &message)
{
    if (0 <= ch && (unsigned int)ch < m_channel.size()){
        return m_channel[ch]->Publish(exchange, routingKey, message);
    }else{
        for (unsigned int i=0; i < m_channel.size(); i++){
            if (m_channel[i]->Publish(exchange, routingKey, message)){
                return true;
            }
        }
        return false;
    }
    return true;
}
