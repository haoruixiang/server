#include "net_dispatcher.h"
#include <glog/logging.h>

class NetCommand
{
public:
    NetCommand(int fd, int cmd, uint64_t id):
        m_fd(fd),
        m_cmd(cmd),
        m_id(id)
    {};
    ~NetCommand(){}
    int         m_fd;
    int         m_cmd;
    uint64_t    m_id;
    std::string m_msg;
};

int  OnCommand(void* ptr, void* data)
{
    NetManager * s = (NetManager*)ptr;
    NetCommand* c = (NetCommand*)data;
    if (0 == c->m_cmd){
        s->doAddFd(c->m_fd, c->m_id);
    }
    if (1 == c->m_cmd){
        s->doClose(c->m_id);
    }
    if (2== c->m_cmd){
        s->doSend(c->m_id, c->m_msg);
    }
    delete c;
    return 0;
}

void NetManager::AddFd(int fd, uint64_t id)
{
    NetCommand * c = new NetCommand(fd, 0, id);
    if (Notify(OnCommand, this, c)<0)
    {
        LOG(ERROR)<<"Notify false:"<<id;
    }
}
void NetManager::Close(uint64_t id)
{
    NetCommand * c = new NetCommand(0, 1, id);
    if (Notify(OnCommand, this, c)<0)
    {
        LOG(ERROR)<<"Notify false:"<<id;
    }
}
void NetManager::Send(uint64_t id, std::string & msg)
{
    NetCommand * c = new NetCommand(0, 2, id);
    c->m_msg.swap(msg);
    if (Notify(OnCommand, this, c)<0)
    {
        LOG(ERROR)<<"Notify false:"<<id;
    }
}

void NetManager::doSend(uint64_t id, std::string & msg)
{
    std::map<uint64_t, NetConnect*>::iterator it = m_conns.find(id);
    if (it != m_conns.end()){
        it->second->AddSend(msg);
    }else{
        LOG(ERROR)<<"not find id:"<<id;
    }
}

void NetManager::doAddFd(int fd, uint64_t id)
{
    NetConnect *conn = new NetConnect(Loop(), id, fd, this);
    m_conns[id] = conn;
    if (m_back){
        m_back->OnConnect(conn);
    }
    conn->StartRecv();
}

void NetManager::doClose(uint64_t id)
{
    std::map<uint64_t, NetConnect*>::iterator it = m_conns.find(id);
    if (it != m_conns.end()){
        if (m_back){
            m_back->OnClose(it->second);
        }
        delete it->second;
        m_conns.erase(it);
    }else{
        LOG(ERROR)<<"not find id:"<<id;
    }    
}

void NetManager::OnRecv(NetConnect* conn)
{
    if (m_back){
        m_back->OnMessage(conn);
    }
    conn->StartRecv();
}

void NetManager::OnCloseConn(NetConnect* conn)
{
    doClose(conn->Id());
}

