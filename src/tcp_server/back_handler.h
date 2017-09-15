#ifndef __BACK_HANDLER_H
#define __BACK_HANDLER_H

#include "async_ev.h"

class NetBackHandler;

class AsyncBackOp
{
public:
    AsyncBackOp(NetBackHandler* p, int fd, int op){
        m_parent = p;
        m_fd = fd;
        m_op = op;
    };
    virtual ~AsyncNetOp(){
    };
    int		    m_fd;
    int             m_op;
    NetBackHandler*       m_parent;
};

class NetBackHandler :public EvCallBack
{
public:
    NetBackHandler(){
        m_op[0]=0;
        m_op[1]=0;
    };
    virtual ~NetBackHandler(){};
        int  StartServer(int max){
        m_check_handler.Start(1);
        m_handler.Start(max,128);
        m_check_handler.Notify(0, this, &(m_op[0]));
    };
    virtual void CallBack(void * data){
        int * v = (int*)data;
	    if (*v == 0){
	        CheckMq();
	    }else{
	        ConsumMessage();
	    }
    };
private:
    void  CheckMq(){
	DeclareQueue();
	BindQueue();
	uint32_t fd = 0;
	for(;;){
	    //1 ms check
	    m_handler.Notify(fd++, this, &(m_op[1]));
	}
    };
    void ConsumMessage(){
	//
    };
    AsyncEvHandler   m_check_handler;
    AsyncEvHandler   m_handler;
    int		         m_op[2];
    std::string	     m_queue;
    std::vector<std::string>      m_bind_keys;
};

#endif
