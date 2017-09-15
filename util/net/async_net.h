#ifndef  __ASYNC_NET_H
#define  __ASYNC_NET_H

#include "async_ev.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

class HNetBuff
{
public:
    HNetBuff():buffer(1024),reader(0),writer(0){
    };
    ~HNetBuff(){
    };
    int ReadFd(int fd){
        char extrabuf[65536];
        struct iovec vec[2];
        const size_t writable = WritableBytes();
        vec[0].iov_base = Begin() + writer;
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);
        const ssize_t n = ::readv(fd, vec, 2);
        if (n < 0) {
        } else if (static_cast<size_t>(n) <= writable) {
            writer += n;
        } else {
            writer = buffer.size();
            Append(extrabuf, n - writable);
        }
        return n;
    };
    size_t ReadableBytes() const {
        return writer - reader; 
    };
    size_t WritableBytes() const {
        return buffer.size() - writer;
    };
    void Append(const std::string& str) {
        Append(str.c_str(), str.size());
    };
    void Append(const char* data, size_t len) {
        EnsureWritableBytes(len);
        std::copy(data, data+len, BeginWrite());
        writer += len;
    };
    void EnsureWritableBytes(size_t len) {
        if (WritableBytes() < len) {
            MakeSpace(len);
        }
    }; 
    void MakeSpace(size_t len) {
        if (WritableBytes() + reader < len) {
            buffer.resize(writer + len);
        } else {
            size_t readable = ReadableBytes();
            std::copy(Begin()+reader, Begin()+writer, Begin());
            reader = 0;
            writer = readable;
        }
    };
    const char* Peek() const {
        return Begin() + reader;
    };
    void Retrieve(size_t len) {
        if (len < ReadableBytes()) {
            reader += len;
        } else {
            reader = 0;
            writer = 0;
        }
    };
    char* BeginWrite() {
        return Begin() + writer;
    };
    char* Begin() {
        return &*buffer.begin();
    };
    const char* Begin() const {
        return &*buffer.begin();
    };
    std::vector<char>  buffer;
    size_t             reader;
    size_t             writer;
};

class AsyncConn
{
public:
    AsyncConn(int fd, uint64_t id){
        m_fd = fd;
        m_id = id;
    };
    virtual ~AsyncConn(){};
    void* GetConext(){return m_conext;};
    void SetContext(void* ptr){m_conext = ptr;};
    int   GetFd(){return m_fd;};
    uint64_t GetId(){return m_id;};
    uint64_t AddId(){return m_id++;};
private:
    int             m_fd;
    uint64_t        m_id;
    void*           m_conext;
};

class AsyncNetCallBack
{
public:
    AsyncNetCallBack(){};
    virtual ~AsyncNetCallBack(){};
    virtual size_t OnMessage(AsyncConn* conn, const char* buff, size_t len){return 0;};
    virtual void CloseConn(AsyncConn* conn){};
    virtual void OnConnect(AsyncConn* conn){};
    virtual void OnTimeOut(){};
};

class HNetSendBuff
{
public:
    void Swap(std::string & msg) {
        buff.swap(msg);
    }
    std::string  buff;
    size_t       send_len;
};

class AsyncNet;

class AsyncNetOp :public AsyncConn
{
public:
    AsyncNetOp(AsyncNet* p, int fd, int op, uint64_t id):AsyncConn(fd,id){
        m_read_buff = 0;
        m_parent = p;
        m_op = op;
        m_status = 0;
    };
    virtual ~AsyncNetOp(){
        std::list<HNetSendBuff*>::iterator it;
        if (m_read_buff){
            delete m_read_buff;
        }
        for (it = m_send_buff.begin(); it != m_send_buff.end(); it++){
            delete (*it);
        }
    };
    int		        m_op;
    ev_io           m_rwatcher;
    ev_io           m_swatcher;
    HNetBuff*       m_read_buff;
    std::list<HNetSendBuff*> m_send_buff;
    AsyncNet*	    m_parent;
    int             m_status;
};

