#include "shmq_handler.h"

ShmqHandler::ShmqHandler(ShmMessageCallBack* back): m_ptr(0), m_back(back), m_exit(0)
{
}

ShmqHandler::~ShmqHandler()
{
    m_exit = 1;
}

bool ShmqHandler::InitMemery(const std::string &key, size_t size)
{
    m_ptr = (char*)ShmPorint(key.c_str(), size);
    if (!m_ptr){
        return false;
    }
    if (shm_init(m_ptr, size)<0){
        return false;
    }
    return true;
}

bool ShmqHandler::AddQueue(const std::string &queue, uint32_t max_size, std::vector<std::string> &keys)
{
    ShmqOp * op = new ShmqOp(queue);
    if (shm_queue_declare(m_ptr, queue.c_str(), queue.length(), max_size, op->id)<0)
    {
        delete op;
        return false;
    }
    for (size_t j=0; j<keys.size(); j++){
        if (shm_queue_bind(m_ptr, queue.c_str(), queue.length(), keys[j].c_str(), keys[j].length())<0){
            delete op;
            return false;
        }
    }
    m_qs.push_back(op);
    return true;
}

bool ShmqHandler::Publish(const std::string & key, const std::string & replyto, const std::string &correlation_id, std::string & msg)
{
    if (!m_ptr){
        return false;
    }
    //1. routing_key, 2. correlation_id, 3. reply_to, 4. delivery_mode
    uint8_t delivery_mode = 0;
    msg.insert(0, reinterpret_cast<const char*>(&delivery_mode), sizeof(uint8_t));

    uint32_t len = replyto.length();
    if (len > 0){
        msg.insert(0, reinterpret_cast<const char*>(replyto.c_str()), len);
    }
    msg.insert(0, reinterpret_cast<const char*>(&len), sizeof(uint32_t));

    len = correlation_id.length();
    if (len>0){
        msg.insert(0, reinterpret_cast<const char*>(correlation_id.c_str()), len);
    }
    msg.insert(0, reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    len = key.length();
    if (len>0){
        msg.insert(0, reinterpret_cast<const char*>(key.c_str()), len);
    }
    msg.insert(0, reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    std::lock_guard<std::mutex> lock(m_lock);
    std::map<std::string, unsigned int>::iterator it = m_key_ids.find(key);
    if (it != m_key_ids.end())
    {
        if (shm_publish_message((char*)m_ptr, key.c_str(), key.length(), m_key_ids[key], msg.c_str(), msg.length())<0){
            return false;
        }
    }else{
        unsigned int id = 0;
        int rt = shm_publish_message((char*)m_ptr, key.c_str(), key.length(), id, msg.c_str(), msg.length());
        if (rt < 0){
            return false;
        }
        if (id>0){
            m_key_ids[key] = id;
        }
    }
    return true;
}

void ShmqHandler::doConsume()
{
    if (m_exit){
        return;
    }
    if (!m_buff.buff){
        m_buff.Broaden();
    }
    for (size_t i=0; i < m_qs.size(); i++)
    {
        do{
            int rt = shm_queue_pop_cas(m_ptr, m_qs[i]->queue.c_str(),
                    m_qs[i]->queue.size(),
                    m_qs[i]->id,
                    m_buff.buff, m_buff.size);
            if (rt <= 0){
                if (rt == -103){
                    m_buff.Broaden();
                    continue;
                }
                if (rt != -110){
                }
                break;
            }
            parse(m_buff.buff, rt, std::string(m_qs[i]->queue.c_str(), m_qs[i]->queue.size()));
        }while(true);
    } 
}

void ShmqHandler::parse(char* buff, size_t len, const std::string & queue)
{
    std::string routing_key;
    std::string reply_to;
    std::string correlation_id;
    std::string body;
    uint32_t * klen = (uint32_t*)buff;
    buff += sizeof(uint32_t);
    len -= sizeof(uint32_t);
    if (*klen > 0)
    {
        routing_key.append(buff, *klen);
        buff += *klen;
        len -= *klen;
    }
    if (len<0){
        return;
    }
    klen = (uint32_t*)buff;
    buff += sizeof(uint32_t);
    len -= sizeof(uint32_t);
    if (*klen > 0)
    {
        correlation_id.append(buff, *klen);
        buff += *klen;
        len -= *klen;
    }
    if (len<0){
        return;
    }
    klen = (uint32_t*)buff;
    buff += sizeof(uint32_t);
    len -= sizeof(uint32_t);
    if (*klen > 0){
        reply_to.append(buff, *klen);
        buff += *klen;
        len -= *klen;
    }
    buff += sizeof(uint8_t);
    len -= sizeof(uint8_t);
    body.append(buff, len);
    if (m_back){
        m_back->OnMessage(Sid(), routing_key, reply_to, correlation_id, body);
    }
}

