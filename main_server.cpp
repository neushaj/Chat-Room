#include <iostream>
#include <stdexcept>

#include "server.h"

using namespace std;


int main(int argc, char** argv) {
    if (argc > 2)
        throw runtime_error("usage: [PORT; default 9000]");

    uint16_t port = 9000;
    if (argc == 2)
        port = stoi(argv[1]);

    Server srv(port);
    srv.serve();
    return EXIT_SUCCESS;
}