class AsyncNet :public EvCallBack , public EvTimeOutCallBack
{
public:
    AsyncNet(){
        m_cid = 0;
        m_op[0] = &AsyncNet::AddAcceptFd;
        m_op[1] = &AsyncNet::AddSendMsg;
        m_op[2] = &AsyncNet::AddCloseFd;
        m_op[3] = &AsyncNet::AddReadWrite;
    };
    virtual ~AsyncNet(){};
    int  StartAcceptServer(AsyncNetCallBack* back, const char* ip, int port, int max){
        m_back = back;
        sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip);
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == fd){
            return -1;
        }
        if (FdInit(fd) != 0){
            close(fd);
            return -1;
        }
        if (bind(fd, (struct sockaddr*)&addr, (socklen_t)(sizeof(struct sockaddr))) != 0){
            close(fd);
            return -1;
        }
        if (listen(fd, SOMAXCONN) == -1){
            close(fd);
            return -1;
        }
        m_handler.Start(max);
        AsyncNetOp* op = new AsyncNetOp(this, fd, 0, 0);
        m_handler.Notify((uint32_t)fd, this, op);
        return 0;
    };
    int ConnectServer(AsyncNetCallBack* back, const char* ip, int port){
        m_back = back;
        sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip);
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(fd, (const sockaddr*)&addr, (socklen_t)(sizeof(struct sockaddr))) !=0){
            close(fd);
            return -1;
        }
        if (FdInit(fd) != 0){
            close(fd);
            return -1;
        }
        m_handler.Start(1, 1.0, this);
        AsyncNetOp* op = new AsyncNetOp(this, fd, 3, m_cid);
        m_handler.Notify((uint32_t)fd, this, op);
        if (m_back){
            m_back->OnConnect((AsyncConn*)op);
        }
        m_cid++;
        LOG(INFO)<<"ConnectServer:"<<op<<" :"<<op->GetFd();
        return 0;
    };
    virtual void CallBack(uint32_t id, void * data){
        AsyncNetOp * v = (AsyncNetOp*)data;
        (this->*m_op[v->m_op])(v);
    };
    virtual void TimeCallBack(){
        if (m_back){
            m_back->OnTimeOut();
        }
    };
    void  SendMsg(std::string & msg, uint64_t id, int fd){
        AsyncNetOp* op = new AsyncNetOp(this, fd, 1, id);
        HNetSendBuff * buff = new HNetSendBuff();
        buff->Swap(msg);
        op->m_send_buff.push_back(buff);
        if (m_handler.Notify((uint32_t)fd, this, op)<0){
            LOG(ERROR)<<"Notify:"<<fd<<" id:"<<id<<" false";
        }
        LOG(INFO)<<"SendMsg:"<<op<<" size:"<<msg.size()<< " fd:"<<fd<<" id:"<< id;
    };
    void CloseFd(uint64_t id, int fd){
        AsyncNetOp* op = new AsyncNetOp(this, fd, 2, id);
        LOG(INFO)<<"CloseFd:"<<id<<" "<<fd<<" "<<op;
        m_handler.Notify((uint32_t)fd, this, op);
    };
    int Notify(uint32_t fd, EvCallBack* back ,void* ptr){
        return m_handler.Notify(fd, back, ptr);
    };
