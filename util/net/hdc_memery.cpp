#include "hdc_memery.h"

void* ShmPorint(const char* key_id, size_t size){
    unsigned int id = StrToInt32(key_id);
    int Memory = shmget ( id, 0, 0 );
    if (Memory < 0){
        if (errno == ENOENT){
            Memory = shmget (id, size, IPC_CREAT|0666);
        }
        if (Memory < 0){
            return 0;
        }
    }
    void* ptr = shmat(Memory,0,0);
    if (ptr == (void*)-1){
        return 0;
    }
    return ptr;
}


