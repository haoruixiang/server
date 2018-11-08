#include "hdc_neuron_mem.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
//#include <errno.h>
#include <stdio.h>
#define  SHM_PE                 sizeof(unsigned int)
#define  SHM_MAX_BLOCK_SIZE     512
#define  SHM_MAX_BUFF           (SHM_MAX_BLOCK_SIZE - (4*SHM_PE))
#define  SHM_MAX_BIND_KEY       64
#define  SHM_MAX_QUEUE          256

/* lock info
 * 0 can write
 * 1 can read
 */

const char * shm_neuron_error[] = {
    "null",
    "init lock false",
    "max key len",
    "have no buff",
    "memery error",
    "queue not declare",
    "max bind count",
    "key not declare",
    "unknown error",
    "queue not init",
    "empty queue",
    "pid not register",
    "unknown error !"
};

const char * shm_get_error(int error)
{
    int i = -100 - error;
    if (i<0 && i>11){
        return shm_neuron_error[12];
    }
    return shm_neuron_error[i];
}


typedef struct
{
    unsigned int            m_index;
    volatile unsigned int   m_state;
    unsigned int            m_msg_len;
    unsigned int            m_next_buff;
    char                    m_buff[SHM_MAX_BUFF];
} shm_buff_node ;

typedef struct
{
    char                    m_name[32];
    unsigned int            m_create_time;
    unsigned int            m_node_num;
    unsigned int            m_bind_cnt;
    unsigned int            m_start_index;
    volatile unsigned int   m_set_lock;
    volatile unsigned int   m_get_lock;
    int                     m_bind_list[SHM_MAX_BIND_KEY];
}shm_queue;

typedef struct
{
    char                m_name[32];
    unsigned int        m_create_time;
    unsigned int        m_queue_cnt;
    int                 m_queue_list[SHM_MAX_BIND_KEY];
}shm_key;

#define  SHM_VERSION   1151986

typedef struct
{
    unsigned int    m_version;
    unsigned int    m_init_time;
    unsigned int    m_free_buff_list;
    unsigned int    m_free_buff_count;
    unsigned int    m_node_cnt;
    unsigned int    m_node_list;
    shm_queue       m_queues[SHM_MAX_QUEUE];
    shm_key         m_keys[SHM_MAX_QUEUE];
}shm_cell_head;

char* shm_ptr(char* ptr, unsigned int index)
{
    if (!ptr || index<=0){
        return 0;
    }
    index--;
    unsigned int mv = sizeof(shm_cell_head) + SHM_MAX_BLOCK_SIZE*index;
    ptr+=mv;
    return ptr;
}

unsigned int shm_get_buff(char* ptr)
{
    if (!ptr) {return 0;}
    shm_cell_head * head = (shm_cell_head*)ptr;
    if (head->m_free_buff_count){
        shm_buff_node* pe = (shm_buff_node*)shm_ptr(ptr, head->m_free_buff_list);
        unsigned int index = head->m_free_buff_list;
        head->m_free_buff_list = pe->m_next_buff;
        head->m_free_buff_count--;
        return index;
    }
    return 0;
}

int shm_set_buff(char* ptr, unsigned int id)
{
    if (!ptr) {return 0;}
    shm_cell_head * head = (shm_cell_head*)ptr;
    shm_buff_node* pe = (shm_buff_node*)shm_ptr(ptr, id);
    pe->m_next_buff = head->m_free_buff_list;
    head->m_free_buff_list = id;
    head->m_free_buff_count++;
    return 1;
}

int shm_init(char* ptr, unsigned int size)
{
    if (!ptr || size <= sizeof(shm_cell_head)){
        return -1;
    }
    shm_cell_head * head = (shm_cell_head*)ptr;
    if (head->m_version == SHM_VERSION){
        return 0;
    }
    memset(head, 0, sizeof(shm_cell_head));
    head->m_version = SHM_VERSION;
    head->m_init_time = (unsigned int)time(0);
    size -= sizeof(shm_cell_head);
    head->m_free_buff_count = size/SHM_MAX_BLOCK_SIZE;
    head->m_node_cnt = head->m_free_buff_count;
    if (head->m_free_buff_count == 0){
        return -102; //no buff
    }
    head->m_free_buff_list = 1;
    shm_buff_node * prev = (shm_buff_node*)shm_ptr(ptr, head->m_free_buff_list);
    prev->m_index = head->m_free_buff_list;
    prev->m_msg_len = 0;
    prev->m_state = 0;
    prev->m_next_buff = 0;
    unsigned int i=2;
    for (; i<=head->m_free_buff_count; i++)
    {
        shm_buff_node * node = (shm_buff_node*)shm_ptr(ptr, i);
        node->m_index = i;
        node->m_msg_len = 0;
        node->m_state = 0;
        node->m_next_buff = 0;
        if (prev){
            prev->m_next_buff = node->m_index;
        }
        prev = node;
    }
    return 0;
}

