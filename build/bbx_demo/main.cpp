extern "C"{
#include <mlc.h>
#include <cycle.h>
#include <session.h>
#include <core/log.h>
#include <event_timer.h>
}

#include <stdio.h>
#include <vector>
#include <algorithm>
#include <string>

logger_t logger = {0,};
logger_t *l = &logger;

class Room
{
    std::vector<session_t *> m_members;
    std::vector<std::string> m_cmds;
public:
    Room(){
    }

    ~Room(){

    }

    void add_cmd(const char *data,uint32_t len)
    {
        log_info(l,"recvdata=%s",data);
        m_cmds.emplace_back(std::string(data,len));
    }

    bool is_in_room(session_t *s)
    {
        return std::find(m_members.begin(),m_members.end(),s) != m_members.end();
    }

    void join(session_t *s)
    {
        if (is_in_room(s)==false) {
            m_members.push_back(s);
        }
    }

    void leave(session_t *s)
    {
        auto iter = std::find(m_members.begin(),m_members.end(),s);
        if (iter != m_members.end()) {
            m_members.erase(iter);
        }
    }

    void broadcast_flush()
    {
        int count = 0;
        auto send_func = [&](session_t *s){
            if(s && s->state == mlc_state_connected)
            {
                std::for_each(m_cmds.begin(),m_cmds.end(),[&](const std::string& send_str){
                    session_data_send(s, send_str.c_str(), send_str.size());
                });
                ++count;
            }
        };
        if (m_members.size()>0 && m_cmds.size()>0) {
            log_info(l,"-----------room broadcast flush,member=%d,cmd=%d---------",m_members.size(),m_cmds.size());
            std::for_each(m_members.begin(),m_members.end(),send_func);
        }
        m_cmds.clear();
    }

};

Room g_room;

int on_recv(session_t *s, const char *data, uint32_t len,void *userdata)
{
    log_info(l,"session recv data,s=%p, data=[%s]",s, data, len);
    g_room.add_cmd(data,len);
    return len;
}
int on_status(session_t *s,uint8_t state, void *userdata)
{
    if (state == mlc_state_lost)
    {
        g_room.leave(s);
        session_close(s,0);
    }
    return 0;
}
int on_connect(session_t *s, void *data)
{
    log_info(l,"add to room %p",s);
    g_room.join(s);
    return 0;
}

int on_flush(void *data)
{
    g_room.broadcast_flush();
    return 0;
}



int main(int argc, char const *argv[])
{
    cycle_conf_t conf = {
        0,
    };
    conf.connection_n = 1024;
    conf.debugger = 0;
    cycle_t *cycle = cycle_create(&conf, 1);
    assert(cycle);
    // mlc_addr_conf_t l_addr = {"0.0.0.0",8888};
    mlc_addr_conf_t l_addr;
    mlc_url_to_conf("mlc://0.0.0.0:8887",&l_addr);
    cycle->sl = session_listener_create(cycle, &l_addr);
    session_listener_set_handler(cycle->sl, NULL, on_connect, on_recv, on_status);
    event_timer_t *flush_timer = event_timer_create(cycle->timer_hub,NULL,(event_timer_handler_pt)on_flush);
    event_timer_start(flush_timer,67,1);

    
    for(int i=0;;i++)
    {
        if (i % 2000 == 0)
        {
            log_trace(NULL,"main loop %d", i);
            cycle_trace(cycle);
        }
        cycle_step(cycle, 1);
    }




}
