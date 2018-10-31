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
#define  SHM_MAX_NODE           (SHM_PE*16)
#define  SHM_MAX_PE             (SHM_MAX_BLOCK_SIZE/SHM_MAX_NODE)
#define  SHM_MAX_BUFF           (SHM_MAX_BLOCK_SIZE - (3*SHM_PE))
#define  SHM_MAX_MEMERY         (512*1024*1024)
#define  SHM_MAX_BIND_KEY       128
#define  SHM_SET_LOCK           100000000
#define  SHM_MAX_PIDS           (SHM_MAX_BLOCK_SIZE/(2*SHM_PE))

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
    volatile unsigned int    m_index;
    unsigned int    m_msg_len;
    unsigned int    m_next_buff;
    char            m_buff[SHM_MAX_BUFF];
} shm_buff_node ;

typedef struct
{
    unsigned int    m_next_node;
    unsigned int    m_cell_index;
} shm_list_64 ;

typedef struct
{
    unsigned int    m_pid;
    volatile unsigned int    m_work; //init 0 1 stop 10 start
    unsigned int    m_time;
} shm_pid ;

/*
class LOCK_INFOS
{
public:
    LOCK_INFOS()
    {
        _THREAD_INIT(&mutex);
    };
    ~LOCK_INFOS()
    {
        _THREAD_LEAVE(&mutex);
    };
    _THREAD_ACTION mutex;
    unsigned int  LockPID;
    time_t  LockTime;
};*/

typedef struct 
{
    pthread_mutex_t mutex;
    unsigned int  LockPID;
    time_t  LockTime;
}LOCK_INFOS;

/****************************************二级*****************************************/
typedef struct
{
    char                    m_name[32]; //32
    unsigned int            m_version;  
    unsigned int            m_creat_time;
    unsigned int            m_node_num;
    unsigned int            m_pid_num:16;
    unsigned int            m_bind_cnt:12;
    unsigned int            m_status:4;
    unsigned int            m_self_index;
    volatile unsigned int   m_set_index;
    volatile unsigned int   m_get_index;
    LOCK_INFOS              m_clock;
    shm_pid                 m_pids[8];
    unsigned int            m_bind_list[SHM_MAX_BIND_KEY]; //queue list or key list
} shm_cell_64;

/***************************************一级******************************************/
typedef struct
{
    unsigned int    m_neuron[16];
} shm_node_64;

#define  SHM_VERSION   1151986

typedef struct
{
    LOCK_INFOS      m_Lock;
    unsigned int    m_version;
    unsigned int    m_init_time;
    unsigned int    m_free_buff_list;
    unsigned int    m_free_buff_count;
    unsigned int    m_node_cnt;
    unsigned int    m_node_list;
    shm_node_64     m_queues_list;       //queue name -> key list
    shm_node_64     m_keys_list;         //key name -> queue names
} shm_cell_head;

//index start for 1
char* shm_ptr(char* ptr, unsigned int index)
{
    if (!ptr || index<=0){
        return 0;
    }
    index--;
    unsigned int mv = sizeof(shm_cell_head) + SHM_MAX_NODE*index;
    ptr+=mv;
    return ptr;
}

