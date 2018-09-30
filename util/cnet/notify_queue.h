#ifndef   __H_NOTIFY_QUEUE_H
#define   __H_NOTIFY_QUEUE_H
#include <ev.h>
#include <mutex>
#include <thread>
#include <sys/eventfd.h>

typedef  int (*NotifyQueueCallBack)(void* ptr, void* data);

class NotifyQueueData
{
public:
    NotifyQueueData(NotifyQueueCallBack back, void* ptr, void*data):
        m_back(back),m_ptr(ptr),m_data(data)
    {};
    ~NotifyQueueData(){};
    NotifyQueueCallBack m_back;
    void*               m_ptr;
    void*               m_data;
};

class NotifyQueue
{
public:
    NotifyQueue(int queue_max = 1280);
    virtual ~NotifyQueue(){};
    virtual void OnTimeOut(){};
    void Start(float time_out = 10.0);
    int  Notify(NotifyQueueCallBack back, void* ptr, void * data);
    struct ev_loop* Loop(){
        return m_loop;
    };
private:
    static void NotifyCallBack(struct ev_loop* loop, struct ev_io* ev, int events){
        NotifyQueue * p = (NotifyQueue*)(ev->data);
        if (p){
            p->OnNotify();
        }
    }
    static void TimeOutCallBack(EV_P_ ev_timer *w, int revents){
        NotifyQueue * p = (NotifyQueue*)(w->data);
        if (p){
            p->OnTimeOut();
        }
    }
    void RunLoop(){
        ev_run(m_loop, 0);
    }
    void OnNotify();

    std::thread*        m_worker;
    ev_io               m_watcher;
    ev_timer            m_twatcher;
    struct ev_loop*     m_loop;
    int                 m_eventfd[2];
    std::mutex          m_mtx;
    NotifyQueueData**   m_queue;
    float               m_time_out;
    uint32_t            m_queue_max;
    volatile uint64_t   m_read_id;
    volatile uint64_t   m_write_id;
    uint32_t            m_id;
    void*               m_ptr;
};
#endif
