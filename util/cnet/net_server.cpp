#include "net_server.h"
#include <string.h>
#include <glog/logging.h>

NetServer::NetServer(int num, int interval):
    m_disp(num,this,interval)
{
}

int  NetServer::StartServer(const char* ip, int port, int max)
{
    sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == fd){
        return -1;
    }
    if (init_fd(fd) != 0){
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
    NetConnect *conn = new NetConnect(Loop(), 0, fd, this);
    conn->StartAccept();
    this->Start(1.0);
    return 0;
}

void NetServer::OnConnect(int fd, uint64_t id)
{
    m_disp.AddFd(fd, id);
}

void  NetServer::Send(uint64_t id, std::string & msg)
{
    m_disp.Send(id, msg);
}

void NetServer::Close(uint64_t id)
{
    m_disp.Close(id);
}

