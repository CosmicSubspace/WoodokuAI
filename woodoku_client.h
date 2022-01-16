 
#pragma once
#include <unistd.h>
#include <stdint.h>

class SocketClient{
private:
    int socketFD;
    bool initialized;

public:
    SocketClient();
    void initialize();
    bool writeData(const void *buf, size_t count);
    ssize_t readData(void* buffer, size_t count);
};

struct ServerState{
    bool boardState[81];
    bool piece1[25];
    bool piece2[25];
    bool piece3[25];
};
struct ClientMove{
    bool shape[25];
    uint8_t x;
    uint8_t y;
};



class WoodokuClient{
private:
    SocketClient sc;
public:
    WoodokuClient();
    bool commitMove(ClientMove *cm);
    bool getServerStateUpdate(ServerState *gsu);
};
