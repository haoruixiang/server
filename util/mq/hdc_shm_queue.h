#ifndef _HDC_SHM_QUEUE_H
#define _HDC_SHM_QUEUE_H

const char * shm_get_error(int error);
int shm_init(char* ptr, unsigned int size);
int shm_queue_declare(char* ptr, const char* queue_name, int len, unsigned int size, unsigned int& idx);
int shm_queue_has_message(char* ptr, const char* queue, int qlen);
int shm_queue_has_message(char* ptr, unsigned int id);
int shm_queue_id(char* ptr, const char* queue_name, int len, unsigned int& idx);
int shm_queue_bind(char* ptr, const char* queue_name, int len, const char* key_name,  int klen);
int shm_queue_unbind(char* ptr, const char* queue_name, int len, const char* key_name, int klen);
int shm_queue_push_cas(char* ptr, const char* queue, int qlen, unsigned int& q_id, const char* data, int len);
int shm_queue_pop_cas(char* ptr, const char* queue, int qlen, unsigned int& q_id, char* data, int max);
int shm_publish_message(char* ptr, const char* key, int klen, unsigned int& key_id, const char* data, int len);
int shm_register_pid(char* ptr, unsigned int& qid, unsigned int pid);
int shm_unregister_pid(char* ptr, unsigned int& qid, unsigned int pid);
int shm_pid_work(char* ptr, unsigned int& qid, unsigned int pid);
int shm_pid_stop(char* ptr, unsigned int& qid, unsigned int pid);
int shm_register_pid(char* ptr, const char* queue, int qlen, unsigned int pid);
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

#endif

