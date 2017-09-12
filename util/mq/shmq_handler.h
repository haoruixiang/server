#ifndef  _SHMQ_HANDLER_H
#define  _SHMQ_HANDLER_H
#include "async_ev.h"

class ShmqHandler : public EvCallBack , public EvTimeOutCallBack
{
public:
    ShmqHandler(){};
    ~ShmqHandler(){};
    int Init(std::string &key, size_t size, uint32_t max_thread){
        //
    };
    int Notice(){
        //
    };
private:
    virtual void CallBack(void * data){
        //op
    };
    virtual void TimeCallBack(){
        //check -> notice
    };
    AsyncEvHandler  m_handler;
    uint32_t        m_max_thread;
    uint32_t        m_notice_id;
};

#endif
