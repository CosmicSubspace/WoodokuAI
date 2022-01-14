#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "assert.h"
#include "limits.h"

#include <iostream>

struct Block{
    uint8_t x;
    uint8_t y;
};
typedef struct Block Block;

class Piece{
private:
    uint32_t data;
public:
    Piece(){
        data=0xFFFFFFFF;
    }
    int numBlocks(){
        uint32_t p=data;
        int n=0;
        while ((p&(0x3F)) != 0x3F){
            n++;
            p=p>>6;
            if (n==5) break;
        }
        return n;
    }
    Block getBlock(int idx){
        uint32_t p=data;
        p=p>>(6*idx);
        Block b;
        b.x=p&(0x07);
        p=p>>3;
        b.y=p&(0x07);
        return b;
    }
    void addBlock(int x, int y){
        uint32_t p=data;
        uint32_t block=0;
        uint32_t mask=0x3F;
        block+=y;
        block=block<<3;
        block+=x;
        for(int i=0;i<5;i++){
            if ((p&mask)==mask){
                // untouched... write here.
                p= (p^mask)|block;
                break;
            }
            mask=mask<<6;
            block=block<<6;
        }
        data=p;
        //do nothing if error - maybe handle errors or smth
    }
    void debug_print(){
        printf("Piece: ");
        for (int i=0;i<numBlocks();i++){
            Block b=getBlock(i);
            printf("(%d,%d) ",b.x,b.y);
        }
        printf("\n");
    }
};


struct Placement{
    Piece piece;
    uint8_t x;
    uint8_t y;
};
typedef struct Placement Placement;

#define MAX_GAME_STEPS 100
class PlacementSequence{
private:
    Placement placements[MAX_GAME_STEPS];
    int length;
public:
    PlacementSequence(){
        length=0;
    }
    void addPlacement(Placement p){
        placements[length]=p;
        length++;
    }
    int getLength(){
        return length;
    }
    Placement get(int idx){
        return placements[idx];
    }
};


#define BOARD_SIZE 9
class Board{
private:
    uint32_t bitfield[3];
    int coord2idx(int x, int y){
        return x+y*BOARD_SIZE;
    }
public:
    Board(){
        bitfield[0]=0;
        bitfield[1]=0;
        bitfield[2]=0;
    }
    bool read(int x, int y){
        int idx=coord2idx(x,y);
        int varnum=idx/32;
        int bitnum=idx%32;
        return ((bitfield[varnum])>>bitnum) & 1;
    }
    void write(int x, int y,bool value){
        int idx=coord2idx(x,y);
        int varnum=idx/32;
        int bitnum=idx%32;
        if (value) bitfield[varnum] |= (1<<bitnum);
        else bitfield[varnum] &= ~(1<<bitnum);
    }

};

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
const char* WHITE="37";
const char* WHITE_BRIGHT="1;37";
const char* WHITE_DIM="2;37";
void ansiColorSet(const char* colorcode){
    printf("\033[%sm",colorcode);
}
void drawBoardFancy(Board preplace, Board preclear, Board postclear){
    for (int y=0;y<BOARD_SIZE;y++){
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
            }else if (celltype==5){
                // Invalid
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
        printf("\n");
    }
}

void waitForEnter(){
    std::cin.get();
}


struct PlacementResult{
    bool success;
    Board preClear;
    Board finalResult;
    int scoreDelta;
};
typedef struct PlacementResult PlacementResult;
PlacementResult doPlacement(Board b,Placement pl){
    PlacementResult pr;

    int n=pl.piece.numBlocks();
    bool success=true;
    for(int i=0;i<n;i++){
        Block block=pl.piece.getBlock(i);
        int blockX=block.x;
        int blockY=block.y;
        int absX=blockX+pl.x;
        int absY=blockY+pl.y;

        if (absX<0 || absX>=BOARD_SIZE) success=false;
        else if (absY<0 || absY>=BOARD_SIZE) success=false;
        else if (b.read(absX,absY) !=0) success=false;

        if (!success) break;

        b.write(absX,absY,true);
    }

    // early abort for fails
    pr.success=success;
    if (!success) return pr;

    pr.preClear=b;


    //check lines
    // checks [0..8] X   [9..17] Y   [18..26] Sq
    // bit is unset if there is an empty cell
    uint32_t checks=0xFFFFFFFF;
    for (int x=0;x<BOARD_SIZE;x++){
        for (int y=0;y<BOARD_SIZE;y++){
            bool cell=b.read(x,y);
            int sq=3*(y/3)+(x/3);

            if (!cell){
                uint32_t mask=0;
                mask |= (1<<(x));
                mask |= (1<<(y+9));
                mask |= (1<<(sq+18));
                checks=checks & (~mask);
            }
        }
    }

    int count=0;
    for (int i=0;i<27;i++) {
        if ((checks >> i) & 1) count++;
    }

    int bonus=0;
    if (count>1) bonus=10*(count-1);
    int score= count*18+bonus+n;

    for (int x=0;x<BOARD_SIZE;x++){
        for (int y=0;y<BOARD_SIZE;y++){
            int sq=3*(y/3)+(x/3);
            if ((checks>>(x)) & 1) b.write(x,y,false);
            if ((checks>>(y+9)) & 1) b.write(x,y,false);
            if ((checks>>(sq+18)) & 1) b.write(x,y,false);
        }
    }
    pr.finalResult=b;
    pr.scoreDelta=score;

    return pr;
}