shm_cell_head* shm_check_head(char* ptr)
{
    if (!ptr){
        return 0;
    }
    shm_cell_head * head = (shm_cell_head*)ptr;
    if (head->m_version != SHM_VERSION){
        return 0;
    }
    return head;
}

int shm_queue_find(shm_cell_head* head, const char* queue_name)
{
    int i = 0;
    for (; i<SHM_MAX_QUEUE; i++){
        if (head->m_queues[i].m_create_time > 0 &&
            strcmp(head->m_queues[i].m_name, queue_name) == 0){
            return i;
        }
    }
    return -1;
}

int shm_key_find(shm_cell_head* head, const char* name)
{
    int i = 0;
    for (; i<SHM_MAX_QUEUE; i++){
        if (head->m_keys[i].m_create_time > 0 &&
            strcmp(head->m_keys[i].m_name, name) == 0){
            return i;
        }
    }
    return -1;
}

int shm_queue_declare(char* ptr, const char* queue_name, int len, unsigned int size)
{
    if (len<0 ||len>=32){
        return -102; //max len
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int i = shm_queue_find(head, queue_name);
    if (i>=0){
        return i;
    }
    i=0;
    for (; i<SHM_MAX_QUEUE; i++){
        if (head->m_queues[i].m_create_time == 0) {
            head->m_queues[i].m_create_time = time(0);
            head->m_queues[i].m_bind_cnt = 0;
            head->m_queues[i].m_start_index = 0;
            int j = 0;
            for (; j<SHM_MAX_BIND_KEY; j++){
                head->m_queues[i].m_bind_list[j] = -1;
            }
            head->m_queues[i].m_node_num = 0;
            head->m_queues[i].m_get_lock = 0;
            head->m_queues[i].m_set_lock = 0;
            memcpy(head->m_queues[i].m_name, queue_name, len);
            break;
        }
    }
    head->m_queues[i].m_start_index = shm_get_buff(ptr);
    if (head->m_queues[i].m_start_index>0) {
        shm_buff_node * node = (shm_buff_node*)shm_ptr(ptr, head->m_queues[i].m_start_index);
        head->m_queues[i].m_node_num  = 1;
        do{
            node->m_state = 0;
            node->m_msg_len = 0;
            if (head->m_free_buff_count == 0 || size <= 0) {
                break;
            }
            node->m_next_buff = shm_get_buff(ptr);
            if (node->m_next_buff==0){
                break;
            }
            node = (shm_buff_node*)shm_ptr(ptr,node->m_next_buff);
            size -- ;
            head->m_queues[i].m_node_num++;
        }while(1);
        node->m_next_buff = head->m_queues[i].m_start_index;
        head->m_queues[i].m_get_lock = head->m_queues[i].m_start_index;
        head->m_queues[i].m_set_lock = head->m_queues[i].m_start_index;
    }
    return i;
}

int shm_create_key(shm_cell_head * head, const char* key_name, int len)
{
    int i=0;
    for (; i<SHM_MAX_QUEUE; i++){
        if (head->m_keys[i].m_create_time == 0) {
            head->m_keys[i].m_create_time = time(0);
            head->m_keys[i].m_queue_cnt = 0;
            int j = 0;
            for (; j<SHM_MAX_BIND_KEY; j++){
                head->m_keys[i].m_queue_list[j] = -1;
            }
            memcpy(head->m_keys[i].m_name, key_name, len);
            return i;
        }
    }
    return -1;
}

int shm_queue_bind(char* ptr, const char* queue_name, int len, const char* key_name,  int klen)
{
    if (len<0 ||len>=32 || klen < 0 || klen >= 32){
        return -102; //max len
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int qid = shm_queue_find(head, queue_name);
    if (qid <0){
        return -1;
    }
    int kid = shm_key_find(head, key_name);
    if (kid < 0){
        kid = shm_create_key(head, key_name, len);
        if (kid<0){
            return -1;
        }
    }
    int i =0;
    int has = 0;
    int bhas =  0;
    for (; i<SHM_MAX_BIND_KEY; i++){
        if (head->m_queues[qid].m_bind_list[i] == kid){
            has = 1;
        }
        if (head->m_keys[kid].m_queue_list[i] == qid){
            bhas = 1;
        }
    }
    if (!has){
        i=0;
        for (; i<SHM_MAX_BIND_KEY; i++){
            if (head->m_queues[qid].m_bind_list[i]<0){
                head->m_queues[qid].m_bind_list[i] = kid;
                head->m_queues[qid].m_bind_cnt++;
                break;
            }
        }
    }
    if (!bhas){
        i = 0;
        for (; i<SHM_MAX_BIND_KEY; i++){
            if (head->m_keys[kid].m_queue_list[i] < 0){
                head->m_keys[kid].m_queue_list[i] = qid;
                head->m_keys[kid].m_queue_cnt++;
                break;
            }
        }
    }
    return 0;
}

int shm_queue_unbind(char* ptr, const char* queue_name, int len, const char* key_name, int klen)
{
    if (len<0 ||len>=32 || klen < 0 || klen >= 32){
        return -102; //max len
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int qid = shm_queue_find(head, queue_name);
    if (qid <0){
        return -1;
    }
    int kid = shm_key_find(head, key_name);
    if (kid < 0){
        return -1;
    }
    int i=0;
    for (; i<SHM_MAX_BIND_KEY; i++)
    {
        if (head->m_queues[qid].m_bind_list[i] == kid){
            head->m_queues[qid].m_bind_list[i] = -1;
            head->m_queues[qid].m_bind_cnt--;
            break;
        }
    }
    i=0;
    for (; i<SHM_MAX_BIND_KEY; i++)
    {
        if (head->m_keys[kid].m_queue_list[i] == qid){
            head->m_keys[kid].m_queue_list[i] = -1;
            head->m_keys[kid].m_queue_cnt --;
            break;
        }
    }
    return 0;
}

int  shm_queue_delete(char* ptr, const char* queue, int qlen)
{
    if (qlen<0 ||qlen>=32){
        return -1;
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int qid = shm_queue_find(head, queue);
    if (qid <0){
        return -1;
    }
    do{
        int i = 0;
        int cnt = head->m_queues[qid].m_node_num;
        for (; i <cnt; i++){
            shm_buff_node * snode = (shm_buff_node*)shm_ptr(ptr, head->m_queues[qid].m_start_index);
            head->m_queues[qid].m_start_index = snode->m_next_buff;
            shm_set_buff(ptr, snode->m_index);
        }
        head->m_queues[qid].m_node_num = 0;
        head->m_queues[qid].m_create_time = 0;
        memset(head->m_queues[qid].m_name, 0, 32);
        head->m_queues[qid].m_get_lock = 0;
        head->m_queues[qid].m_set_lock = 0;
        head->m_queues[qid].m_bind_cnt = 0;
        head->m_queues[qid].m_start_index = 0;
        i = 0;
        for (; i<SHM_MAX_BIND_KEY; i++){
            if (head->m_queues[qid].m_bind_list[i] >= 0){
                int kid = head->m_queues[qid].m_bind_list[i];
                int ii = 0;
                for (; ii<SHM_MAX_BIND_KEY; ii++)
                {
                    if (head->m_keys[kid].m_queue_list[ii] == qid)
                    {
                        head->m_keys[kid].m_queue_list[ii] = -1;
                        head->m_keys[kid].m_queue_cnt --;
                    }
                }
            }
            head->m_queues[qid].m_bind_list[i] = -1;
        }
    }while(0);
    return 0;
}

int  shm_queue_clear(char* ptr, const char* queue, int qlen)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int q_id = shm_queue_find(head, queue);
    char * buff = (char*)malloc(1024*1024*32);
    if (!buff){
        return -1;
    }
    while(1){
        int rt = shm_queue_pop_cas_by_id(ptr, q_id, buff, 1024*1024*4);
        if (rt < 0){
            break;
        }
    }
    free(buff);
    return 1;
}

int shm_queue_push_cas_by_id(char* ptr, unsigned int qid, const char* data, int len)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head || qid >= SHM_MAX_QUEUE){
        return -1;
    }
    if (head->m_queues[qid].m_create_time == 0){
        return -2;
    }
    unsigned int num = len/SHM_MAX_BUFF + 1;
    do{
        unsigned int _set = head->m_queues[qid].m_set_lock;
        unsigned int _get = head->m_queues[qid].m_get_lock;
        shm_buff_node * snode = (shm_buff_node*)shm_ptr(ptr, _set);
        if (snode->m_state != 0) {
            return -103;
        }
        unsigned int i=1;
        shm_buff_node * cnode = snode;
        for (; i< num; i++){
            cnode = (shm_buff_node*)shm_ptr(ptr, cnode->m_next_buff);
            if (cnode->m_state != 0 || cnode->m_index == _get){
                return -103;
            }
        }
        if (!__sync_bool_compare_and_swap(&(head->m_queues[qid].m_set_lock), _set, cnode->m_next_buff)){
            continue;
        }
        unsigned int slen  = len;
        cnode = snode;
        i = 0;
        for (; i<num; i++)
        {
            cnode->m_msg_len = len;
            if (slen > SHM_MAX_BUFF)
            {
                memcpy(cnode->m_buff, data, SHM_MAX_BUFF);
                data += SHM_MAX_BUFF;
                slen -= SHM_MAX_BUFF;
            }
            else
            {
                memcpy(cnode->m_buff, data, slen);
                break;
            }
            cnode = (shm_buff_node*)shm_ptr(ptr, cnode->m_next_buff);
        }
        if (!__sync_bool_compare_and_swap(&(snode->m_state), 0, 1)){
            snode->m_state = 1;
        }
        return len;
    }while(0);
    return 0;
}

int shm_queue_push_cas(char* ptr, const char* queue, int qlen, const char* data, int len)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int id = shm_queue_find(head, queue);
    if (id<0){
        return -1;
    }
    return shm_queue_push_cas_by_id(ptr, id, data, len);
}

