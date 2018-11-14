#include "net_dispatcher.h"
#include <glog/logging.h>

NetMessageCallBack::NetMessageCallBack()
{
}

NetMessageCallBack::~NetMessageCallBack()
{
}

class NetCommand
{
public:
    NetCommand(int fd, int cmd, uint64_t id):
        m_fd(fd),
        m_cmd(cmd),
        m_id(id),
        cmd_key(0),
        sqid(0),
        m_next(0),
        m_msg(""),
        m_ex(""),
        m_key("")
    {};
    ~NetCommand(){
    };
    void Clear(){
        m_fd = 0;
        m_cmd = 0;
        m_id = 0;
        cmd_key = 0;
        sqid = 0;
        m_next = 0;
        m_msg.clear();
        m_msg.clear();
        m_ex.clear();
        m_key.clear();
    }
    int         m_fd;
    int         m_cmd;
    uint64_t    m_id;
    uint32_t    cmd_key;
    uint32_t    sqid;
    NetCommand* m_next;
    std::string m_msg;
    std::string m_ex;
    std::string m_key;
};

NetCommand * m_chead = 0;
std::mutex   m_clock;

NetCommand * GetC(int fd, int cmd, uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_clock);
    NetCommand * rt = m_chead;
    if (m_chead){
        m_chead = m_chead->m_next;
        rt ->m_fd = fd;
        rt->m_cmd = cmd;
        rt->m_id = id;
    }else{
        rt = new NetCommand(fd, cmd, id);
    }
    return rt;
}

void  DelC(NetCommand * p)
{
    p->Clear();
    std::lock_guard<std::mutex> lock(m_clock);
    p->m_next = m_chead;
    m_chead = p;
}

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
    if (3== c->m_cmd){
        s->doAddAccept(c->m_fd, c->m_id);
    }
    if (4== c->m_cmd){
        s->doAsyncMessage(c->m_id, c->m_msg);
    }
    if (5== c->m_cmd){
        s->doAsyncMessage(c->cmd_key, c->sqid, c->m_id, c->m_msg);
    }
    if (6== c->m_cmd){
        s->doPublish(c->m_id, c->m_fd, c->m_ex, c->m_key, c->m_msg);
    }
    DelC(c);
    return 0;
}

NetManager::~NetManager()
{
    LOG(ERROR)<<"NetManager exit tid["<<Sid()<<"]";
    do{
        NetConnect* conn = (NetConnect*)m_conns.GetDel();
        if (conn){
            delete conn;
        }else{
            break;
        }
    }while(true);
    sleep(1);
}

bool NetManager::AddAccept(int fd, uint64_t id)
{
    NetCommand * c = GetC(fd, 3, id);
    if (Notify(OnCommand, this, c)<0)
    {
        return false;
    }
    return true;
}

bool NetManager::AddFd(int fd, uint64_t id)
{
    NetCommand * c = GetC(fd, 0, id);
    if (Notify(OnCommand, this, c)<0)
    {
        return false;
    }
    return true;
}

bool NetManager::Close(uint64_t id)
{
    NetCommand * c = GetC(0, 1, id);
    if (Notify(OnCommand, this, c)<0)
    {
        return false;
    }
    return true;
}

bool NetManager::AsyncMessage(uint64_t id, std::string & msg)
{
    NetCommand * c = GetC(0, 4, id);
    c->m_msg.swap(msg);
    if (Notify(OnCommand, this, c)<0)
    {
        return false;
    }
    return true;
}

bool NetManager::AsyncMessage(uint32_t cmd, uint32_t sqid, uint64_t id, std::string & msg)
{
    NetCommand * c = GetC(0, 5, id);
    c->m_msg.swap(msg);
    c->cmd_key = cmd;
    c->sqid = sqid;
    if (Notify(OnCommand, this, c)<0)
    {
        return false;
    }
    return true;
}

