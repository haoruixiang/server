#include "notify_queue.h"
#include <glog/logging.h>
NotifyQueue::NotifyQueue(int queue_max)
{
    m_loop = ev_loop_new(0);
    m_read_id = 0;
    m_write_id = 1;
    m_queue_max = queue_max;
    m_queue = (NotifyQueueData**)malloc(
            queue_max * sizeof(NotifyQueueData*));
    for (int i=0; i<queue_max; i++){
        m_queue[i] = new NotifyQueueData(0,0,0);
    }
}

void NotifyQueue::Start(float time_out)
{
    m_eventfd[0] = -1;
    m_eventfd[1] = -1;
    m_eventfd[0] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
    m_eventfd[1] = m_eventfd[0];
    ev_init(&m_watcher, NotifyCallBack);
    m_watcher.data = this;
    ev_io_set(&m_watcher, m_eventfd[0], EV_READ);
    ev_io_start(m_loop, &m_watcher);
    ev_init(&m_twatcher, TimeOutCallBack);
    m_twatcher.data = this;
    ev_timer_set(&m_twatcher, time_out, 1);
    ev_timer_start(m_loop, &m_twatcher);
    m_worker = new std::thread([&]() mutable { this->RunLoop();});
}

int NotifyQueue::Notify(NotifyQueueCallBack back, void* ptr, void * data)
{
    do{
        uint64_t cid = m_write_id;
        if (cid - m_read_id >= m_queue_max){
            return -1;
        }
        int id = (cid-1)%m_queue_max;
        m_queue[id]->m_back = back;
        m_queue[id]->m_data = data;
        m_queue[id]->m_ptr = ptr;
        if (!__sync_bool_compare_and_swap(&m_write_id, cid, cid+1)){
            continue;
        }
        break;
    }while(true);
    uint64_t numadded64 = 1;
    ssize_t bytes_written = write(m_eventfd[1], &numadded64, sizeof(numadded64));
    if (bytes_written != sizeof(numadded64)){
        LOG(WARNING)<<"NotifyQueue write false";
    }
    return 0;
}

void NotifyQueue::OnNotify()
{
    int m = 0;
    do{
        NotifyQueueData* back = 0;
        {
            if (m_write_id-1 > m_read_id){
                int id = m_read_id % m_queue_max;
                back = m_queue[id];
                m_read_id++;
            }
        }
        if (back && back->m_back){
            back->m_back(back->m_ptr, back->m_data);
        }else{
            break;
        }
        m++;
    }while(true);
    uint64_t value[m];
    if (m_eventfd[0] >= 0) {
        read(m_eventfd[0], &value, sizeof(value));
    }
}

