#include "util/json/json.h"
#include "glog/logging.h"
#include "sys_help.h"
#include "async_net.h"

class ClientHandler :public AsyncNetCallBack
{
public:
    ClientHandler(){};
    virtual ~ClientHandler(){};
    int Connect(const char* ip, int port){
	if (m_handler.ConnectServer(this, ip, port, 0)<0){
            LOG(ERROR)<<"ConnectServer "<<ip<<" "<<port<<" false";
            return -1;
        }
	return 0;
    };
private:
    virtual void OnConnect(uint32_t tid, AsyncConn* conn, void* data){
        LOG(INFO)<<"OnConnect:"<<tid<<" id:"<<conn->GetId()<<" fd:"<<conn->GetFd()<<" "<<conn;
    };
    virtual size_t OnMessage(uint32_t tid, AsyncConn* conn, const char* buff, size_t len){
        return 0;
    };
    virtual void CloseConn(uint32_t tid, AsyncConn* conn, void* data){
        close(conn->GetFd());
    };
    virtual void OnTimeOut(uint32_t tid){
    };
    AsyncNet         m_handler;
};

int main(int agrv, char** agrc)
{
    enableCoredump();
    ClientHandler net;
    for (int i=0; i<200; i++){
        net.Connect("183.60.212.19",8088);
    }
    sleep(10);
    return 0;
}