bool NetManager::Publish(uint64_t id, int ch, const std::string &exchange, const std::string &key, std::string &message)
{
    NetCommand * c = GetC(ch, 6, id);
    c->m_msg.swap(message);
    c->m_ex = exchange;
    c->m_key = key;
    if (Notify(OnCommand, this, c)<0)
    {
        return false;
    }
    return true;
}

bool NetManager::Send(uint64_t id, std::string & msg)
{
    NetCommand * c = GetC(0, 2, id);
    c->m_msg.swap(msg);
    if (Notify(OnCommand, this, c)<0)
    {
        return false;
    }
    return true;
}

void NetManager::doAsyncMessage(uint64_t id, std::string & msg)
{
    if (m_back){
        m_back->OnAsyncMessage(Sid(), msg);
    }
}

void NetManager::doPublish(uint64_t id, int ch, const std::string &exchange, const std::string &key, std::string &message)
{
    if (m_back){
        m_back->OnPublish(id, ch, exchange, key, message);
    }
}
void NetManager::doAsyncMessage(uint32_t cmd, uint32_t sqid, uint64_t id, std::string & msg)
{
    if (m_back){
        m_back->OnAsyncMessage(Sid(), cmd, sqid, id, msg);
    }
}

void* NetManager::GetContext(uint64_t id)
{
    NetConnect* c = (NetConnect*)m_conns.Get((const char*)&id, sizeof(uint64_t));
    if (c){
        return c->GetContext();
    }
    return 0;
}

void NetManager::doSend(uint64_t id, std::string & msg)
{
    NetConnect* c = (NetConnect*)m_conns.Get((const char*)&id, sizeof(uint64_t));
    if (c){
        c->AddSend(msg);
    }else{
        LOG(ERROR)<<"not find id["<<id<<"] tid["<<Sid()<<"]";
    }
}

void NetManager::doAddFd(int fd, uint64_t id)
{
    NetConnect *conn = new NetConnect(Loop(), id, fd, this);
    conn->m_prev = 0;
    conn->m_next = m_head;
    if (m_head){
        m_head->m_prev = conn;
    }
    m_head = conn;
    m_conns.Set((const char*)&id, sizeof(uint64_t), conn);
    if (m_back){
        m_back->OnConnect(Sid(), conn);
    }
    conn->StartRecv();
}

void NetManager::doAddAccept(int fd, uint64_t id)
{
    NetConnect *conn = new NetConnect(Loop(), id, fd, this);
    conn->m_prev = 0;
    conn->m_next = m_head;
    if (m_head){
        m_head->m_prev = conn;
    }
    m_head = conn;
    m_conns.Set((const char*)&id, sizeof(uint64_t), conn);
    conn->StartAccept();
}

void NetManager::doClose(uint64_t id)
{
    NetConnect* c = (NetConnect*)m_conns.GetDel((const char*)&id, sizeof(uint64_t));
    if (c){
        if (m_back){
            m_back->OnClose(Sid(), c);
        }
        if (c->m_prev == 0){
            m_head = c->m_next;
            if (m_head){
                m_head->m_prev = 0;
            }
        }else{
            NetConnect* prev = c->m_prev;
            NetConnect* next = c->m_next;
            prev->m_next = next;
            if (next){
                next->m_prev = prev;
            }
        }
        c->m_next = 0;
        delete c;
    }else{
        LOG(ERROR)<<"not find id["<<id<<"] tid["<<Sid()<<"]";
    }
}

void NetManager::OnRecv(NetConnect* conn)
{
    if (m_back){
        m_back->OnMessage(Sid(), conn);
    }
    conn->StartRecv();
}

void NetManager::OnCloseConn(NetConnect* conn)
{
    doClose(conn->Id());
}

void NetManager::OnConnect(int fd, uint64_t id)
{
    if (m_back){
        m_back->OnAccept(fd, id);
    }
}

NetDispatcher::~NetDispatcher()
{
    LOG(ERROR)<<"NetDispatcher exit now";
    delete[] m_procs;
}
