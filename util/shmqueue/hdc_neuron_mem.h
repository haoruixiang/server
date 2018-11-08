#pragma once
#ifdef __cplusplus
extern "C" {
#endif 

void shm_test_queue(char* ptr, const char* queue_name, int len);

const char * shm_get_error(int error);

char * shm_malloc_buff(int max);

int shm_init(char* ptr, unsigned int size);

int shm_queue_declare(char* ptr, const char* queue_name, int len, unsigned int size);

int shm_queue_has_message(char* ptr, const char* queue, int qlen);

int shm_queue_has_message_id(char* ptr, unsigned int id);

unsigned int shm_queue_id(char* ptr, const char* queue_name, int len);

unsigned int shm_key_id(char* ptr, const char* key_name, int len);

int shm_queue_bind(char* ptr, const char* queue_name, int len, const char* key_name,  int klen);

int shm_queue_unbind(char* ptr, const char* queue_name, int len, const char* key_name, int klen);

int shm_queue_push_cas(char* ptr, const char* queue, int qlen, const char* data, int len);

int shm_queue_push_cas_by_id(char* ptr, unsigned int q_id, const char* data, int len);

int shm_queue_pop_cas(char* ptr, const char* queue, int qlen, char* data, int max);

int shm_queue_pop_cas_by_id(char* ptr, unsigned int q_id, char* data, int max);

int shm_publish_message(char* ptr, const char* key, int klen, const char* data, int len);

int shm_publish_message_by_id(char* ptr, unsigned int key_id, const char* data, int len);

int publish_async_msg(char* ptr, const char* key, int klen, unsigned int id, const char* data, int len, char *correlation_id, int clen, int type);

int get_del_message(char* ptr, unsigned int q_id, char* data, int max);

typedef struct {
    //1. routing_key, 2. correlation_id, 3. reply_to, 4. delivery_mode
    short    routing_key_max;
    short    routing_key_len;
    short    correlation_id_max;
    short    correlation_id_len;
    short    reply_to_max;
    short    reply_to_len;
    short    delivery_mode;
    int      max;
    int      move_len;
} ShmQueueInfo;

int shm_queue_pop(char* ptr, unsigned int q_id, ShmQueueInfo* prl, char * routing_key, char *correlation_id, char *reply_to, char* buff);

/*
unsigned int shm_register_pid_q(char* ptr, unsigned int& qid, unsigned int pid);

int shm_unregister_pid_q(char* ptr, unsigned int& qid, unsigned int pid);

int shm_pid_work_q(char* ptr, unsigned int& qid, unsigned int pid);

int shm_pid_stop_q(char* ptr, unsigned int& qid, unsigned int pid);

int shm_register_pid(const char* ptr, const char* queue, int qlen, unsigned int pid);

int shm_unregister_pid(char* ptr, const char* queue, int qlen, unsigned int pid);

int shm_pid_work(char* ptr, const char* queue, int qlen, unsigned int pid);

int shm_pid_stop(char* ptr, const char* queue, int qlen, unsigned int pid);

struct shm_q_info
{
    char            m_name[32];
    unsigned int    m_id;
    unsigned int    m_bind[32];
    unsigned int    m_time;
    unsigned int    m_pids[8];
    unsigned int    m_node_num;
    unsigned int    m_free_num;
};

int  shm_find_queue_ids(char* ptr, unsigned int * id, int max);

int  shm_get_queue_info(char* ptr, unsigned int id, shm_q_info & info);

int  shm_get_key_info(char* ptr, unsigned int id, shm_q_info & info);

int  shm_get_shm_info(char* ptr, shm_q_info & info);

int  shm_queue_delete(char* ptr, const char* queue, int qlen);

int  shm_queue_clear(char* ptr, const char* queue, int qlen, bool bauto = false);
*/
#ifdef __cplusplus
}
#endif