int shm_queue_pop_cas_by_id(char* ptr, unsigned int qid, char* data, int max)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head || qid >= SHM_MAX_QUEUE){
        return -1;
    }
    if (head->m_queues[qid].m_create_time == 0){
        return -2;
    }
    int rt = 0;
    do{
        unsigned int _get = head->m_queues[qid].m_get_lock;
        shm_buff_node * snode = (shm_buff_node*)shm_ptr(ptr, _get);
        if (snode->m_state == 0){
            return -110; //no message
        }
        if (snode->m_msg_len > (unsigned int)max){
            return -103; //no buff
        }
        int num = snode->m_msg_len/SHM_MAX_BUFF + 1;  //
        shm_buff_node * cnode = snode;
        int i=1;
        for (; i<num; i++){
            cnode = (shm_buff_node*)shm_ptr(ptr, cnode->m_next_buff);
        }
        unsigned int _new = cnode->m_next_buff;
        if ( !__sync_bool_compare_and_swap( &(head->m_queues[qid].m_get_lock), _get, _new) ) {
            continue;
        }
        cnode = snode;
        unsigned int len = snode->m_msg_len;
        rt = len;
        char * cp = data;
        i = 0;
        for (; i<num; i++)
        {
            if (len >SHM_MAX_BUFF)
            {
                memcpy(cp, cnode->m_buff, SHM_MAX_BUFF);
                cp += SHM_MAX_BUFF;
                len -= SHM_MAX_BUFF;
            }
            else
            {
                if (len < 0){
                    return -104;
                }
                if (len >0){
                    memcpy(cp, cnode->m_buff, len);
                    len = 0;
                    cp += len;
                }
            }
            cnode = (shm_buff_node*)shm_ptr(ptr, cnode->m_next_buff);
        }
        if (!__sync_bool_compare_and_swap(&(snode->m_state), 1, 0))
        {
            snode->m_state = 0;
        }
    }while(0);
    return rt;
}

int shm_queue_pop_cas(char* ptr, const char* queue, int qlen, char* data, int max)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int id = shm_queue_find(head, queue);
    if (id<0){
        return -1;
    }
    return shm_queue_pop_cas_by_id(ptr, id, data, max);
}

int shm_publish_message_by_id(char* ptr, unsigned int kid, const char* data, int len)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head || kid >= SHM_MAX_QUEUE){
        return -1;
    }
    int rt = 1;
    unsigned int cc = 0;
    unsigned int i=0;
    for (; i<SHM_MAX_BIND_KEY; i++)
    {
        if (head->m_keys[kid].m_queue_list[i]>=0){
            unsigned int qid = (unsigned)head->m_keys[kid].m_queue_list[i];
            int rs = shm_queue_push_cas_by_id(ptr, qid, data, len);
            if (rs<-1){
                rt = 0;
            }
            cc++;
        }
        if (cc>= head->m_keys[kid].m_queue_cnt){
            break;
        }
    }
    return rt;
}

int shm_publish_message(char* ptr, const char* key, int klen, const char* data, int len)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int kid = shm_key_find(head, key);
    if (kid<0){
        return -1;
    }
    return shm_publish_message_by_id(ptr, kid, data, len);
}

