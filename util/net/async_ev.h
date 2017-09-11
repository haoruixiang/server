#ifndef   __H_ASYNC_EV_H
#define   __H_ASYNC_EV_H

#include <ev.h>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#ifdef  USE_EVENTFD
#include <sys/eventfd.h>
#else
#endif
#include <glog/logging.h>

class EvCallBack
{
public:
    EvCallBack(){m_version=0;};
    virtual ~EvCallBack(){};
    virtual void CallBack(void * data){};
private:
    unsigned char  m_version;
};

class EvTimeOutCallBack
{
public:
    EvTimeOutCallBack(){};
    virtual ~EvTimeOutCallBack(){};
    virtual void TimeCallBack(){};
};

class EvQueueData
{
public:
    EvQueueData(EvCallBack* back, void* data){m_back = back; m_data = data;};
    ~EvQueueData(){};
    EvCallBack * m_back;
    void*	 m_data;
};

class HEvIter
{
public:
    HEvIter(float timeout = 1.0, EvTimeOutCallBack* b = 0,int queue_max = 12800){
        m_read_id = 0;
        m_write_id = 1;
        m_time_back = b;
        m_time_out = timeout;
        m_queue_max = queue_max;
        m_queue = (EvQueueData**)malloc (queue_max * sizeof(EvQueueData*));
        for (int i=0; i<queue_max; i++){
            m_queue[i] = new EvQueueData(0,0);
        }
    };
    virtual ~HEvIter(){
    };
    void Start() {
        m_loop = ev_loop_new(0);
        m_eventfd[0] = -1; //re
        m_eventfd[1] = -1;
#ifdef USE_EVENTFD
        m_eventfd[0] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
        m_eventfd[1] = m_eventfd[0];
#else
        pipe(m_eventfd);
#endif
        ev_init(&m_watcher, NotifyCallBack);
        m_watcher.data = this;
        ev_io_set(&m_watcher, m_eventfd[0], EV_READ);
        ev_io_start(m_loop, &m_watcher);
        ev_init(&m_twatcher, TimeOutCallBack);
        m_twatcher.data = this;
        ev_timer_set(&m_twatcher, m_time_out, 1);
        ev_timer_start(m_loop, &m_twatcher);
        m_worker = new std::thread([&]() mutable { this->RunLoop();});
    };
    int Notify(EvCallBack *back, void * data){
        do{
            uint64_t cid = m_write_id;
            if (cid - m_read_id >= m_queue_max){
                return -1;
            }
            int id = (cid-1)%m_queue_max;
            m_queue[id]->m_back = back;
            m_queue[id]->m_data = data;
            if (!__sync_bool_compare_and_swap(&m_write_id, cid, cid+1)){
                continue;
            }
            break;
        }while(true);
        uint64_t numadded64 = 1;
        ssize_t bytes_written = ::write(m_eventfd[1], &numadded64, sizeof(numadded64));
        if (bytes_written != sizeof(numadded64)){
        }
        return 0;
    };
    struct ev_loop* Loop(){
        return m_loop;
    };
    void*  Context(uint64_t id){
        return m_ptr[id];
    };
    void   SetContext(uint64_t id, void* p){
        m_ptr[id] = p;
    };
    void   DelContext(uint64_t id){
        std::map<uint64_t, void*>::iterator it = m_ptr.find(id);
        if (it != m_ptr.end()){
            m_ptr.erase(it);
        }
    };
private:
    static void NotifyCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        HEvIter * p = (HEvIter*)(ev->data);
        if (p){
            p->DoNotify();
        }
    };
    static void TimeOutCallBack(EV_P_ ev_timer *w, int revents){
        HEvIter * p = (HEvIter*)(w->data);
        if (p){
            p->DoTimeOut();
        }
    };
    void RunLoop(){
        ev_run(m_loop, 0);
    };
    void DoTimeOut(){
        if (m_time_back){
            m_time_back->TimeCallBack();
        }
    };
    void DoNotify(){
        int m = 0;
        do{
            EvQueueData* back = 0;
            {   
                if (m_write_id-1 > m_read_id){
                    int id = m_read_id % m_queue_max;
                    back = m_queue[id];
                    m_read_id++;
                }
            }
            if (back){
                back->m_back->CallBack(back->m_data);
            }else{
                break;
            }
            m++;
        }while(true);
        uint64_t value[m];
        if (m_eventfd[0] >= 0) {
            ::read(m_eventfd[0], &value, sizeof(value));
        }
    };
    std::thread*        m_worker;
    ev_io               m_watcher;
    ev_timer            m_twatcher;
    struct ev_loop*     m_loop;
    int                 m_eventfd[2];
    std::mutex          m_mtx;
    EvQueueData**       m_queue;
    EvTimeOutCallBack*  m_time_back;
    float               m_time_out;
    uint32_t		    m_queue_max;
    volatile uint64_t	m_read_id;
    volatile uint64_t	m_write_id;
    std::map<uint64_t, void*> m_ptr;
};

class AsyncEvHandler :public EvCallBack
{
public:
    AsyncEvHandler(){
        m_iter_cnt = 0;
        for (uint32_t i = 0; i< 1024; i++){
            m_iters[i] = 0;
        }
    };
    virtual ~AsyncEvHandler(){
        for (uint32_t i = 0; i< 1024; i++){
            if (m_iters[i]){
                m_iters[i]->Notify(this, m_iters[i]);
                m_iters[i] = 0;
            }
        }
    };
    virtual void CallBack(void * data){
        HEvIter * v = (HEvIter*)data;
        if (v){
            ev_break(v->Loop());
            delete v;
        }
    };
    void Start(uint32_t max, float timeout = 1.0, EvTimeOutCallBack* p = 0, int max_queue = 12800){
        if (m_iter_cnt>0){
            return;
        }
        m_iter_cnt = max >1024 ? 1024:max;
        for (uint32_t i = 0; i< m_iter_cnt; i++){
            m_iters[i] = new HEvIter(timeout,p,max_queue);
            m_iters[i]->Start();
        }
    };
    int Notify(uint32_t fd, EvCallBack* back, void * data){
        uint32_t cid = fd % m_iter_cnt;
        if (m_iters[cid]){
            return m_iters[cid]->Notify(back, data);
        }
        return -1;
    };
    struct ev_loop* Loop(uint32_t fd){
        uint32_t cid = fd % m_iter_cnt;
        if (m_iters[cid]){
            return m_iters[cid]->Loop();
        }
        return 0;
    };
    void * Context(uint32_t fd, uint64_t id){
        uint32_t cid = fd % m_iter_cnt;
        if (m_iters[cid]){
            return m_iters[cid]->Context(id);
        }
        return 0;
    };
    void SetContext(uint32_t fd, uint64_t id, void* p){
        uint32_t cid = fd % m_iter_cnt;
        if (m_iters[cid]){
            m_iters[cid]->SetContext(id, p);
        }
    };
    void DelContext(uint32_t fd, uint64_t id){
        uint32_t cid = fd % m_iter_cnt;
        if (m_iters[cid]){
            m_iters[cid]->DelContext(id);
        }
    };
    HEvIter*    m_iters[1024];
    uint32_t	m_iter_cnt;
};

#endif
