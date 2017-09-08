#ifndef  __ASYNC_NET_H
#define  __ASYNC_NET_H

#include "async_ev.h"
#include <sys/types.h>
#include <sys/socket.h>
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

class AsyncNetCallBack
{
public:
    AsyncNetCallBack(){};
    virtual ~AsyncNetCallBack(){};
    virtual size_t ReadMsgOk(const char* buff, size_t len, uint64_t id, int fd){return 0;};
    virtual void CloseConn(uint64_t id, int fd){};
    virtual void OnConnect(uint64_t id, int fd){};
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

class AsyncNetOp
{
public:
    AsyncNetOp(AsyncNet* p, int fd, int op, uint64_t id = 0){
        m_id = id;
        m_read_buff = 0;
        m_parent = p;
        m_fd = fd;
        m_op = op;
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
    int             m_fd;
    int		        m_op;
    ev_io           m_rwatcher;
    ev_io           m_swatcher;
    uint64_t        m_id;
    HNetBuff*       m_read_buff;
    std::list<HNetSendBuff*> m_send_buff;
    AsyncNet*	    m_parent;
};

class AsyncNet :public EvCallBack
{
public:
    AsyncNet(){
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
        AsyncNetOp* op = new AsyncNetOp(this, fd, 0);
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
        m_handler.Start(1);
        AsyncNetOp* op = new AsyncNetOp(this, fd, 3);
        m_handler.Notify((uint32_t)fd, this, op);
        if (m_back){
            m_back->OnConnect(0, fd);
        }
        return 0;
    };
    virtual void CallBack(void * data){
        AsyncNetOp * v = (AsyncNetOp*)data;
        (this->*m_op[v->m_op])(v);
    };
    void  SendMsg(std::string & msg, uint64_t id, int fd){
        AsyncNetOp* op = new AsyncNetOp(this, fd, 1, id);
        HNetSendBuff * buff = new HNetSendBuff();
        buff->Swap(msg);
        op->m_send_buff.push_back(buff);
        m_handler.Notify((uint32_t)fd, this, op);
    };
    void CloseFd(uint64_t id, int fd){
        AsyncNetOp* op = new AsyncNetOp(this, fd, 2);
        m_handler.Notify((uint32_t)fd, this, op);
    };
private:
    void AddAcceptFd(AsyncNetOp* op){
        ev_init(&(op->m_rwatcher), AcceptCallBack);
        op->m_rwatcher.data = op;
        ev_io_set(&(op->m_rwatcher), op->m_fd, EV_READ);
        ev_io_start(m_handler.Loop((uint32_t)(op->m_fd)), &(op->m_rwatcher));
    };
    void AddCloseFd(AsyncNetOp* op){
        AsyncNetOp * rop = (AsyncNetOp*)m_handler.Context(op->m_id, op->m_fd);
        if (rop && rop->m_fd == op->m_fd && rop->m_id == op->m_id){
            ev_io_stop(m_handler.Loop((uint32_t)rop->m_fd), &(rop->m_rwatcher));
            ev_io_stop(m_handler.Loop((uint32_t)rop->m_fd), &(rop->m_swatcher));
            delete rop;
        }
        m_handler.DelContext(op->m_id, op->m_fd);
        if (m_back){
            m_back->CloseConn(op->m_id, op->m_fd);
        }
        delete op;
    };
    void AddSendMsg(AsyncNetOp* op){
        AsyncNetOp * rop = (AsyncNetOp*)m_handler.Context(op->m_id, op->m_fd);
        if (rop && op->m_fd == rop->m_fd && op->m_id == rop->m_id){
            if (rop->m_send_buff.size()<=0){
                ev_init(&(rop->m_swatcher), WriteCallBack);
                rop->m_swatcher.data = rop;
                ev_io_set(&(rop->m_swatcher), rop->m_fd, EV_WRITE);
                ev_io_start(m_handler.Loop((uint32_t)(rop->m_fd)), &(rop->m_swatcher));
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
        ev_io_set(&(op->m_rwatcher), op->m_fd, EV_READ);
        ev_io_start(m_handler.Loop((uint32_t)(op->m_fd)), &(op->m_rwatcher));
        ev_init(&(op->m_swatcher), WriteCallBack);
        op->m_swatcher.data = op;
        ev_io_set(&(op->m_swatcher), op->m_fd, EV_WRITE);
        ev_io_start(m_handler.Loop((uint32_t)(op->m_fd)), &(op->m_swatcher));
        m_handler.SetContext(op->m_id, op->m_fd, op); //register
    };
    void Accept(AsyncNetOp* op){
        if (!op){return;}
        do {
            sockaddr_storage addr_storage;
            socklen_t addr_len = sizeof(sockaddr_storage);
            sockaddr* saddr =  (sockaddr*)(&addr_storage);
            int fd = accept(op->m_fd, saddr, &addr_len);
            if (fd<=0){
                return;
            }
            if (FdInit(fd)<0) {
                close(fd);
                return;
            }
            AsyncNetOp* cop = new AsyncNetOp(this, fd, 3, op->m_id++);
            m_handler.Notify((uint32_t)fd, this, cop);
        }while(true);
    };
    void Read(AsyncNetOp* op){
        if (!op){return;}
        if (!op->m_read_buff){
            op->m_read_buff = new HNetBuff();
        }
        int rt = op->m_read_buff->ReadFd(op->m_fd);
        if (rt <= 0){
            if (errno != EAGAIN || errno != EWOULDBLOCK || rt == -8888){
                CloseFd(op->m_id, op->m_fd);
                return ;
            }
        }
        if (rt > 0 && m_back ){
            size_t len = m_back->ReadMsgOk(op->m_read_buff->Peek(), op->m_read_buff->ReadableBytes(), op->m_id, op->m_fd);
            op->m_read_buff->Retrieve(len);
        }
    };
    void Write(AsyncNetOp* op){
        if (!op){return;}
        do{
            if (op->m_send_buff.size()<=0){
                ev_io_stop(m_handler.Loop((uint32_t)(op->m_fd)), &(op->m_swatcher));
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
            int rt = ::write(op->m_fd, w, len);
            if (rt < 0) {
                if (errno != EWOULDBLOCK) {
                    if (errno == EPIPE || errno == ECONNRESET){
                        CloseFd(op->m_id, op->m_fd);
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
    void            (AsyncNet::*m_op[5])(AsyncNetOp*); //void (Test::*add[2])();
};

#endif
