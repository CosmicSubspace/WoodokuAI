#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "assert.h"
#include "limits.h"
#include "time.h"


#include <iostream>

struct Uint8x2{
    uint8_t x;
    uint8_t y;
};
typedef struct Uint8x2 Vec2u8;

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
    Vec2u8 getBlock(int idx){
        uint32_t p=data;
        p=p>>(6*idx);
        Vec2u8 b;
        b.x=p&(0x07);
        p=p>>3;
        b.y=p&(0x07);
        return b;
    }
    Vec2u8 calculateBoundingBox(){
        Vec2u8 res;
        res.x=0;
        res.y=0;
        int n=numBlocks();
        for (int i=0;i<n;i++){
            Vec2u8 block=getBlock(i);
            if (block.x>res.x) res.x=block.x;
            if (block.y>res.y) res.y=block.y;
        }
        return res;
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
            Vec2u8 b=getBlock(i);
            printf("(%d,%d) ",b.x,b.y);
        }
        printf("\n");
    }
    bool hasBlockAt(int x, int y){
        // Very unoptimized. Only call in debug/infrequent code plz
        for(int i=0;i<numBlocks();i++){
            Vec2u8 b=getBlock(i);
            if ((b.x==x) && (b.y==y)) return true;
        }
        return false;
    }
};


struct Placement{
    Piece piece;
    uint8_t x;
    uint8_t y;
};
typedef struct Placement Placement;

#define MAX_GAME_STEPS 10000
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
        Vec2u8 block=pl.piece.getBlock(i);
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

void drawPieceQueue(PieceQueue pq, int idx, int count){
    for (int y=0;y<5;y++){
        for (int n=0;n<count;n++){
            Piece p=pq.getPiece(idx+n);
            for(int x=0;x<6;x++){
                if (p.hasBlockAt(x,y)){
                    if (n==0) ansiColorSet(CYAN_BRIGHT);
                    printf("[]");
                    if (n==0) ansiColorSet(NONE);
                }else{
                    printf("  ");
                }
            }

        }
        printf("\n");
    }
}

class GameState{
private:
    int score;
    Board board;
    PieceQueue *pq;
    int currentPieceIndex;
    bool dead;
public:
    GameState(PieceQueue *pieceQueue){
        score=0;
        board=Board();
        pq=pieceQueue;
        currentPieceIndex=0;
        dead=false;
    }
    bool isDead(){
        return dead;
    }
    void die(){
        dead=true;
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
    int getCurrentStepNum(){
        return currentPieceIndex;
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
    bool applyPlacement(Placement pl){
        PlacementResult pr=doPlacement(getBoard(),pl);
        if (pr.success){
            setBoard(pr.finalResult);
            addScore(pr.scoreDelta);
            incrementPieceQueue();
            return true;
        }else{
            return false;
        }
    }
};

#define SEARCH_DEPTH 4
struct DFSResult{
    Placement placements[SEARCH_DEPTH];
    int score;
    bool valid;
};
typedef struct DFSResult DFSResult;

DFSResult search(GameState initialState,int depth){

    DFSResult optimalResult;
    optimalResult.score=INT_MIN;
    optimalResult.valid=false;

    if (depth>=SEARCH_DEPTH) return optimalResult;

    Piece currentPiece=initialState.getCurrentPiece();
    //currentPiece.debug_print();
    Vec2u8 bbox;
    bbox=currentPiece.calculateBoundingBox();
    for (int x=0;x<(9-bbox.x);x++){
        for (int y=0;y<(9-bbox.y);y++){
            Placement pl;
            pl.piece=currentPiece;
            pl.x=x;
            pl.y=y;

            GameState inState=initialState;

            bool plres=inState.applyPlacement(pl);
            if (plres){
                //printf("Depth %d",depth);
                DFSResult dr;

                dr.score=inState.getScore();
                dr.valid=true;

                DFSResult dr_recursed=search(inState,depth+1);
                if (dr_recursed.valid){
                    dr=dr_recursed;
                }


                if (dr.score>optimalResult.score){

                    for (int i=(depth+1);i<SEARCH_DEPTH;i++){
                        dr.placements[i]=dr_recursed.placements[i];
                    }
                    dr.placements[depth]=pl;

                    optimalResult=dr;
                }


            }
        }
    }


    return optimalResult;
}

/*
 * TODO
 * Monte-carlo searching
 */
int main(){
    Piece tmp;
    Piece pieces[MAX_GAME_STEPS];
    int pieceN=0;

    // AUTO GENERATED CODE BELOW
    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(1,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(0,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(0,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(1,1);
    tmp.addBlock(0,2);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(2,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(2,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(0,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(1,1);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(1,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(2,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    tmp.addBlock(2,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    tmp.addBlock(0,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(1,1);
    tmp.addBlock(0,2);
    tmp.addBlock(1,2);
    tmp.addBlock(2,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(1,1);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(3,0);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    tmp.addBlock(0,3);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(3,0);
    tmp.addBlock(4,0);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    tmp.addBlock(0,3);
    tmp.addBlock(0,4);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(3,0);
    tmp.addBlock(2,1);
    tmp.addBlock(1,2);
    tmp.addBlock(0,3);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,1);
    tmp.addBlock(2,2);
    tmp.addBlock(3,3);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(2,0);
    tmp.addBlock(1,1);
    tmp.addBlock(0,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,1);
    tmp.addBlock(2,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,1);
    pieces[pieceN++]=tmp;


    srand(time(nullptr));


    PieceQueue pq;
    for(int i=0;i<MAX_GAME_STEPS;i++){
        pq.addPiece(pieces[rand()%pieceN]);
    }




    Placement pl;
    pl.piece=pieces[0];
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

    Board lastBoard;
    GameState gs(&pq);
    while (1){
        printf("\n\n\n");
        if (gs.getCurrentStepNum() > MAX_GAME_STEPS-10){
            break;
        }
        printf("Next piece:\n");
        drawPieceQueue(pq,gs.getCurrentStepNum(),5);
        //drawPiece(pq.getPiece(gs.getCurrentStepNum()));



        printf("DFS calculating...\n");
        DFSResult dfsr=search(gs,0);
        PlacementResult pr;
        Placement placement;
        if (!dfsr.valid){
            //No placement! Dead probs.
            printf("No placement possible!\n");
            break;
        }
        printf("DFS max: %d\n",dfsr.score);


        placement=dfsr.placements[0];
        pr=doPlacement(gs.getBoard(),placement);

        gs.applyPlacement(placement);
        printf("Step %d \n",gs.getCurrentStepNum());
        printf("Score: %d (+%d)\n",gs.getScore(),pr.scoreDelta);
        drawBoardFancy(lastBoard,pr.preClear,pr.finalResult);

        lastBoard=pr.finalResult;


        //printf("Enter to coninue...\n");
        //waitForEnter();
    }

    return 0;
}
