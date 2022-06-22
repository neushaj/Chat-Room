#include "server.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <thread>
#include <tuple>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "message.h"

using namespace std;

#define MAX_PAYLOAD_LENGTH 10000


UserID ChatRoom::connect(string name) {
    lock_guard<std::mutex> lock(mutex);

    users[next_user_id] = User{name: name};
    return next_user_id++;
}


void ChatRoom::disconnect(UserID user_id) {
    lock_guard<std::mutex> lock(mutex);

    users.erase(user_id);
}


vector<UserID> ChatRoom::list() {
    lock_guard<std::mutex> lock(mutex);

    vector<UserID> ret;
    transform(users.begin(), users.end(), std::back_inserter(ret),
              [](decltype(users)::const_reference u) { return u.first; });
    return ret;
}


User ChatRoom::info(UserID user_id) {
    lock_guard<std::mutex> lock(mutex);

    return users.at(user_id);
}


void ChatRoom::send(UserID from_id, UserID to_id, string message) {
    lock_guard<std::mutex> lock(mutex);

    auto& user = users[to_id];
    user.messages.push(make_pair(from_id, message));
}


pair<UserID, string> ChatRoom::receive(UserID user_id) {
    lock_guard<std::mutex> lock(mutex);

    auto& user = users[user_id];
    if (user.messages.empty())
        return make_pair(0, "");
    auto message = user.messages.front();
    user.messages.pop();
    return message;
}

 
Server::Server(std::uint16_t port) {
    auto srv_addr = sockaddr_in{sin_family: AF_INET,
                                sin_port: htons(port),
                                sin_addr: {s_addr: htonl(INADDR_ANY)}};

    fd = socket(AF_INET, SOCK_STREAM, 0);

    try {
        auto err = bind(fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
        if (err < 0)
            throw runtime_error("failed to bind");

        err = listen(fd, 10);
        if (err < 0)
            throw runtime_error("failed to listen");
    } catch (exception) {
        close(fd);;
        throw;
    }
}


void Server::serve() {
    while (1) {
        sockaddr_in client_addr;
        auto client_addr_len = sizeof(client_addr);
        auto client_fd = accept(this->fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
        char client_ip[INET_ADDRSTRLEN+1];
        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, INET_ADDRSTRLEN);
        clog << "new client: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;

        thread(&Server::client, this, client_fd).detach();
    }
}


void Server::client(int client_fd) {
    auto client = Client(client_fd, room);
    try {
        client.run();
    } catch (runtime_error e) {
        cerr << "Exception: " << e.what() << endl;
    }
}


Client::Client(int fd, ChatRoom& room): fd(fd), room(room){}


Client::~Client() {
    close(fd);
    room.disconnect(user_id);

    clog << user_id << " disconnected" << endl;
}


void Client::run() {
    while (true) {
        clog << "waiting for message from " << (connected ? to_string(user_id) : "the new client") << endl;

        Header header;
        read((uint8_t*)&header, sizeof(header));
        auto payload_length = header.length - sizeof(Header);
        if (payload_length < 0)
            throw runtime_error("negative payload length");
        uint8_t payload[MAX_PAYLOAD_LENGTH + 1];
        read(payload, payload_length);

        switch (header.message_type) {
        case CONNECT:
            if (payload_length < 1)
                throw runtime_error("wrong message length");
            {
                payload[payload_length] = 0;
                string user_name = string((char*)payload);
                connect(user_name);

                /*auto reply_header = Header{{message_type: CONNACK, message_id: header.message_id},
                                           length: sizeof(Header)};*/
                auto reply_header = Header {};
                reply_header.message_type = CONNACK;
                reply_header.message_id = header.message_id;
                reply_header.length = sizeof(Header);
                write((uint8_t*)&reply_header, sizeof(reply_header));
            }
            break;

        case LIST:
            {
                auto lst = list();
                /*auto reply_header = Header{{message_type: LISTREPLY, message_id: header.message_id},
                                           length: uint8_t(sizeof(Header) + 2*lst.size())};*/
                auto reply_header = Header {};
                reply_header.message_type = LISTREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = uint8_t(sizeof(Header) + 2*lst.size());
                write((uint8_t*)&reply_header, sizeof(reply_header));
                for (auto user_id_: lst) {
                    uint16_t buf = htons(user_id_);
                    write((uint8_t*)&buf, sizeof(buf));
                }
            }
            break;

        case INFO:
            if (payload_length != 2)
                throw runtime_error("wrong message length");
            {
                uint16_t id = ntohs(*(uint16_t*)payload);
                string name = "";
                try {
                    name = info(id).name;
                } catch (exception) {}
                /*auto reply_header = Header{{message_type: INFOREPLY, message_id: header.message_id},
                                           length: uint8_t(sizeof(Header) + name.length())};*/
                auto reply_header = Header {};
                reply_header.message_type = INFOREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = uint8_t(sizeof(Header) + name.length());
                write((uint8_t*)&reply_header, sizeof(reply_header));
                write((uint8_t*)name.c_str(), name.length());
            }
            break;

        case SEND:
            if (payload_length <= 2)
                throw runtime_error("wrong message length");
            {
                auto id = ntohs(*(uint16_t*)payload);
                auto message_length = payload_length - 2;
                char* message = (char*)payload + 2;
                message[message_length] = 0;
                uint8_t* state_buff = (uint8_t*)"\xff";
                try {
                    send(id, message);
                } catch (exception) {
                    state_buff = (uint8_t*)"\x00";
                }
                /*auto reply_header = Header{{message_type: SENDREPLY, message_id: header.message_id},
                                           length: sizeof(Header) + 1};*/
                auto reply_header = Header {};
                reply_header.message_type = SENDREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = sizeof(Header) + 1;           
                write((uint8_t*)&reply_header, sizeof(reply_header));
                write(state_buff, 1);
            }
            break;

        case RECEIVE:
            if (payload_length != 0)
                throw runtime_error("wrong message length");
            {     
                auto pr = receive();
                auto usr_id = (pr.first);
                auto message = pr.second;
                /*auto reply_header = Header{{message_type: RECEIVEREPLY, message_id: header.message_id},
                                            length: uint8_t(sizeof(Header) + sizeof(usr_id) + message.length())};*/
                auto reply_header = Header {};
                reply_header.message_type = RECEIVEREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = uint8_t(sizeof(Header) + sizeof(usr_id) + message.length());   
                write((uint8_t*)&reply_header, sizeof(reply_header));
                write((uint8_t*)&usr_id, sizeof(usr_id));
                write((uint8_t*)message.c_str(), message.length());
            }
            break;
        }
    }
}


void Client::read(uint8_t* buffer, size_t len) {
    auto n = 0;
    while (n < len) {
        auto ret = ::read(fd, buffer + n, len - n);
        if (ret < 0){
            throw runtime_error("failed to read");
            
        }
        if (ret == 0)
            throw runtime_error("socket closed");

        n += ret;
    }
}


void Client::write(const uint8_t* buffer, size_t len) {
    auto n = 0;
    while (n < len) {
        auto ret = ::write(fd, buffer + n, len - n);
        if (ret < 0)
            throw runtime_error("failed to write");
        if (ret == 0)
            throw runtime_error("socket closed");

        n += ret;
    }
}


void Client::connect(string user_name) {
    if (connected)
        throw runtime_error("connected before");

    user_id = room.connect(user_name);
    connected = true;

    clog << user_id << " [" << user_name << "]" << " connected" << endl;
}


vector<UserID> Client::list() {
    if (!connected)
        throw runtime_error("not connected");

    clog << "list from " << user_id << endl;
    return room.list();
}


User Client::info(UserID user_id_) {
    if (!connected)
        throw runtime_error("not connected");

    clog << "info from " << user_id << " for " << user_id_ << endl;
    return room.info(user_id_);
}


void Client::send(UserID id, string message) {
    if (!connected)
        throw runtime_error("not connected");

    clog << "send from " << user_id << " to " << id << " \"" << message << "\"" << endl;
    room.send(user_id, id, message);
}


pair<UserID, string> Client::receive() {
    if (!connected)
        throw runtime_error("not connected");

    clog << "receive from " << user_id << endl;
    return room.receive(user_id);
}
