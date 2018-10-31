#include "net_connect.h"
#include <unistd.h>
#include <strings.h>


NetConnect::~NetConnect()
{
    close(m_fd);
    if (m_send_state == 1){
        ev_io_stop(m_loop, &m_swatcher);
    }
    if (m_recv_state == 1){
        ev_io_stop(m_loop, &m_rwatcher);
    }
}

void NetConnect::StartAccept()
{
    ev_init(&m_rwatcher, AcceptCallBack);
    m_rwatcher.data = this;
    ev_io_set(&m_rwatcher, m_fd, EV_READ);
    ev_io_start(m_loop, &m_rwatcher);
    m_recv_state = 1;
}

void NetConnect::AddSend(const std::string & msg)
{
    m_send_buff.Append(msg);
    if (m_send_state == 0){
        ev_init(&m_swatcher, WriteCallBack);
        m_swatcher.data = this;
        ev_io_set(&m_swatcher, m_fd, EV_WRITE);
        ev_io_start(m_loop, &m_swatcher);
        m_send_state = 1;
    }
}

int32_t NetConnect::Ip()
{
    struct sockaddr_in peeraddr;
    bzero(&peeraddr, sizeof peeraddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
    if (::getpeername(m_fd, (sockaddr*)&peeraddr, &addrlen) < 0) {
        return 0;
    }
    return peeraddr.sin_addr.s_addr;
}

void NetConnect::StartRecv()
{
    ev_init(&m_rwatcher, ReadCallBack);
    m_rwatcher.data = this;
    ev_io_set(&m_rwatcher, m_fd, EV_READ);
    ev_io_start(m_loop, &m_rwatcher);
    m_recv_state = 1;
}

void NetConnect::Read()
{
    int rt = m_read_buff.ReadFd(m_fd);
    if (rt <= 0){
        if (errno != EAGAIN || errno != EWOULDBLOCK || rt == -8888){
            ev_io_stop(m_loop, &m_rwatcher);
            m_recv_state = 0;
            if (m_back){
                m_back->OnCloseConn(this);
            }
            return ;
        }
    }
    if (rt > 0) {
        ev_io_stop(m_loop, &m_rwatcher);
        m_recv_state = 0;
        if (m_back){
            m_back->OnRecv(this);
        }
    }
}

void NetConnect::Write()
{
    do{
        if (m_send_buff.ReadableBytes() <= 0){
            ev_io_stop(m_loop, &m_swatcher);
            m_send_state = 0;
            break;
        }
        const char * w = m_send_buff.Peek();
        size_t len = m_send_buff.ReadableBytes();
        int rt = ::write(m_fd, w, len);
        if (rt < 0)
        {
            if (errno != EWOULDBLOCK)
            {
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    if (m_back)
                    {
                        m_back->OnCloseConn(this);
                    }
                    return ;
                }
            }
        } else {
            if (rt == 0){
                return ;
            }
            m_send_buff.Retrieve(rt);
        }
    }while(true);
}

void NetConnect::Accept()
{
    do {
        sockaddr_storage addr_storage;
        socklen_t addr_len = sizeof(sockaddr_storage);
        sockaddr* saddr =  (sockaddr*)(&addr_storage);
        int fd = accept(m_fd, saddr, &addr_len);
        if (fd<=0){
            return;
        }
        if (init_fd(fd)<0) {
            close(fd);
            return;
        }
        if (m_back)
        {
            m_back->OnConnect(fd, ++m_id);
        }
    }while(true);
}

int init_fd(int fd)
{
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
}