unsigned int shm_get_node(char* ptr)
{
    if (!ptr) {return 0;}
    shm_cell_head * head = (shm_cell_head*)ptr;
    if (0==head->m_node_list && head->m_free_buff_count)
    {
        unsigned int old = head->m_free_buff_list;
        shm_buff_node* pe = (shm_buff_node*)shm_ptr(ptr, head->m_free_buff_list);
        head->m_free_buff_count--;
        head->m_node_list = old;
        head->m_free_buff_list = pe->m_next_buff;
        char *p =(char*)pe;
        unsigned int num = SHM_MAX_BLOCK_SIZE/SHM_MAX_NODE;
        memset(p, 0, sizeof(shm_buff_node));
        unsigned int i=0;
        for (; i<num; i++)
        {
            shm_list_64 *n = (shm_list_64*)p;
            n->m_next_node = 0;
            n->m_cell_index = 0;
            p += SHM_MAX_NODE;
            if (i<(num-1)){
                n->m_next_node = old+i+1;
            }
        }
    }
    if (0==head->m_node_list){return 0;}
    unsigned int rt = head->m_node_list;
    shm_list_64 * n = (shm_list_64*)shm_ptr(ptr, head->m_node_list);
    head->m_node_list = n->m_next_node;
    n->m_next_node = 0;
    return rt;
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
    
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr,PTHREAD_PROCESS_SHARED);
    if (pthread_mutex_init( &(head->m_Lock.mutex), &mutexattr ) < 0){
        return -101;  //初始化锁失败
    }
    
    head->m_version = SHM_VERSION;
    head->m_init_time = (unsigned int)time(0);
    head->m_free_buff_list = 0;
    head->m_node_list = 0;

    size -= sizeof(shm_cell_head);
    head->m_free_buff_count = size/SHM_MAX_BLOCK_SIZE;
    head->m_node_cnt = head->m_free_buff_count;
    unsigned int i=0;
    for (; i<64; i++)
    {
        head->m_queues_list.m_neuron[i] = 0;
        head->m_keys_list.m_neuron[i] = 0;
    }
    
    shm_buff_node * prev = 0;
    i=0;
    for (; i<head->m_free_buff_count; i++)
    {
        if (!head->m_free_buff_list){
            head->m_free_buff_list = i*SHM_MAX_PE+1;
        }
        shm_buff_node * node = (shm_buff_node*)shm_ptr(ptr, i*SHM_MAX_PE+1);
        node->m_index = i*SHM_MAX_PE+1;
        node->m_msg_len = 0;
        node->m_next_buff = 0;
        if (prev){
            prev->m_next_buff = node->m_index;
        }
        prev = node;
    }
    return 0;
}

shm_cell_64* shm_node_declare(char* ptr, const char* name, int len, shm_node_64* pe)
{
    shm_list_64 * ls = 0;
    int i = 0;
    for (; i < len; i++)
    {
        unsigned char uc = name[i];
        unsigned int  pos1 = uc/16;
        unsigned int  pos2 = uc%16;

        if (0 == pe->m_neuron[pos1]){
            pe->m_neuron[pos1] = shm_get_node(ptr);
            if (0 == pe->m_neuron[pos1]){
                return 0;
            }
        }

        shm_node_64 * op = (shm_node_64*)shm_ptr(ptr, pe->m_neuron[pos1]);
        if (!op){
            return 0;
        }

        if (0 == op->m_neuron[pos2]){
            op->m_neuron[pos2] = shm_get_node(ptr);
            if (0 == op->m_neuron[pos2]){
                return 0;
            }
        }

        ls = (shm_list_64*)shm_ptr(ptr, op->m_neuron[pos2]);
        if (!ls){
            return 0;
        }

        if (0 == ls->m_next_node && (i+1) < len){
            ls->m_next_node = shm_get_node(ptr);
            if (0 == ls->m_next_node){
                return 0;
            }
        }
        
        pe = (shm_node_64*)shm_ptr(ptr, ls->m_next_node);
        if (!pe && (i+1) < len){
            return 0;
        }
    }
    shm_cell_64* so = 0;
    if (!ls->m_cell_index){
        ls->m_cell_index = shm_get_buff(ptr);
        so = (shm_cell_64*)shm_ptr(ptr, ls->m_cell_index);
        if (so){
            so->m_version = 0;
            so->m_self_index = ls->m_cell_index;
            so->m_bind_cnt = 0;
            memset(so->m_name, 0, 32);
            memcpy(so->m_name, name, len);
            i=0;
            for (; i<SHM_MAX_BIND_KEY; i++)
            {
                so->m_bind_list[i] = 0;
            }
        }
    }else{
        so = (shm_cell_64*)shm_ptr(ptr, ls->m_cell_index);
    }
    return so;
}

