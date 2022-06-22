#include "client.h"

client::client(string c_name, int s_port):
name(c_name), server_port(s_port){
    message_id = 0;
}


void client::connectServer() {
    struct sockaddr_in server_address;
    
    fd_server = socket(AF_INET, SOCK_STREAM, 0);
    
    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(server_port); 
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (connect(fd_server, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) { // checking for errors
        throw runtime_error("Error in connecting to server\n");
    }

}

void client::send_connect_msg(){
    message_id++;
    auto sent_header = Header {};
    Header read_msg;
    sent_header.message_type = CONNECT;
    sent_header.message_id = message_id;
    sent_header.length = sizeof(Header) + name.length();
    write(fd_server, (uint8_t*)&sent_header, sizeof(sent_header));
    write(fd_server, (uint8_t*)name.c_str(), name.length());
    read(fd_server, (uint8_t*)&read_msg, sizeof(read_msg));
    if(read_msg.message_type != CONNACK || read_msg.message_id != message_id)
        throw runtime_error("Server is not answering!!\n");           
}

void client::send_list_msg(){
    message_id++;
    auto sent_header = Header {};
    Header read_msg;
    sent_header.message_type = LIST;
    sent_header.message_id = message_id;
    sent_header.length = sizeof(Header);
    write(fd_server, (uint8_t*)&sent_header, sizeof(sent_header));
    read(fd_server, (uint8_t*)&read_msg, sizeof(read_msg));
    
   /* if(read_msg.message_type != LISTREPLY || read_msg.message_id != message_id)
        throw runtime_error("Server is not answering!!\n");  */

    int num_of_names = (read_msg.length - sizeof(read_msg)) /2;
    uint16_t id_list[num_of_names];
    for(int i=0; i< num_of_names; i++){
        uint16_t buf;
        read(fd_server, (uint8_t*)&buf, sizeof(buf));
        id_list[i] = ntohs(buf);
    }
    ids.clear();
    names.clear();
    for(int i=0; i< num_of_names; i++){
        send_info_msg(id_list[i]); 
    }
}

void client::send_info_msg(uint16_t id){
    message_id++;
    id = htons(id);
    auto sent_header = Header {};
    Header read_msg;
    sent_header.message_type = INFO;
    sent_header.message_id = message_id;
    sent_header.length = sizeof(Header)+2;
    write(fd_server, (uint8_t*)&sent_header, sizeof(sent_header));
    write(fd_server, (uint8_t*)&id, sizeof(id));
    read(fd_server, (uint8_t*)&read_msg, sizeof(read_msg));
     
    int length_of_name = read_msg.length - sizeof(read_msg);
    uint8_t read_name[length_of_name+1];
    read(fd_server, read_name, length_of_name);
    read_name[length_of_name] = 0;
    string user_name = string((char*)read_name);
    if(length_of_name != 0){
        ids.push_back(ntohs(id));
        names.push_back(user_name);
    }
}

void client::send_text_msg(string msg, string name){
    uint16_t dest_id;
    for(int i=0; i<ids.size(); i++){
        if(names[i] == name){
            dest_id = ids[i];
            break;
        }
    }

    message_id++;
    dest_id = htons(dest_id);
    auto sent_header = Header {};
    Header read_msg;
    sent_header.message_type = SEND;
    sent_header.message_id = message_id;
    sent_header.length = sizeof(Header)+ sizeof(dest_id) + msg.length();
    write(fd_server, (uint8_t*)&sent_header, sizeof(sent_header));
    write(fd_server, (uint8_t*)&dest_id, sizeof(dest_id));
    write(fd_server, (uint8_t*)msg.c_str(), msg.length()); //note
    read(fd_server, (uint8_t*)&read_msg, sizeof(read_msg));
    uint8_t status;
    read(fd_server, &status, sizeof(status));
    status = (status);
    
    if(unsigned(status) == 0)
        cout << "   -sending failed! try again!" << endl; 
}

string client::find_name_by_id(uint16_t sender_id){
    send_list_msg(); // to make sure our list is updated
    for(int i=0; i<ids.size(); i++){
        if(ids[i] == sender_id){
            return names[i];
        }
    }
    return 0;
}

int client::recieve_text_msg(){
    message_id++;
    auto sent_header = Header {};
    Header read_msg;
    sent_header.message_type = RECEIVE;
    sent_header.message_id = message_id;
    sent_header.length = sizeof(Header);
    write(fd_server, (uint8_t*)&sent_header, sizeof(sent_header));
    read(fd_server, (uint8_t*)&read_msg, sizeof(read_msg));
    
    uint32_t sender_id;
    read(fd_server, (uint8_t*)&sender_id, sizeof(sender_id));
    int length_of_msg = read_msg.length - sizeof(read_msg) - sizeof(sender_id);
    if(unsigned(sender_id)!=0){
        uint8_t msg[length_of_msg+1];
        read(fd_server, msg, length_of_msg);
        msg[length_of_msg] = 0;
        string msg_txt = string((char*)msg);
        string name = find_name_by_id(sender_id);
        if(sender_id != 0)
            cout << " A new message is recieved from " << name << " : " << msg_txt <<endl;
    }
    return 0;

}

void client::start(){
    connectServer();
    send_connect_msg();
    while(1){
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        auto tv = timeval{3, 0};
        auto ret = select(3, &fds, NULL, NULL, &tv);
        if(ret<0)
            throw runtime_error("Select error!!");
        else if(ret == 0)
            recieve_text_msg();
        
        else if(FD_ISSET(STDIN_FILENO, &fds)){
            string line;
            cin >> line;
            if(line == "list"){
                send_list_msg();
                for(auto n: names){
                    if(n != name)
                        cout << "       -name: " << n << endl;
                }
            }
            else if(line == "send"){
                string name, msg;
                cin >> name;
                getline(cin, msg);
                send_list_msg(); // to make sure our list is updated
                send_text_msg(msg, name);
            }
            else if(line == "exit"){
                cout << "Exitting!!" << endl;
                exit(0);
            }
        }
    }
}

