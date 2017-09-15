#ifndef  __PUBLIC_HANDLER_H
#define  __PUBLIC_HANDLER_H

struct public_config
{
    Json_u::Value           m_cfg;
    net_command_handlers    m_fhandler;
    net_command_handlers    m_bhandler;
    HPointNode              m_hash_uid;
    HPointNode              m_hash_sr;
};

#endif