shm_cell_64* shm_node_find(char* ptr, const char* name, int len, shm_node_64* pe)
{
    shm_list_64 * ls = 0;
    int i=0;
    for (; i < len; i++)
    {
        unsigned char uc = name[i];
        unsigned int  pos1 = uc/16;
        unsigned int  pos2 = uc%16;

        if (0 == pe->m_neuron[pos1]){
            return 0;
        }

        shm_node_64 * op = (shm_node_64*)shm_ptr(ptr, pe->m_neuron[pos1]);
        if (!op){return 0;}

        if (0 == op->m_neuron[pos2]){
            return 0;
        }

        ls = (shm_list_64*)shm_ptr(ptr, op->m_neuron[pos2]);
        if (!ls){return 0;}
        if (0 == ls->m_next_node && (i+1) < len){
            return 0;
        }
        pe = (shm_node_64*)shm_ptr(ptr, ls->m_next_node);
        if (!pe && (i+1) < len ){return 0;}
    }
    shm_cell_64* so = 0;
    if (ls && ls->m_cell_index){
        so = (shm_cell_64*)shm_ptr(ptr, ls->m_cell_index);
    }
    return so;
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
/*
void shm_test_queue(char* ptr, const char* queue_name, int len)
{
    if (len<0 ||len>=32){
        LOG(ERROR)<<"queue_name len:"<<len<<" error";
        return ;
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        LOG(ERROR)<<"ptr null";
        return ;
    }
    shm_node_64 * pe = &(head->m_queues_list);
    shm_cell_64 * so = shm_node_find(ptr, queue_name, len, pe);
    if (!so){
        LOG(ERROR)<<"not find queue:"<<queue_name;
        return ;
    }
    
    LOG(ERROR)<<"size shm_cell_64:"<<sizeof(shm_cell_64);
    LOG(ERROR)<<"m_version:"<<so->m_version; 
    LOG(ERROR)<<"m_self_index:"<<so->m_self_index;
    LOG(ERROR)<<"m_creat_time:"<<so->m_creat_time;
    LOG(ERROR)<<"m_node_num:"<<so->m_node_num;
    LOG(ERROR)<<"m_set_index:"<<so->m_set_index;
    LOG(ERROR)<<"m_get_index:"<<so->m_get_index;
    shm_buff_node * node = (shm_buff_node*)shm_ptr(ptr, so->m_set_index);
    for (unsigned int i=0; i< so->m_node_num; i++)
    {
        LOG(ERROR)<<"i:"<<i<<" index:"<<node->m_index<<" next:"<<node->m_next_buff;
        node = (shm_buff_node*)shm_ptr(ptr, node->m_next_buff);
    }
}
*/

int shm_queue_declare(char* ptr, const char* queue_name, int len, unsigned int size)
{
    if (len<0 ||len>=32){
        return -102; //max len
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    pthread_mutex_lock(&(head->m_Lock.mutex));
    int rt = 0;
    do{
        shm_node_64 * pe = &(head->m_queues_list);
        shm_cell_64 * so = shm_node_declare(ptr, queue_name, len, pe);
        if (!so){
            rt = -103;
            break;
        }

        if (so->m_version != SHM_VERSION)
        {
            so->m_version = SHM_VERSION;
            so->m_creat_time = time(0);
            so->m_node_num = 0;
            so->m_bind_cnt = 0;
            so->m_status = 0;
            int i=0;
            for (; i<SHM_MAX_BIND_KEY;i++){
                so->m_bind_list[i] = 0;
            }
            so->m_get_index = 0;
            so->m_set_index = 0;
        }
        if (so->m_status >= 1)
        {
            rt = 1;
            break;
        }
        so->m_status = 1;
        if (size > head->m_free_buff_count){
            rt = -103; //no buff
            break;
        }    
        so->m_node_num = size;
        so->m_get_index = shm_get_buff(ptr);
        shm_buff_node * node = (shm_buff_node*)shm_ptr(ptr, so->m_get_index);
        if (!node){
            rt = -104;
            break;
        }
        node->m_index = 0;
        node->m_next_buff = shm_get_buff(ptr);
        if (0==node->m_next_buff){
            rt = -104;
            break;
        }
        so->m_set_index = node->m_next_buff;
        node = (shm_buff_node*)shm_ptr(ptr, so->m_set_index);
        if (!node){
            rt = -104;
            break;
        }
        node->m_index = 1;
        so->m_pid_num = 0;
        int i=0;
        for (; i<8; i++){
            so->m_pids[i].m_pid = 0;
            so->m_pids[i].m_work = 0;
            so->m_pids[i].m_time = 0;
        }
        size++;
        unsigned int ii=2;
        for (; ii<size; ii++)
        {
            node->m_next_buff = shm_get_buff(ptr);
            if (0==node->m_next_buff){
                rt = -104;
                break;
            }
            node = (shm_buff_node*)shm_ptr(ptr, node->m_next_buff);
            if (!node){
                rt = -104;
                break;
            }
            node->m_index = ii;
        }
        node->m_next_buff = so->m_get_index;
    }while(0);
    pthread_mutex_unlock(&(head->m_Lock.mutex));
    return rt;
}

unsigned int shm_queue_id(char* ptr, const char* queue_name, int len)
{
    if (len<0 ||len>=32){
        return 0; //max len
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return 0;
    }
    shm_node_64 * pe = &(head->m_queues_list);
    shm_cell_64 * ls = shm_node_find(ptr, queue_name, len, pe);
    if (!ls){
        return 0; //queue not declare
    }
    return ls->m_self_index;
}

unsigned int shm_key_id(char* ptr, const char* key, int klen)
{
     if (klen<0 ||klen>=32){
        return 0; //max len
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return 0;
    }
    shm_node_64 * pe = &(head->m_keys_list);
    shm_cell_64 * ls = shm_node_find(ptr, key, klen, pe);
    if (!ls){
        return 0;
    }
    return ls->m_self_index;
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
    pthread_mutex_lock(&(head->m_Lock.mutex));
    int rt = 1;
    do{
        shm_node_64 * pe = &(head->m_queues_list);
        shm_cell_64 * ls = shm_node_find(ptr, queue_name, len, pe);
        if (!ls){
            rt = -105; //queue not declare
            break;
        }
        shm_node_64 * ke = &(head->m_keys_list);
        shm_cell_64 * ks = shm_node_declare(ptr, key_name, klen, ke);
        if (!ks){
            rt = -103; //no buff
            break;
        }
        if (ks->m_bind_cnt >= SHM_MAX_BIND_KEY || ls->m_bind_cnt >= SHM_MAX_BIND_KEY){
            rt = -106; //max SHM_MAX_BIND_CNT
            break;
        }

        int has = 0;
        int bhas =  0;
        unsigned int i=0;
        for (; i<SHM_MAX_BIND_KEY; i++)
        {
            if (ls->m_bind_list[i]==ks->m_self_index){
                has = 1;
            }
            if (ks->m_bind_list[i]==ls->m_self_index){
                bhas = 1;
            }
        }

        //bind key
        if (!has)
        {
            i=0;
            for (; i<SHM_MAX_BIND_KEY; i++)
            {
                if (ls->m_bind_list[i]==0){
                    ls->m_bind_list[i]=ks->m_self_index;
                    ls->m_bind_cnt++;
                    break;
                }
            }
        }

        if (!bhas)
        {
            i = 0;
            for (; i<SHM_MAX_BIND_KEY; i++)
            {
                if (ks->m_bind_list[i]==0){
                    ks->m_bind_list[i]=ls->m_self_index;
                    ks->m_bind_cnt++;
                    break;
                }
            }
        }
    }while(0);
    pthread_mutex_unlock(&(head->m_Lock.mutex));
    return rt;
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
    pthread_mutex_lock(&(head->m_Lock.mutex));
    int rt = 1;
    do{
        shm_node_64 * pe = &(head->m_queues_list);
        shm_cell_64 * ls = shm_node_find(ptr, queue_name, len, pe);
        if (!ls){
            rt = -105; //queue not declare
            break;
        }

        shm_node_64 * ke = &(head->m_keys_list);
        shm_cell_64 * ks = shm_node_find(ptr, key_name, klen, ke);
        if (!ks){
            rt = -107; //key not declare
            break;
        }

        //unbind key
        unsigned int i=0;
        for (; i<SHM_MAX_BIND_KEY; i++)
        {
            if (ls->m_bind_list[i]==ks->m_self_index){
                ls->m_bind_list[i]=0;
                ls->m_bind_cnt--;
                break;
            }
        }
        i=0;
        for (; i<SHM_MAX_BIND_KEY; i++)
        {
            if (ks->m_bind_list[i]==ls->m_self_index){
                ks->m_bind_list[i]=0;
                ks->m_bind_cnt--;
                break;
            }
        }
    }while(0);
    pthread_mutex_unlock(&(head->m_Lock.mutex));
    return rt;
}

void shm_notify(shm_cell_64* so)
{
    int cnt = 0;
    int i=0;
    for (; i<8; i++)
    {
        if (so->m_pids[i].m_pid)
        {
            cnt++;
            if (so->m_pids[i].m_work == 2 && kill(so->m_pids[i].m_pid, SIGUSR1) == -1)
            {
                /*
                if (errno == ESRCH){
                    so->m_pids[i].m_pid = 0;
                    so->m_pid_num --;
                }*/
            }
        }
        if (cnt >= so->m_pid_num){
            break;
        }
    }
}

shm_cell_64*  shm_get_cell(char* ptr, const char* queue, int qlen) //, unsigned int& q_id, shm_cell_64 *&so)
{
    if (qlen<=0 ||qlen>=32){
        return 0; //max queue len
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return 0;
    }
    shm_node_64 * pe = &(head->m_queues_list);
    shm_cell_64  *so = shm_node_find(ptr, queue, qlen, pe);
    if (so && (so->m_version != SHM_VERSION || so->m_status == 0) ){
        return 0; //queue not init
    }
    return so;
}

shm_cell_64*  shm_get_cell_id(char* ptr, unsigned int q_id)
{
    shm_cell_64  *so = (shm_cell_64*)shm_ptr(ptr, q_id);
    if (so && (so->m_version != SHM_VERSION || so->m_status == 0) ){
        return 0; //queue not init
    }
    return so;
}

int shm_queue_has_message_id(char* ptr, unsigned int id)
{
    shm_cell_64* so = shm_get_cell_id(ptr,id);
    if (!so)
    {
        return -1;
    }
    shm_buff_node* gnode = (shm_buff_node*)shm_ptr(ptr, so->m_get_index);
    unsigned int gid = gnode->m_index%SHM_SET_LOCK;
    shm_buff_node* snode = (shm_buff_node*)shm_ptr(ptr, so->m_set_index);
    unsigned int sid = snode->m_index%SHM_SET_LOCK;
    unsigned int nmax = so->m_node_num+1;
    unsigned int nfree = (gid - sid + nmax)%nmax;
    unsigned int num = so->m_node_num - nfree;
    return num;
}

int shm_queue_has_message(char* ptr, const char* queue, int qlen)
{
    shm_cell_64* so =  shm_get_cell(ptr, queue, qlen);
    if (!so){
        return -1;
    }
    shm_buff_node* gnode = (shm_buff_node*)shm_ptr(ptr, so->m_get_index);
    unsigned int gid = gnode->m_index%SHM_SET_LOCK;
    shm_buff_node* snode = (shm_buff_node*)shm_ptr(ptr, so->m_set_index);
    unsigned int sid = snode->m_index%SHM_SET_LOCK;
    unsigned int nmax = so->m_node_num+1;
    unsigned int nfree = (gid - sid + nmax)%nmax;
    unsigned int num = so->m_node_num - nfree;
    return num;
}

int  shm_queue_delete(char* ptr, const char* queue, int qlen)
{
    shm_cell_64* so = shm_get_cell(ptr, queue, qlen);
    if (!so){
        return -1;
    }
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }
    int rt = 0;
    pthread_mutex_lock(&(head->m_Lock.mutex));
    do{
        unsigned int i=0;
        for (; i<8; i++)
        {
            if (so->m_pids[i].m_pid)
            {
            }
        }
        so->m_status = 0;
        unsigned int cn = so->m_node_num+1;
        i =0;
        for (; i< cn; i++)
        {
            shm_buff_node * snode = (shm_buff_node*)shm_ptr(ptr, so->m_get_index);
            unsigned int id = so->m_get_index;
            so->m_get_index = snode->m_next_buff;
            shm_set_buff(ptr, id);
        }
        so->m_get_index = 0;
        so->m_set_index = 0;
        so->m_node_num = 0;
        so->m_pid_num = 0;
        i = 0;
        for (; i<8; i++){
            so->m_pids[i].m_pid = 0;
            so->m_pids[i].m_work = 0;
            so->m_pids[i].m_time = 0;
        }
        i = 0;
        for (; i<SHM_MAX_BIND_KEY; i++)
        {
            if (so->m_bind_list[i]){
                shm_cell_64* sk = (shm_cell_64*)shm_ptr(ptr, so->m_bind_list[i]);
                int ii = 0;
                for (; ii<SHM_MAX_BIND_KEY; ii++)
                {
                    if (sk->m_bind_list[ii] == so->m_self_index)
                    {
                        sk->m_bind_list[ii] = 0;
                        sk->m_bind_cnt --;
                    }
                }
                so->m_bind_list[i] = 0;
            }
        }
        so->m_bind_cnt = 0;
    }while(0);
    pthread_mutex_unlock(&(head->m_Lock.mutex));
    return rt;
}

int  shm_queue_clear(char* ptr, const char* queue, int qlen)
{
    char * buff = (char*)malloc(1024*1024*4);
    if (!buff){
        return -1;
    }
    unsigned int q_id = shm_queue_id(ptr, queue, qlen);
    while(1){
        int rt = shm_queue_pop_cas_by_id(ptr, q_id, buff, 1024*1024*4);
        if (rt < 0){
            break;
        }
    }
    free(buff);
    return 1;
}

int shm_queue_push_cas_by_id(char* ptr, unsigned int q_id, const char* data, int len)
{
    shm_cell_64* so = shm_get_cell_id(ptr, q_id);
    if (!so){
        return -1;
    }
    unsigned int num = len/SHM_MAX_BUFF + 1;
    int trycnt = 0;
    do{
        unsigned int _old = so->m_set_index;
        unsigned int _get_old = so->m_get_index;
        shm_buff_node * snode = (shm_buff_node*)shm_ptr(ptr, _old);
        unsigned int _iold = snode->m_index;
        if (_iold > SHM_SET_LOCK)
        {
            //may be set ok, so try one
            if (trycnt >= 18){
                return -103; //can read no buff
            }
            trycnt++;
            continue;
        }
        shm_buff_node * cnode = snode;
        int tty = 0;
        unsigned int i=0;
        for (; i< num; i++)
        {
            if ((cnode->m_index > SHM_SET_LOCK && i> 0) || (cnode->m_next_buff == _get_old))
            {
                //not lock so return
                tty = 1;
                break;
            }
            if ((i+1)<num){
                cnode = (shm_buff_node*)shm_ptr(ptr, cnode->m_next_buff);
            }
        }
        if (tty || !__sync_bool_compare_and_swap(&(so->m_set_index), _old, cnode->m_next_buff))
        {
            //may be swap false, so try more
            if (trycnt >= 18){
                return -103;
            }
            trycnt++;
            continue; //lock false
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
        if (!__sync_bool_compare_and_swap(&(snode->m_index), _iold, _iold + SHM_SET_LOCK))
        {
            snode->m_index = _iold + SHM_SET_LOCK;
        }
        shm_notify(so);
        return len;
    }while(1);
    return 0;
}

int shm_queue_push_cas(char* ptr, const char* queue, int qlen, const char* data, int len)
{
    unsigned int id = shm_queue_id(ptr, queue, qlen);
    if (0 == id){
        return -1;
    }
    return shm_queue_push_cas_by_id(ptr, id, data, len);
}

int shm_queue_pop_cas_by_id(char* ptr, unsigned int q_id, char* data, int max)
{
    shm_cell_64* so = shm_get_cell_id(ptr, q_id);
    if (!so){
        return -1;
    }
    int rt = 0;
    do{
        unsigned int _old = so->m_get_index;
        shm_buff_node * rnode = (shm_buff_node*)shm_ptr(ptr, _old);
        unsigned int _new = rnode->m_next_buff;
        rnode =  (shm_buff_node*)shm_ptr(ptr, rnode->m_next_buff); // move 1 indx
        if (rnode->m_msg_len > (unsigned int)max)
        {
            return -103; // no buff
        }
        unsigned int _iold = rnode->m_index;
        if (_iold < SHM_SET_LOCK)
        {
            return -110; 
        }
        unsigned int num = rnode->m_msg_len/SHM_MAX_BUFF + 1;  // 
        shm_buff_node * cnode = rnode;
        unsigned int i=0;
        for (; i<num; i++)
        {
            if ((i+1)<num){
                _new = cnode->m_next_buff;
                cnode = (shm_buff_node*)shm_ptr(ptr, cnode->m_next_buff);
            }
        }
        if (!__sync_bool_compare_and_swap(&(so->m_get_index), _old, _new))
        {
            //continue;
            return -110; //lock false so no message
        }
        cnode = rnode;
        unsigned int len = rnode->m_msg_len;
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
        if (!__sync_bool_compare_and_swap(&(rnode->m_index), _iold, _iold%SHM_SET_LOCK))
        {
            rnode->m_index = _iold%SHM_SET_LOCK;
        }
        so->m_status = 1; //获取到消息还原设置.
        return rt;
    }while(1);
    return rt;
}

int shm_queue_pop_cas(char* ptr, const char* queue, int qlen, char* data, int max)
{
    unsigned int id = shm_queue_id(ptr, queue, qlen);
    if (0 == id){
        return -1;
    }
    return shm_queue_pop_cas_by_id(ptr, id, data, max);
}

int shm_publish_message_by_id(char* ptr, unsigned int key_id, const char* data, int len)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){
        return -1;
    }

    int rt = 1;
    shm_cell_64 * ls = (shm_cell_64*)shm_ptr(ptr, key_id);
    if (!ls){
        return -1;
    }
    unsigned int cc = 0;
    unsigned int i=0;
    for (; i<SHM_MAX_BIND_KEY; i++)
    {
        if (ls->m_bind_list[i]){
            unsigned int q_id = ls->m_bind_list[i];
            int rs = shm_queue_push_cas_by_id(ptr, q_id, data, len);
            if (rs<-1){
                rt = 0;
            }
            cc++;
        }
        if (cc>= ls->m_bind_cnt){
            break;
        }
    }
    return rt;
}

