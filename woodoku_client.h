 
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <cassert>
#include <sys/types.h>
// Code modified from Examples section from
// man getaddrinfo(3)
#include <sys/socket.h>
#include <netdb.h>

#define SOCKETCLIENT_BUFFER_SIZE 8192
class SocketClient{
private:
    int socketFD;
    bool initialized;
    char internalBuffer[SOCKETCLIENT_BUFFER_SIZE];
    int bufferLength;
public:
    SocketClient(){
        initialized=false;
        bufferLength=0;
    }
    void initSocket(const char *node,const char *service){
        struct addrinfo hints;
        struct addrinfo *result, *rp;
        size_t len;

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

    bool writeData(const void *buf, size_t count){
        if (!initialized) return false;
        //printf("Socket write\n");
        if (write(socketFD, buf, count) != count) {
            fprintf(stderr, "partial/failed write\n");
            return false;
        }
        return true;
    }
    ssize_t fillBuffer(){
        if (!initialized) return -1;
        char* remainingBuffer=(internalBuffer+bufferLength);
        int remainingBufferSize=SOCKETCLIENT_BUFFER_SIZE-bufferLength;

        if (remainingBufferSize<1) return 0;

        ssize_t nread = read(socketFD, remainingBuffer, remainingBufferSize);

        if (nread == -1) {
            if ((errno==EAGAIN) || (errno==EWOULDBLOCK)){
                //printf("Socket read: NO DATA\n");
                return 0;
            }else{
                perror("read");
                return -1;
            }
        }else{
            /*
            printf("Socket read: %d bytes\n",(int)nread);

            for (int i=0;i<nread;i++){
                printf("Recv %d (buf[%d]) %x\n",i,bufferLength+i,internalBuffer[bufferLength+i]);
            }*/

            bufferLength+=nread;
            return nread;
        }
    }
    void shiftBuffer(unsigned int n){
        assert(n<=bufferLength);
        int newLength=bufferLength-n;
        for(int i=0;i<newLength;i++){
            internalBuffer[i]=internalBuffer[i+n];
        }
        bufferLength=newLength;
    }
    int getBufferLength(){
        return bufferLength;
    }
    bool readDataAssuredLength(void* buffer, int count){
        fillBuffer();
        if (getBufferLength()>=count){
            memcpy(buffer,internalBuffer,count);
            /*
            for (int i=0;i<count;i++){
                printf("memcpy %d %x %x\n",
                       i,
                       internalBuffer[i],
                       ((uint8_t*)buffer)[i]);
            }*/
            return true;
        }else return false;
    }
};

/*
 * ServerState [0]
 *   + ClientMove [0]
 * ServerState [1]
 */
struct ServerState{
    uint32_t turnIndex;
    bool boardState[81];
    uint8_t numPieces;
    bool pieces[3][25];
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
    WoodokuClient(const char *addr, const char *port){
      sc.initSocket(addr,port);
    }
    bool sendPacket(ClientMove *cm,uint32_t turn, bool retire){
        uint8_t buf[34];
        int bufferidx=0;

        // Start magic
        assert(bufferidx==0);
        buf[bufferidx++]=0x22;

        if (cm != nullptr){
            // Piece shape
            assert(bufferidx==1);
            for (int i=0;i<25;i++){
                buf[bufferidx++]=cm->shape[i];
            }

            // Piece X,Y
            assert(bufferidx==26);
            buf[bufferidx++]=cm->x;
            buf[bufferidx++]=cm->y;
        }else bufferidx+=27;

        // Turn Index, Big-endian
        assert(bufferidx==28);
        buf[bufferidx++]=((turn>>24)%256);
        buf[bufferidx++]=((turn>>16)%256);
        buf[bufferidx++]=((turn>>8)%256);
        buf[bufferidx++]=((turn>>0)%256);

        // Retire?
        assert(bufferidx==32);
        buf[bufferidx++]=retire;

        // End magic
        assert(bufferidx==33);
        buf[bufferidx++]=0x23;

        assert (bufferidx==34);
        return sc.writeData(buf,34);
    }
    bool sendRetire(uint32_t turnIndex){
        return sendPacket(nullptr,turnIndex,true);
    }
    bool sendMove(uint32_t turnIndex,ClientMove *cm){
        return sendPacket(cm,turnIndex,false);
    }


    bool recvServerStateUpdate(ServerState *gsu){
        uint8_t buf[163];
        int bufferidx=0;


        if (!sc.readDataAssuredLength(buf,163)) return false;



        // Start magic
        assert (bufferidx==0);
        if (buf[bufferidx++]!=0x41) {
            printf("Unmatched Packet ID! %x\n",
                   buf[bufferidx-1]);
            return false;
        }

        // Board state
        assert (bufferidx==1);
        for (int i=0;i<81;i++){
            gsu->boardState[i]=(buf[bufferidx++]!=0);
        }

        // Num. pieces
        assert (bufferidx==82);
        gsu->numPieces=buf[bufferidx++];

        // Pieces
        assert (bufferidx==83);
        for (int pidx=0;pidx<3;pidx++){
            for (int i=0;i<25;i++){
                gsu->pieces[pidx][i]=(buf[bufferidx++]!=0);
            }
        }

        // Turn Index, Big-endian
        assert(bufferidx==158);
        gsu->turnIndex=0;
        gsu->turnIndex+=(buf[bufferidx++]<<24);
        gsu->turnIndex+=(buf[bufferidx++]<<16);
        gsu->turnIndex+=(buf[bufferidx++]<<8);
        gsu->turnIndex+=(buf[bufferidx++]<<0);

        // End magic
        assert(bufferidx==162);
        if (buf[bufferidx++] != 0x42){
            printf("Unmatched Magic number!\n");
            return false;
        }
        assert (bufferidx==163);
        sc.shiftBuffer(bufferidx);
        return true;
    }
};
