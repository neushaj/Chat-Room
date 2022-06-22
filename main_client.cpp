
#include "client.h"

using namespace std;



int main(int argc, char** argv) {
    uint16_t port;
    string name;

    if (argc == 3){
        port = stoi(argv[1]);
        name = argv[2];
    }
    else
        throw runtime_error("Error: Wrong Input");
    
    client new_client(name, port);
    new_client.start();    
}