int shm_publish_message(char* ptr, const char* key, int klen, const char* data, int len)
{
    unsigned int kid = shm_key_id(ptr, key, klen);
    if (0==kid){
        return -1;
    }
    return shm_publish_message_by_id(ptr, kid, data, len);
}

/*
int shm_proce_pid(shm_cell_64* so, unsigned int pid, bool set)
{
    bool has = false;
    for (unsigned int i=0; i<8; i++)
    {
        if (so->m_pids[i].m_pid)
        {
            if (kill(so->m_pids[i].m_pid,0) == -1)
            {
                if (errno == EPERM){
                    LOG(ERROR)<<"pid:"<<so->m_pids[i].m_pid<<" not have power to signal";
                }
                if (errno == ESRCH){
                    LOG(ERROR)<<"pid:"<<so->m_pids[i].m_pid<<" empty signal";
                    so->m_pids[i].m_pid = 0;
                    so->m_pid_num--;
                }
            }
        }
        if (so->m_pids[i].m_pid == pid){
            so->m_pids[i].m_work = 0;
            if (!set){
                so->m_pids[i].m_pid = 0;
                so->m_pid_num--;
            }
            has = true;
        }
    }
    if (has || !set){ return 1; }

    for (unsigned int i=0; i<8; i++)
    {
        if (so->m_pids[i].m_pid == 0){
            so->m_pids[i].m_pid = pid;
            so->m_pids[i].m_work = 0;
            so->m_pid_num++;
            return 1;
        }
    }
    return 0;
}

int shm_register_pid(char* ptr, unsigned int& qid,  unsigned int pid)
{
    shm_cell_64* so = 0;
    int rt = shm_get_cell(ptr, "1", 1, qid, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_proce_pid(so, pid, true);
}

int shm_register_pid(char* ptr, const char* queue, int qlen,  unsigned int pid)
{
    shm_cell_64* so = 0;
    unsigned int q_id = 0;
    int rt = shm_get_cell(ptr, queue, qlen, q_id, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_proce_pid(so, pid, true);
}

int shm_unregister_pid(char* ptr, unsigned int& qid, unsigned int pid)
{
    shm_cell_64* so = 0;
    int rt = shm_get_cell(ptr, "1", 1, qid, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_proce_pid(so, pid, false);
}

int shm_unregister_pid(char* ptr, const char* queue, int qlen, unsigned int pid)
{
    shm_cell_64* so = 0;
    unsigned int q_id = 0;
    int rt = shm_get_cell(ptr, queue, qlen, q_id, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_proce_pid(so, pid, false);
}

int shm_pid_to_work(shm_cell_64* so, unsigned int pid)
{
    for (unsigned int i=0; i<8; i++)
    {
        if (kill(so->m_pids[i].m_pid,0) == -1)
        {
            if (errno == EPERM){
                LOG(ERROR)<<"pid:"<<so->m_pids[i].m_pid<<" not have power to signal";
            }
            if (errno == ESRCH){
                LOG(ERROR)<<"pid:"<<so->m_pids[i].m_pid<<" empty signal";
                so->m_pids[i].m_pid = 0;
                so->m_pid_num--;
            }
        }
        if (so->m_pids[i].m_pid == pid)
        {
            so->m_pids[i].m_work = 1;
            so->m_pids[i].m_time=time(0);
            break;
        }
    }
    return 0;
}

int shm_pid_work(char* ptr, unsigned int& qid, unsigned int pid)
{
    shm_cell_64* so = 0;
    int rt = shm_get_cell(ptr, "1", 1, qid, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_pid_to_work(so, pid);
}

int shm_pid_work(char* ptr, const char* queue, int qlen, unsigned int pid)
{
    shm_cell_64* so = 0;
    unsigned int q_id = 0;
    int rt = shm_get_cell(ptr, queue, qlen, q_id, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_pid_to_work(so, pid);
}

int shm_pid_to_stop(shm_cell_64* so, unsigned int pid)
{
    for (unsigned int i=0; i<8; i++)
    {
        if (so->m_pids[i].m_pid == pid){
            LOG(WARNING)<<"shm_pid_to_stop:"<<pid;
            so->m_pids[i].m_work = 2;
        }
    }
    return 1; //stop ok
}

int shm_pid_stop(char* ptr, unsigned int& qid, unsigned int pid)
{
    shm_cell_64* so = 0;
    int rt = shm_get_cell(ptr, "1",  1, qid, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_pid_to_stop(so, pid);
}

int shm_pid_stop(char* ptr, const char* queue, int qlen, unsigned int pid)
{
    shm_cell_64* so = 0;
    unsigned int q_id = 0;
    int rt = shm_get_cell(ptr, queue, qlen, q_id, so);
    if (rt<0)
    {
        return rt;
    }
    return shm_pid_to_stop(so, pid);
}

void shm_find_queue_id(char* ptr, shm_node_64* pe, unsigned int* id, int & ind, int max)
{
    for (int i=0; i<16; i++)
    {
        if (pe->m_neuron[i])
        {
            shm_node_64 * op = (shm_node_64*)shm_ptr(ptr, pe->m_neuron[i]);
            for (int j=0; j<16; j++)
            {
                if (op->m_neuron[j])
                {
                    shm_list_64* ls  = (shm_list_64*)shm_ptr(ptr, op->m_neuron[j]);
                    if (ls->m_cell_index)
                    {
                        shm_cell_64* so = (shm_cell_64*)shm_ptr(ptr, ls->m_cell_index);
                        if (ind<max && so->m_status >0){
                            id[ind] = ls->m_cell_index;
                            ind++;
                        }
                    }
                    if (ls->m_next_node){
                        shm_node_64* cp = (shm_node_64*)shm_ptr(ptr, ls->m_next_node);
                        shm_find_queue_id(ptr, cp, id, ind, max);
                    }
                }
            }
        }
    }
}

int  shm_find_queue_ids(char* ptr, unsigned int * id, int max)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){return -1; }
    int ind = 0;
    shm_find_queue_id(ptr, &(head->m_queues_list), id, ind, max);
    return ind;
}

int  shm_get_queue_info(char* ptr, unsigned int id, shm_q_info & info)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){return -1; }
    shm_cell_64* so = (shm_cell_64*)shm_ptr(ptr, id);
    if (!so)
    {
        return -1;
    }
    info.m_id = id;
    info.m_time = so->m_creat_time;
    for (int i=0; i<32; i++){
        info.m_bind[i] = so->m_bind_list[i];
    }
    for (int i=0; i<8; i++){
        info.m_pids[i] = so->m_pids[i].m_pid;
    }
    info.m_node_num = so->m_node_num;
    shm_buff_node* gnode = (shm_buff_node*)shm_ptr(ptr, so->m_get_index);
    unsigned int gid = gnode->m_index%SHM_SET_LOCK;
    shm_buff_node* snode = (shm_buff_node*)shm_ptr(ptr, so->m_set_index);
    unsigned int sid = snode->m_index%SHM_SET_LOCK;
    if (info.m_node_num){
        info.m_free_num = (gid - sid + info.m_node_num+1)%(info.m_node_num+1);
    }else{
        info.m_free_num = 0;
    }
    memcpy(info.m_name, so->m_name, 32);
    return 0;
}

int  shm_get_key_info(char* ptr, unsigned int id, shm_q_info & info)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){return -1; }
    shm_cell_64* so = (shm_cell_64*)shm_ptr(ptr, id);
    if (!so)
    {
        return -1;
    }
    info.m_id = id;
    memcpy(info.m_name, so->m_name, 32);
    return 0;
}

int  shm_get_shm_info(char* ptr, shm_q_info & info)
{
    shm_cell_head * head = shm_check_head(ptr);
    if (!head){return -1; }if (!head){return -1; }
    info.m_node_num = head->m_node_cnt;
    info.m_free_num = head->m_free_buff_count;
    return 0;
}*/



