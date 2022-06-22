#include <cstdint>


#define CONNECT       1
#define CONNACK       2
#define LIST          3
#define LISTREPLY     4
#define INFO          5
#define INFOREPLY     6
#define SEND          7
#define SENDREPLY     8
#define RECEIVE       9
#define RECEIVEREPLY 10

struct Header {
    union {
        struct {
            uint8_t message_type:4;
            uint8_t message_id:4;
        };
        uint8_t message_info;
    };

    uint8_t length;
} __attribute__((packed));
