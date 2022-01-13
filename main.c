#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"

struct Piece{
    uint8_t count;
    uint8_t *blocks; //Low nibble:X High nibble:Y
};
typedef struct Piece Piece;
Piece *piecedefs;
int piecedefN=0;


#define BOARD_SIZE 9
uint8_t *board;
uint8_t accessBoard(uint8_t *b, int x, int y){
    //TODO in-bounds check?
    return b[x+y*BOARD_SIZE];
}
void writeBoard(uint8_t *b, int x, int y,uint8_t value){
    //TODO in-bounds check?
    b[x+y*BOARD_SIZE]=value;
}
void drawBoard(uint8_t *b){
    char buffer[(BOARD_SIZE*2+1)*BOARD_SIZE+1];
    int idx=0;
    for (int y=0;y<BOARD_SIZE;y++){
        for (int x=0;x<BOARD_SIZE;x++){
            if (accessBoard(b,x,y)!=0) {
                buffer[idx++]='[';
                buffer[idx++]=']';
            }
            else {
                buffer[idx++]=' ';
                buffer[idx++]=' ';
            }

        }
        buffer[idx++]='\n';
    }
    buffer[idx++]='\0';

    printf(buffer);
}
uint8_t tryPiece(Piece *p, uint8_t *b,uint8_t locX,uint8_t locY){
    for(int i=0;i<p->count;i++){
        uint8_t block=(p->blocks)[i];
        int blockX=block&(0x0F);
        int blockY=block&(0xF0)>>4;
        int absX=blockX+locX;
        int absY=blockY+locY;
        if (absX<0 || absX>=BOARD_SIZE) return 2;
        if (absY<0 || absY>=BOARD_SIZE) return 3;
        if (accessBoard(b,absX,absY) !=0) return 10;
    }
    return 0; //no problems
}
void placePiece(Piece *p, uint8_t *b,uint8_t locX,uint8_t locY){
    for(int i=0;i<p->count;i++){
        uint8_t block=(p->blocks)[i];
        int blockX=block&(0x0F);
        int blockY=(block&(0xF0))>>4;
        int absX=blockX+locX;
        int absY=blockY+locY;
        writeBoard(b,absX,absY,1);
    }
}
uint8_t tryAndPlace(Piece *p, uint8_t *b,uint8_t locX,uint8_t locY){
    uint8_t ret=tryPiece(p,b,locX,locY);
    if (ret==0){
        placePiece(p,b,locX,locY);
    }
    return ret;
}
int main(){
    piecedefs = (Piece*) malloc(sizeof(Piece)*100);
    piecedefN=1;
    piecedefs[0].count=4;
    piecedefs[0].blocks= (uint8_t*) malloc(sizeof(uint8_t)*4);
    piecedefs[0].blocks[0]=0x00;
    piecedefs[0].blocks[1]=0x10;
    piecedefs[0].blocks[2]=0x20;
    piecedefs[0].blocks[3]=0x11;



    board = (uint8_t*) malloc(sizeof(uint8_t)*BOARD_SIZE*BOARD_SIZE);
    for( int i=0;i<BOARD_SIZE*BOARD_SIZE;i++){
        board[i]=0;
        //if (i%4==0) board[i]=1;
    }

    printf("Placing #1: %d\n",tryAndPlace(&piecedefs[0],board,0,0));
    printf("Placing #2: %d\n",tryAndPlace(&piecedefs[0],board,0,0));
    printf("Placing #3: %d\n",tryAndPlace(&piecedefs[0],board,3,3));
    drawBoard(board);
    return 0;
}
