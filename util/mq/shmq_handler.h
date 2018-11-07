#ifndef  _SHMQ_HANDLER_H
#define  _SHMQ_HANDLER_H
#include "hdc_shm_queue.h"
#include "hdc_memery.h"
#include "notify_queue.h"
#include <vector>
#include <map>
class ShmMessageCallBack
{
public:
    ShmMessageCallBack(){};
    ~ShmMessageCallBack(){};
    virtual void OnMessage(int tid, const std::string &route_key, 
        const std::string &replyto, const std::string &correlation_id, std::string &body){};
};

class ShmqOp
{
public:
    ShmqOp(const std::string & q):queue(q),id(0){
    };
    ~ShmqOp(){};
    std::string queue;
    uint32_t   id;
};

#define  SHM_BLOCK_BUFF 1024*1024
class ShmBuff
{
public:
    ShmBuff():buff(0),size(0){
    };
    ~ShmBuff(){
        if (buff){
            delete buff;
        }
    };
    void Broaden(){
        if (buff){
            delete buff;
        }
        size += SHM_BLOCK_BUFF;
        buff = new char[size];
    };
    char*  buff;
    int    size;
};

class ShmqHandler : public NotifyQueue
{
public:
    ShmqHandler(ShmMessageCallBack* back);
    ~ShmqHandler();
    bool InitMemery(const std::string &key, size_t size);
    bool AddQueue(const std::string &queue, uint32_t max_size, std::vector<std::string> &keys);
    bool Publish(const std::string & key, const std::string & replyto, const std::string &correlation_id, std::string & msg);
    void OnMessage(const char* buff, size_t len, const char* queue, size_t qlen);
    virtual void OnTimeOut(){
        doConsume();
    };
private:
    void doConsume();
    void parse(char* buff, size_t size, const std::string & queue);
    char*                 m_ptr;
    ShmMessageCallBack   *m_back;
    volatile uint8_t      m_exit;
    std::vector<ShmqOp*>  m_qs;
    ShmBuff  m_buff;
    std::mutex            m_lock;
    std::map<std::string, unsigned int> m_key_ids;   
};

#endif