private:
    void AddAcceptFd(AsyncNetOp* op){
        ev_init(&(op->m_rwatcher), AcceptCallBack);
        op->m_rwatcher.data = op;
        ev_io_set(&(op->m_rwatcher), op->GetFd(), EV_READ);
        ev_io_start(m_handler.Loop((uint32_t)(op->GetFd())), &(op->m_rwatcher));
    };
    void AddCloseFd(AsyncNetOp* op){
        AsyncNetOp * rop = (AsyncNetOp*)m_handler.Context(op->GetId(), op->GetFd());
        if (rop && rop->GetFd() == op->GetFd() && rop->GetId() == op->GetId()){
            ev_io_stop(m_handler.Loop((uint32_t)rop->GetFd()), &(rop->m_rwatcher));
            ev_io_stop(m_handler.Loop((uint32_t)rop->GetFd()), &(rop->m_swatcher));
            m_handler.DelContext(rop->GetId(), rop->GetFd());
            if (m_back){
                m_back->CloseConn(rop);
            }
            delete rop;
        }
        delete op;
    };
    void CloseOp(AsyncNetOp* op){
        LOG(INFO)<<"CloseOp:"<<op<<" "<<op->GetFd();
        if (op){
            ev_io_stop(m_handler.Loop((uint32_t)op->GetFd()), &(op->m_rwatcher));
            ev_io_stop(m_handler.Loop((uint32_t)op->GetFd()), &(op->m_swatcher));
            m_handler.DelContext(op->GetId(), op->GetFd());
            if (m_back){
                m_back->CloseConn(op);
            }
            delete op;
        }
    };
    void AddSendMsg(AsyncNetOp* op){
        AsyncNetOp * rop = (AsyncNetOp*)m_handler.Context(op->GetId(), op->GetFd());
        LOG(INFO)<<"AddSendMsg:"<<op<<" :"<<rop;
        if (rop && op->GetFd() == rop->GetFd() && op->GetId() == rop->GetId()){
            LOG(INFO)<<"AddSendMsg:"<<op<<" :"<<rop;
            if (rop->m_status == 0){
                ev_init(&(rop->m_swatcher), WriteCallBack);
                rop->m_swatcher.data = rop;
                ev_io_set(&(rop->m_swatcher), rop->GetFd(), EV_WRITE);
                ev_io_start(m_handler.Loop((uint32_t)(rop->GetFd())), &(rop->m_swatcher));
                rop->m_status = 1;
            }
            HNetSendBuff * buff = op->m_send_buff.front();
            op->m_send_buff.pop_front();
            rop->m_send_buff.push_back(buff);
        }
        delete op;
    };
    void AddReadWrite(AsyncNetOp* op){
        ev_init(&(op->m_rwatcher), ReadCallBack);
        op->m_rwatcher.data = op;
        ev_io_set(&(op->m_rwatcher), op->GetFd(), EV_READ);
        ev_io_start(m_handler.Loop((uint32_t)(op->GetFd())), &(op->m_rwatcher));
        ev_init(&(op->m_swatcher), WriteCallBack);
        op->m_swatcher.data = op;
        ev_io_set(&(op->m_swatcher), op->GetFd(), EV_WRITE);
        ev_io_start(m_handler.Loop((uint32_t)(op->GetFd())), &(op->m_swatcher));
        m_handler.SetContext(op->GetId(), op->GetFd(), op); //register
        op->m_status = 1;
    };
    void Accept(AsyncNetOp* op){
        if (!op){return;}
        do {
            sockaddr_storage addr_storage;
            socklen_t addr_len = sizeof(sockaddr_storage);
            sockaddr* saddr =  (sockaddr*)(&addr_storage);
            int fd = accept(op->GetFd(), saddr, &addr_len);
            if (fd<=0){
                return;
            }
            if (FdInit(fd)<0) {
                close(fd);
                return;
            }
            AsyncNetOp* cop = new AsyncNetOp(this, fd, 3, op->AddId());
            m_handler.Notify((uint32_t)fd, this, cop);
        }while(true);
    };
    void Read(AsyncNetOp* op){
        if (!op){return;}
        if (!op->m_read_buff){
            op->m_read_buff = new HNetBuff();
        }
        int rt = op->m_read_buff->ReadFd(op->GetFd());
        if (rt <= 0){
            if (errno != EAGAIN || errno != EWOULDBLOCK || rt == -8888){
                LOG(ERROR)<<"read error:"<<errno<<" "<<op<<" "<<op->GetFd();
                CloseOp(op); 
                return ;
            }
        }
        LOG(INFO)<<"Read:"<<op->GetFd()<<" size:"<<rt;
        if (rt > 0 && m_back ){
            size_t len = m_back->OnMessage(op, op->m_read_buff->Peek(), op->m_read_buff->ReadableBytes());
            op->m_read_buff->Retrieve(len);
        }
    };
    void Write(AsyncNetOp* op){
        if (!op){return;}
        do{
            if (op->m_send_buff.size()<=0){
                ev_io_stop(m_handler.Loop((uint32_t)(op->GetFd())), &(op->m_swatcher));
                op->m_status = 0;
                break;
            }
            HNetSendBuff* f = op->m_send_buff.front();
            if (f->send_len >= f->buff.size()){
                op->m_send_buff.pop_front();
                delete f;
                continue;
            }
            const char * w = f->buff.c_str()+f->send_len;
            size_t len = f->buff.size() - f->send_len;
            int rt = ::write(op->GetFd(), w, len);
            LOG(INFO)<<"Write:"<<op->GetFd()<<" size:"<<rt;
            if (rt < 0) {
                if (errno != EWOULDBLOCK) {
                    if (errno == EPIPE || errno == ECONNRESET){
                        LOG(ERROR)<<"write error:"<<errno<<" "<<op;
                        CloseOp(op);
                        return ;
                    }
                }
            } else {
                f->send_len += rt;
                if (rt == 0){
                    return ;
                }
            }
        }while(true);
    };
    static void ReadCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        AsyncNetOp * p = (AsyncNetOp*)(ev->data);
        if (p && p->m_parent){
            p->m_parent->Read(p);
        }
    };
    static void WriteCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        AsyncNetOp * p = (AsyncNetOp*)(ev->data);
        if (p && p->m_parent){
            p->m_parent->Write(p);
        }
    };
    static void AcceptCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        AsyncNetOp * p = (AsyncNetOp*)(ev->data);
        if (p && p->m_parent){
            p->m_parent->Accept(p);
        }
    };
    int FdInit(int fd){
        int rt = -1;
        do{
            if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0){
                break;
            }
            int one = 1;
            if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0){
                break;
            }
            if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) != 0){
                break;
            }
            int old_flags = fcntl(fd, F_GETFD, 0);
            if (old_flags < 0){
                break;
            }
            int new_flags = old_flags | FD_CLOEXEC;
            if (fcntl(fd, F_SETFD, new_flags) <0){
                break;
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0){
                break;
            }
            rt = 0;
        }while(false);
        return rt;
    };
    AsyncEvHandler	m_handler;
    AsyncNetCallBack * m_back;
    uint64_t        m_cid;
    void            (AsyncNet::*m_op[5])(AsyncNetOp*); //void (Test::*add[2])();
};

#endif