class PieceQueue{
private:
    int numPieces;
    Piece pieces[MAX_GAME_STEPS];
public:
    PieceQueue(){
        numPieces=0;
    }
    void addPiece(Piece p){
        pieces[numPieces]=p;
        numPieces++;
    }
    Piece getPiece(int idx){
        return pieces[idx];
    }
    int getLength(){
        return numPieces;
    }

};

class GameState{
private:
    int score;
    Board board;
    PieceQueue *pq;
    int currentPieceIndex;
    PlacementSequence psq;
public:
    GameState(PieceQueue *pieceQueue){
        score=0;
        board=Board();
        pq=pieceQueue;
        currentPieceIndex=0;
        psq=PlacementSequence();
    }
    void incrementPieceQueue(){
        currentPieceIndex++;
        if (currentPieceIndex>=(pq->getLength())) currentPieceIndex=(pq->getLength())-1;
    }
    bool atEnd(){
        return currentPieceIndex+1>=(pq->getLength());
    }
    Piece getCurrentPiece(){
        return pq->getPiece(currentPieceIndex);
    }
    Board getBoard(){
        return board;
    }
    void setBoard(Board b){
        board=b;
    }
    int getScore(){
        return score;
    }
    void addScore(int n){
        score+=n;
    }
    void addPlacement(Placement p){
        psq.addPlacement(p);
    }
    PlacementSequence getPsq(){
        return psq;
    }
};

GameState search(GameState initialState,int depth){
    if (depth<=0) return initialState;
    //printf("### DBG: depth %d\n",depth);

    GameState optimalState=initialState;
    int maxScore=INT_MIN;
    Piece currentPiece=initialState.getCurrentPiece();
    //currentPiece.debug_print();
    for (int x=0;x<9;x++){
        for (int y=0;y<9;y++){
            Placement pl;
            pl.piece=currentPiece;
            pl.x=x;
            pl.y=y;
            PlacementResult pr=doPlacement(initialState.getBoard(),pl);
            if (pr.success){
                //currentPiece.debug_print();
                //printf("    DBG: Placement success %d %d\n",x,y);
                //drawBoard(pr.result);

                GameState inState=initialState;
                inState.setBoard(pr.finalResult);
                inState.addScore(pr.scoreDelta);
                inState.addPlacement(pl);
                inState.incrementPieceQueue();
                GameState outState=search(inState,depth-1);

                if (outState.getScore()>maxScore){
                    maxScore=outState.getScore();
                    optimalState=outState;
                }
            }else{
                //currentPiece.debug_print();
                //printf("    DBG: Placement fail %d %d\n",x,y);
            }
        }
    }
    return optimalState;
}


int main(){
    Piece t1,r1,o1,x1,i1;


    t1.addBlock(0,0);
    t1.addBlock(1,0);
    t1.addBlock(2,0);
    t1.addBlock(1,1);

    r1.addBlock(0,0);
    r1.addBlock(1,0);
    r1.addBlock(0,1);

    x1.addBlock(0,1);
    x1.addBlock(1,1);
    x1.addBlock(1,0);
    x1.addBlock(1,2);
    x1.addBlock(2,1);

    i1.addBlock(0,1);
    i1.addBlock(0,0);
    i1.addBlock(0,3);
    i1.addBlock(0,2);


    PieceQueue pq;
    pq.addPiece(t1);
    pq.addPiece(t1);
    pq.addPiece(i1);
    pq.addPiece(x1);
    pq.addPiece(r1);
    pq.addPiece(r1);

    for(int i=0;i<5;i++){
        pq.getPiece(i).debug_print();
    }



    Placement pl;
    pl.piece=x1;
    pl.x=2;
    pl.y=2;


    Board b=Board();
    //b.bitfield[0]=0xDEADBEEF;
    b.write(2,2,true);
    /*
    printf("Placing #1: %d\n",tryAndPlace(&piecedefs[0],board,0,0));
    printf("Placing #2: %d\n",tryAndPlace(&piecedefs[0],board,0,0));
    printf("Placing #3: %d\n",tryAndPlace(&piecedefs[0],board,3,3));*/

    PlacementResult pr=doPlacement(b,pl);
    printf("Placing #1: success? %d\n",pr.success);
    printf("Placing #1: Board\n");
    drawBoard(pr.preClear);
    printf("Placing #1: Deletion\n");
    drawBoard(pr.finalResult);
    printf("Placing #1: score %d\n",pr.scoreDelta);

    printf("DFS start...\n");
    GameState igs(&pq);
    GameState fgs=search(igs,5);
    printf("DFS max: %d\n",fgs.getScore());
    drawBoard(fgs.getBoard());

    //Replay
    while (1){
        printf("\n\n\nBegin replay:\n");
        PlacementSequence rp_psq=fgs.getPsq();
        Board rp_b=Board();
        Board lastBoard;
        for(int i=0;i<rp_psq.getLength();i++){
            PlacementResult pr=doPlacement(rp_b,rp_psq.get(i));
            printf("\n\nStep %d \n",i);
            printf("Score delta: %d\n",pr.scoreDelta);
            rp_b=pr.finalResult;
            drawBoardFancy(lastBoard,pr.preClear,pr.finalResult);
            lastBoard=pr.finalResult;
            waitForEnter();
        }
    }
    return 0;
}
