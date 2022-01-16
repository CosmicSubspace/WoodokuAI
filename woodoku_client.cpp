#include <sys/types.h>
// Code modified from Examples section from
// man getaddrinfo(3)

#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <cassert>


#include "woodoku_client.h"


SocketClient::SocketClient(){
    initialized=false;
}
void SocketClient::initialize(){
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    size_t len;

    const char *node= "127.0.0.1";
    const char *service= "14311";

    /* Obtain address(es) matching host/port. */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    int gai=getaddrinfo(node, service, &hints, &result);
    if (gai != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socketFD = socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);
        if (socketFD == -1)
            continue;

        if (connect(socketFD, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(socketFD);
    }

    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    if (fcntl(socketFD, F_SETFL, O_NONBLOCK)) {
        fprintf(stderr, "Nonblock set fail\n");
        perror("fcntl nonblock set");
        exit(EXIT_FAILURE);
    }
    //printf("Socket initialized\n");
    initialized=true;
}

bool SocketClient::writeData(const void *buf, size_t count){
    if (!initialized) return false;
    printf("Socket write\n");
    if (write(socketFD, buf, count) != count) {
        fprintf(stderr, "partial/failed write\n");
        return false;
    }
    return true;
}
ssize_t SocketClient::readData(void* buffer, size_t count){
    if (!initialized) return -1;
    ssize_t nread;
    nread = read(socketFD, buffer, count);
    if (nread == -1) {
        if ((errno==EAGAIN) || (errno==EWOULDBLOCK)){
            //printf("Socket read: NO DATA\n");
            return 0;
        }else{
            perror("read");
            return -1;
        }
    }else{
        //printf("Socket read: %d bytes\n",(int)nread);
        return nread;
    }
}



WoodokuClient::WoodokuClient(){
    sc.initialize();
}
bool WoodokuClient::commitMove(ClientMove *cm){
    uint8_t buf[29];
    int bufferidx=0;
    buf[bufferidx++]=0x22;
    for (int i=0;i<25;i++){
        buf[bufferidx++]=cm->shape[i];
    }
    buf[bufferidx++]=cm->x;
    buf[bufferidx++]=cm->y;
    buf[bufferidx++]=0x23;
    assert (bufferidx==29);
    return sc.writeData(buf,29);
}

//TODO this does NOT handle cases where data gets split
// into multiple packets. fix that.
bool WoodokuClient::getServerStateUpdate(ServerState *gsu){
    u_int8_t buf[158];
    int bufferidx=0;
    if (sc.readData(buf,158)<158) return false;

    if (buf[bufferidx++]!=0x41) {
        printf("Unmatched Packet ID!\n");
        return false;
    }

    for (int i=0;i<81;i++){
        gsu->boardState[i]=(buf[bufferidx++]!=0);
    }

    for (int i=0;i<25;i++){
        gsu->piece1[i]=(buf[bufferidx++]!=0);
    }
    for (int i=0;i<25;i++){
        gsu->piece2[i]=(buf[bufferidx++]!=0);
    }
    for (int i=0;i<25;i++){
        gsu->piece3[i]=(buf[bufferidx++]!=0);
    }
    if (buf[bufferidx++] != 0x42){
        printf("Unmatched Magic number!\n");
        return false;
    }
    assert (bufferidx==158);
    return true;
}

/*
void sleepMillis(uint32_t ms){
    usleep(ms*1000);
}

int main(){

    WoodokuClient wc;

    while (1){
        ServerState ss;
        if (!wc.getServerStateUpdate(&ss)){
            printf("No SS Received...\n");
            sleepMillis(100);
        }else{
            printf("Received SS\n");
            ClientMove cm;
            for (int i=0;i<25;i++){
                cm.shape[i]=ss.piece1[i];
            }
            cm.x=0;
            cm.y=0;
            if (wc.commitMove(&cm)){
                printf("Commmit move success\n");
            }else{
                printf("Commmit move failed\n");
            }
        }

    }

    return 0;
}*/












