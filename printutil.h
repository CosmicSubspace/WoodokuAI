#include "piece.h"
#include "game.h"

#include <cstdio>
#include <iostream>

#define BOARD_SIZE 9

void drawPiece(Piece p){
    int maxX=0;
    int maxY=0;
    for(int i=0;i<p.numBlocks();i++){
        Vec2u8 b=p.getBlock(i);
        if (b.x>maxX) maxX=b.x;
        if (b.y>maxY) maxY=b.y;
    }

    for (int y=0;y<=maxY;y++){
        for(int x=0;x<=maxX;x++){
            if (p.hasBlockAt(x,y)){
                printf("[]");
            }else{
                printf("  ");
            }
        }
        printf("\n");
    }
}


void drawBoard(Board b){
    char buffer[(BOARD_SIZE*2+1)*BOARD_SIZE+1];
    int idx=0;
    for (int y=0;y<BOARD_SIZE;y++){
        for (int x=0;x<BOARD_SIZE;x++){
            if (b.read(x,y)!=0) {
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

    printf("%s", buffer);
}

const char* NONE="0";
const char* RED="31";
const char* RED_BRIGHT="1;31";
const char* RED_DIM="2;31";
const char* GREEN="32";
const char* GREEN_BRIGHT="1;32";
const char* GREEN_DIM="2;32";
const char* YELLOW="33";
const char* YELLOW_BRIGHT="1;33";
const char* YELLOW_DIM="2;33";
const char* BLUE="34";
const char* BLUE_BRIGHT="1;34";
const char* BLUE_DIM="2;34";
const char* MAGENTA="35";
const char* MAGENTA_BRIGHT="1;35";
const char* MAGENTA_DIM="2;35";
const char* CYAN="36";
const char* CYAN_BRIGHT="1;36";
const char* CYAN_DIM="2;36";
const char* WHITE="37";
const char* WHITE_BRIGHT="1;37";
const char* WHITE_DIM="2;37";

void ansiColorSet(const char* colorcode){
    printf("\033[%sm",colorcode);
}
void drawBoardFancy(Board preplace, Board preclear, Board postclear){
    printf(" ");
    for (int x=0;x<BOARD_SIZE;x++){
        if ((x/3)%2==0)printf("--");
        else printf("  ");
    }
    printf("\n");

    for (int y=0;y<BOARD_SIZE;y++){
        if ((y/3)%2==0) printf("|");
        else printf(" ");
        for (int x=0;x<BOARD_SIZE;x++){
            int celltype=0;
            if (preplace.read(x,y)) celltype |= 1;
            if (preclear.read(x,y)) celltype |= 2;
            if (postclear.read(x,y)) celltype |= 4;

            if (celltype==0){
                //Empty cell
                printf("  ");
            }else if (celltype==1){
                // Invalid
                printf("??");
            }else if (celltype==2){
                // Cell placed and immediately erased
                ansiColorSet(RED_BRIGHT);
                printf("[]");
                ansiColorSet(NONE);
            }else if (celltype==3){
                // Existing cell, erased
                ansiColorSet(RED);
                printf("[]");
                ansiColorSet(NONE);
            }else if (celltype==4){
                // Invalid
                printf("??");
            }else if (celltype==5){
                // Invalid
                printf("??");
            }else if (celltype==6){
                //Newly-placed cell
                ansiColorSet(WHITE_BRIGHT);
                printf("[]");
                ansiColorSet(NONE);
            }else if (celltype==7){
                //Untouched cell
                ansiColorSet(WHITE_DIM);
                printf("[]");
                ansiColorSet(NONE);
            }
        }
        if ((y/3)%2==0) printf("|");
        else printf(" ");
        printf("\n");
    }

    printf(" ");
    for (int x=0;x<BOARD_SIZE;x++){
        if ((x/3)%2==0)printf("--");
        else printf("  ");
    }
    printf("\n");
}

void waitForEnter(){
    std::cin.get();
}

void drawPieceQueue(PieceQueue *pq, uint32_t idx, int count, int dfsrange){
    for (int y=0;y<5;y++){
        for (int n=0;n<count;n++){
            bool visible=pq->isVisible(idx+n);
            Piece p;
            if (visible) p=pq->getPiece(idx+n);

            if (n==dfsrange) printf(" | ");

            for(int x=0;x<6;x++){
                if (visible && p.hasBlockAt(x,y)){
                    if (n==0) ansiColorSet(CYAN_BRIGHT);
                    else if (n<dfsrange) ansiColorSet(CYAN_DIM);
                    else ansiColorSet(WHITE_DIM);
                    printf("[]");
                    ansiColorSet(NONE);
                }else if ((!visible) && y==1 && (x/2)==1){
                    if (n==0) ansiColorSet(CYAN_BRIGHT);
                    else if (n<dfsrange) ansiColorSet(CYAN_DIM);
                    else ansiColorSet(WHITE_DIM);
                    printf("??");
                    ansiColorSet(NONE);
                }else{
                    printf("  ");
                }
            }

        }
        printf("\n");
    }
}
