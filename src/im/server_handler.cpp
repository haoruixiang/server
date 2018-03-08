#include "server_handler.h"

void NetServerHandler::DelSession(ConnSession* session, uint64_t id, int fd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::map <uint32_t, std::map<uint64_t, int> >::iterator iter = m_sessions.find(session->Uid());
    if (iter != m_sessions.end())
    {
        if (iter->second.empty())
        {
            m_sessions.erase(iter);
        }
        else
        {
            std::map<uint64_t, int>::iterator it = iter->second.find(id);
            if (it != iter->second.end())
            {
                iter->second.erase(it);
            }
        }
    }
}

void NetServerHandler::SetSessionRoute(uint32_t uid, ConnSession* session, uint64_t id, int fd)
{
    session->SetLogin(uid);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions[uid][id]=fd;
}

void NetServerHandler::SendToClientLoop(int fd, uint64_t id, const char* buff, size_t len)
{
    std::string m = std::string(buff, len);
    m_net.SendMsg(m, id, fd);
}

size_t NetServerHandler::Process(uint32_t tid, AsyncConn* conn, const char* buff, size_t len)
{
    int rsize = 0;
    int bHeart = 0;
    do{
        if (len< sizeof(MsgHead)){
            break;
        }
        MsgHead * ph = (MsgHead*)buff;
        if (ph->m_type == IM_HEART){
            rsize += sizeof(MsgHead);
            buff += sizeof(MsgHead);
            bHeart = 1;
            len -= sizeof(MsgHead);
            continue;
        }
        if ((len - sizeof(MsgHead)) < ph->m_len){
            LOG(WARNING)<<"recv incomplete pkg slen:"<<len
                <<" tlen:"<<ph->m_len
                <<" version:"<<ph->m_version
                <<" type:"<<ph->m_type
                <<" identifier:"<<ph->m_identifier;
            break;
        }
        rsize += sizeof(MsgHead);
        rsize += ph->m_len;
        len -= sizeof(MsgHead);
        len -= ph->m_len;
        buff += sizeof(MsgHead);
        /*
        switch (ph->m_type){
            case IM_MSG_REQ:
                if (!MsgReq(conn, buff, ph->m_len)){
                    return -1;
                }
                break;
            case IM_MSG_RSP:
                if (!MsgRsp(conn, buff, ph->m_len)){
                    return -1;
                }
                break;
            case IM_OP_REQ:
                if (!OpReq(conn, buff, ph->m_len)){
                    return -1;
                }
                break;
            case IM_OP_RSP:
                if (!OpRsp(conn, buff, ph->m_len)){
                    return -1;
                }
                break;
            default:
                LOG(ERROR)<<"recv unknown msg:"<<tid;
                return -1;
        }*/
    }while(true);
    if (bHeart){
        //response heart
    }
    return rsize;
}

void NetServerHandler::MsgReq(AsyncConn* conn, const char* buff, int len)
{
    //交换密钥 -> 
}

void NetServerHandler::PushMessage(uint32_t uid, const char* buff, size_t len)
{
    std::map<uint64_t, int> router;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::map<uint32_t, std::map<uint64_t, int>>::iterator iter = m_sessions.find(uid);
        if (iter != m_sessions.end()){
            std::map<uint64_t, int>::iterator it = iter->second.begin();
            for (; it!=iter->second.end(); it++){
                router[it->first] = it->second;
            }
        }
    }
    std::map<uint64_t, int>::iterator it = router.begin();
    for (; it != router.end(); it++){
        SendToClientLoop(it->second, it->first, buff, len);
    }
}
