#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>


typedef std::uint32_t UserID;


struct User {
    std::string name;
    std::queue<std::pair<UserID, std::string>> messages;
};


class ChatRoom {
public:
    UserID connect(std::string name);
    void disconnect(UserID user_id);
    void send(UserID from_id, UserID to_id, std::string message);
    std::pair<UserID, std::string> receive(UserID id);
    std::vector<UserID> list();
    User info(UserID user_id);
private:
    std::map<UserID, User> users;
    std::mutex mutex;
    UserID next_user_id = 1;
};


class Server {
public:
    Server(std::uint16_t port);
    void serve();
private:
    void client(int);

    int fd;
    ChatRoom room;
};


class Client {
public:
    Client(int fd, ChatRoom& room);
    ~Client();

    void run();

private:
    void read(uint8_t* buffer, size_t len);
    void write(const uint8_t* buffer, size_t len);
    void connect(std::string name);
    std::vector<UserID> list();
    User info(UserID user_id);
    void send(UserID, std::string message);
    std::pair<UserID, std::string> receive();

    int fd;
    ChatRoom& room;
    bool connected = false;
    UserID user_id = 0;
};
