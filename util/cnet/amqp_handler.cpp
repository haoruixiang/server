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

int AmqpHandler::AddAmqp(AMQP_CONFIG* conf)
{
    uint64_t id = m_id++;
    sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(conf->m_port);
    addr.sin_addr.s_addr = inet_addr(conf->m_ip.c_str());
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(fd, (const sockaddr*)&addr, (socklen_t)(sizeof(struct sockaddr))) !=0){
        close(fd);
        return -1;
    }
    if (init_fd(fd) != 0){
        close(fd);
        return -1;
    }
    m_manger.AddFd(fd, id);
    m_conns[id] = new AmqpConn(this, conf, id);
    return 0;
}

void AmqpHandler::OnMessage(NetConnect* conn)
{
    std::map<uint64_t, AmqpConn*>::iterator it = m_conns.find(conn->Id());
    if (it != m_conns.end())
    {
        int rt = it->second->Parse(conn->ReadPeek(), conn->ReadLen());
        if (rt>0)
        {
            conn->ReadRetrieve(rt);
        }else{
            m_manger.doClose(conn->Id());
        }
    }else{
        LOG(ERROR)<<"OnMessage not fount id:"<< conn->Id();
        m_manger.doClose(conn->Id());
    }
}

void AmqpHandler::SendData(std::string& msg, uint64_t id)
{
    m_manger.doSend(id, msg);
}

void AmqpHandler::OnConnError(uint64_t id)
{
    m_manger.doClose(id);
}

void AmqpHandler::OnMessage(uint64_t id, const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
{
    
}

AmqpConn::AmqpConn(AmqpConnCallBack *p, AMQP_CONFIG* conf, uint64_t id):
    m_back(p),
    m_conf(conf),
    m_id(id)
{

}

AmqpConn::~AmqpConn()
{
    Close();    
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

void AmqpConn::Init(int flage = 0)
{
    Clear();
    m_connection = new AMQP::Connection(this, 
            AMQP::Login(m_conf->m_username.c_str(), 
            m_conf->m_passwd.c_str()),
            m_conf->m_vhost.c_str());
    for (size_t i=0; i<m_conf->m_queue.size(); i++){
        AmqpChannel * channel = new AmqpChannel(m_connection,
            m_conf->m_queue[i], this, m_conf->m_ex, flage);
        m_channel.push_back(channel);
    }
}

int  AmqpConn::Parse(const char* buff, int len)
{
    return m_connection->parse(buff, len);
}

