#include "util/json/json.h"
#include "glog/logging.h"
#include "server_handler.h"
#include "amqp_handler.h"
#include "sys_help.h"
int main(int agrv, char** agrc)
{
    enableCoredump();
    //NetServerHandler s;
    //s.Start("172.16.0.56", 8899, 4, 4);
    AMQP_QUEUE queue[2];
    queue[0].m_queue = "a_test1";
    queue[0].m_bind_keys.push_back(std::string("test_key"));
    queue[0].m_bind_keys.push_back(std::string("test_key1"));
    queue[1].m_queue = "a_test2";
    queue[1].m_bind_keys.push_back(std::string("test_key"));
    queue[1].m_bind_keys.push_back(std::string("test_key2"));
    AMQP_CONFIG config[2];
    config[0].m_ip = "127.0.0.1";
    config[0].m_port = 5672;
    config[0].m_username = "guesta";
    config[0].m_passwd = "guest";
    config[0].m_vhost = "/";
    config[0].m_ex = "tv";
    config[0].m_queue.push_back(&queue[0]);
    config[1].m_ip = "127.0.0.1";
    config[1].m_port = 5672;
    config[1].m_username = "guest";
    config[1].m_passwd = "guest";
    config[1].m_vhost = "/";
    config[1].m_ex = "tv";
    config[1].m_queue.push_back(&queue[1]);
    AmqpConfig cfg;
    cfg.m_conf.push_back(&config[0]);
    cfg.m_conf.push_back(&config[1]);
    AmqpHandler mq;
    mq.Login(&cfg, 0x100, 0);
    sleep(200);
    return 0;
}

