#ifndef  _SHMQ_HANDLER_H
#define  _SHMQ_HANDLER_H
#include "async_ev.h"
#include "hdc_shm_queue.h"
#include "hdc_memery.h"

typedef struct {
    std::string      m_queue;
    uint32_t         m_max_size;
    std::vector<std::string> m_bind_keys;
}SHM_QUEUE;

class ShmqOp
{
public:
    ShmqOp(std::string & q):queue(q),id(0){
    };
    ~ShmqOp(){};
    std::string queue;
    uint32_t   id;
};

#define  SHM_BLOCK_BUFF 1024*1024
class ShmBuff
{
public:
    ShmBuff(){
        size = 0;
        buff = 0;
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

class ShmqHandler : public EvCallBack , public EvTimeOutCallBack
{
public:
    ShmqHandler(){
        m_ptr = 0;
        m_notice_id = 1;
        m_max_thread = 0;
    };
    ~ShmqHandler(){
    };
    int Consume(std::string &key, size_t size, 
                std::vector<SHM_QUEUE*>& queues, 
                uint32_t max_thread = 4){
        if (max_thread<=1){
            return -1;
        }
        m_ptr = (char*)ShmPorint(key.c_str(), size);
        if (!m_ptr){
            return -1;
        }
        if (shm_init(m_ptr, size)<0){
            return -1;
        }
        for (size_t i=0; i<queues.size(); i++){
            ShmqOp * op = new ShmqOp(queues[i]->m_queue);
            if (shm_queue_declare(m_ptr, 
                                    queues[i]->m_queue.c_str(),
                                    queues[i]->m_queue.length(),
                                    queues[i]->m_max_size, op->id)<0){
                delete op;
                return -1;
            }
            for (size_t j=0; j<queues[i]->m_bind_keys.size(); j++){
                if (shm_queue_bind(m_ptr,
                                queues[i]->m_queue.c_str(),
                                queues[i]->m_queue.length(),
                                queues[i]->m_bind_keys[j].c_str(),
                                queues[i]->m_bind_keys[j].length())<0){
                    delete op;
                    return -1;
                }
            }
            m_qs.push_back(op);
        }
        for (uint32_t i=0; i<max_thread; i++){
            ShmBuff * b = new ShmBuff();
            m_buff.push_back(b);
        }
        m_handler.Start(max_thread, 0.001, this);
        m_max_thread = max_thread;
        return 0;
    };
    void Notify(){
        uint32_t id=1;
        for (size_t i=0; i < m_qs.size(); i++){
            m_handler.Notify(id, this, m_qs[i]);
            id ++;
            if (id>=m_max_thread){
                id = 1;
            }
        }
    };
    void OnMessage(const char* buff, size_t len, const char* queue, size_t qlen){
    };
private:
    virtual void CallBack(uint32_t id, void * data){
        ShmqOp* op = (ShmqOp*)data;
        if (!m_buff[id]->buff){
            m_buff[id]->Broaden();
        }
        do{
            int rt = shm_queue_pop_cas(m_ptr, op->queue.c_str(), 
                    op->queue.size(),
                    op->id,
                    m_buff[id]->buff, m_buff[id]->size);
            if (rt <= 0){
                if (rt == -103){
                    m_buff[id]->Broaden();
                    continue;
                }
                break;
            }
            OnMessage(m_buff[id]->buff, rt, op->queue.c_str(), op->queue.size());
        }while(true);
    };
    virtual void TimeCallBack(){
        for (size_t i=0; i < m_qs.size(); i++){
            if (shm_queue_has_message(m_ptr, m_qs[i]->id)){
                m_handler.Notify(m_notice_id, this, m_qs[i]);
                m_notice_id++;
                if (m_notice_id>=m_max_thread){
                    m_notice_id = 1;
                }
            }   
        }
    };
    char*           m_ptr;
    std::vector<ShmqOp*> m_qs;
    std::vector<ShmBuff*> m_buff;
    AsyncEvHandler  m_handler;
    uint32_t        m_max_thread;
    uint32_t        m_notice_id;
};
#endif
