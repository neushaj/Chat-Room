#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include <cstring>
#include <algorithm>
#include <exception>
#include <iostream>
#include <tuple>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


#include "message.h"
using namespace std;

class client{
    private:
        int fd_server;
        int server_port;
        string name;
        int message_id;
        vector<uint16_t> ids;
        vector<string> names;

        void connectServer();
        void send_connect_msg();
        void send_list_msg();
        void send_info_msg(uint16_t id);
        void send_text_msg(string msg, string name);
        int recieve_text_msg();
        string find_name_by_id(uint16_t sender_id);

    public:
        client(string c_name, int s_port);
        void start();
        
};
