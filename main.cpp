#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "assert.h"
#include "limits.h"
#include "time.h"


#include <iostream>
#include <chrono>

// Crucial constant, determines the DFS search depth
// Has very heavy impact on speed and smartness.
// set to 0 for dynamic search depth adjustment.
#define CONSTANT_SEARCH_DEPTH 0

// Dynamic depth parameters.
// The program will go for an additional DFS level
// If the current DFS took less than ADDITIONAL_DEPTH_THRESH milliseconds.
#define MAX_SEARCH_DEPTH 10
#define ADDITIONAL_DEPTH_THRESH 100

// Size of the PieceQueue buffer.
#define PIECEQUEUE_SIZE 20

// Maximum game length. 0 for unlimited.
#define MAX_GAME_STEPS 0

// A lot of code assumes 9x9 board size implicitly.
// You should probably leave this alone.
#define BOARD_SIZE 9

// Use constant seed. Useful for performance measurement.
// Use 0 to disable.
#define CONSTANT_SEED 0

#define INVISBLE_PIECE_MONTE_CARLO_ITER 10

#define PREVIEW_PIECES 6


uint64_t timeSinceEpochMillisec() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

struct Uint8x2{
    uint8_t x;
    uint8_t y;
};
typedef struct Uint8x2 Vec2u8;

//0..5, 6..11, 12..17, 18..23, 24..29 Block X,Y
//30 reserved (default 1)
//31 reserved (default 1)
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


class PieceGenerator{
private:
    Piece *pp;
    int pps;
public:
    PieceGenerator(Piece *piecePool, int piecePoolSize){
        pp=piecePool;
        pps=piecePoolSize;
    }
    Piece generate(){
        return pp[rand()%pps];
    }
};
PieceGenerator *globalPG;

class PieceQueue{
private:
    uint32_t baseIndex;
    uint32_t hiddenStart;
    Piece pieces[PIECEQUEUE_SIZE];
    int queue_size;
public:
    PieceQueue(){
        baseIndex=0;
        queue_size=0;
    }
    void increment(){
        for (int i=0;i<(PIECEQUEUE_SIZE-1);i++){
            pieces[i]=pieces[i+1];
        }
        baseIndex++;
        queue_size--;
    }
    void rebase(uint32_t newidx){
        int diff=newidx-baseIndex;
        if (diff==0) return;
        assert (diff>=0);
        assert (diff<queue_size);
        for (int i=0;i<diff;i++){
            increment();
        }
        assert(baseIndex==newidx);
    }
    void addPiece(Piece p){
        pieces[queue_size]=p;
        queue_size++;
        assert(queue_size<=PIECEQUEUE_SIZE);
    }
    Piece getPiece(uint32_t idx){
        int diff=idx-baseIndex;
        assert (diff>=0);
        assert (diff<queue_size);
        return pieces[diff];
    }
    bool isVisible(uint32_t idx){
        return (idx<baseIndex+queue_size);
    }
    int getQueueLength(){
        return queue_size;
    }
};

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

class GameState{
private:
    int32_t score;
    Board board;
    PieceQueue *pq;
    uint32_t currentPieceIndex;
public:
    GameState(PieceQueue *pieceQueue){
        score=0;
        board=Board();
        pq=pieceQueue;
        currentPieceIndex=0;
    }
    void incrementPieceQueue(){
        currentPieceIndex++;
    }
    Piece getCurrentPiece(){
        return pq->getPiece(currentPieceIndex);
    }
    bool currentPieceVisible(){
        return pq->isVisible(currentPieceIndex);
    }
    uint32_t getCurrentStepNum(){
        return currentPieceIndex;
    }
    Board getBoard(){
        return board;
    }
    int32_t getScore(){
        return score;
    }
    bool applyPlacement(Placement pl){
        PlacementResult pr=doPlacement(getBoard(),pl);
        if (pr.success){
            board=pr.finalResult;
            score+=pr.scoreDelta;
            incrementPieceQueue();
            return true;
        }else{
            return false;
        }
    }
};



struct DFSResult{
    Placement bestPlacement;
    int32_t score;
    bool valid;
};
typedef struct DFSResult DFSResult;

DFSResult search(GameState initialState,int depth,int targetDepth){

    DFSResult nullResult;
    nullResult.score=-100;
    nullResult.valid=false;

    if (depth>=targetDepth) return nullResult;

    DFSResult res;
    res.valid=false;

    bool piece_visible=initialState.currentPieceVisible();
    Piece currentPiece;
    if (piece_visible) currentPiece=initialState.getCurrentPiece();
    //printf("Depth %d\n",depth);
    //drawPiece(currentPiece);

    int piece_count=1;
    if (!piece_visible) piece_count=INVISBLE_PIECE_MONTE_CARLO_ITER;

    int32_t score_totals=0;
    for (int pidx=0;pidx<piece_count;pidx++){
        if (!piece_visible) {
            //printf("Invidible piece!\n");
            currentPiece=globalPG->generate();
        }

        DFSResult optimalResult=nullResult;
        //Prune loops a little with some simple bounding box calculation
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
                    //printf("Depth %d X %d Y %d\n",depth,x,y);
                    // DFS result until here
                    DFSResult dr;
                    dr.score=inState.getScore();
                    dr.bestPlacement=pl;
                    dr.valid=true;

                    // Try recursing, if successful, copy result
                    DFSResult dr_recursed=search(inState,depth+1,targetDepth);
                    if (dr_recursed.valid){
                        dr.score=dr_recursed.score;
                    }

                    // Copy result into optimal if score greatest
                    if (dr.score>optimalResult.score){
                        optimalResult=dr;
                    }
                }
            }
        }

        if (optimalResult.valid){
            score_totals+=optimalResult.score;
            res.valid=true; //vaild if at least one of the possibilities is valid.
            res.bestPlacement=optimalResult.bestPlacement;
        }else{
            score_totals+=(-1000);
        }
    }

    res.score=score_totals/piece_count;
    return res;
}

/*
 * TODO
 * Monte-carlo searching
 */

#define NUM_PIECES 64
int main(){
    Piece tmp;
    Piece pieces[NUM_PIECES];
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

    if (CONSTANT_SEED) srand(CONSTANT_SEED);
    else srand(time(nullptr));


    PieceQueue pq;
    PieceGenerator pcgn(pieces,pieceN);
    globalPG=&pcgn;
    Board lastBoard;
    GameState gs(&pq);
    while (1){

        printf("\n\n\n");
        if (MAX_GAME_STEPS){
            if (gs.getCurrentStepNum() >= MAX_GAME_STEPS){
                ansiColorSet(RED);
                printf("Maximum game length reached!\n");
                ansiColorSet(NONE);
                break;
            }
        }

        // Mimics the game
        if (!pq.isVisible(gs.getCurrentStepNum())) {
            pq.addPiece(pcgn.generate());
            pq.addPiece(pcgn.generate());
            pq.addPiece(pcgn.generate());
        }

        // Constant forward queue
        //while(!pq.isVisible(gs.getCurrentStepNum()+15)) pq.addPiece(pcgn.generate());

        pq.rebase(gs.getCurrentStepNum());





        DFSResult dfsr;
        int dfsDepth;
        if (CONSTANT_SEARCH_DEPTH){
            dfsDepth=CONSTANT_SEARCH_DEPTH;
            printf("DFS calculating... constant depth %d\n",dfsDepth);
            dfsr=search(gs,0,dfsDepth);
        }else{
            dfsDepth=1;
            while (dfsDepth<MAX_SEARCH_DEPTH){
                printf("\rDFS calculating... dynamic depth %d  ",dfsDepth);
                fflush(stdout);
                uint64_t startT=timeSinceEpochMillisec();
                dfsr=search(gs,0,dfsDepth);
                uint64_t deltaT=timeSinceEpochMillisec()-startT;
                if (deltaT>ADDITIONAL_DEPTH_THRESH) break;
                dfsDepth++;
            }
            printf("\n");
        }

        drawPieceQueue(&pq,gs.getCurrentStepNum(),PREVIEW_PIECES,dfsDepth);

        PlacementResult pr;
        Placement placement;
        if (!dfsr.valid){
            //No placement! Dead probs.
            ansiColorSet(RED);
            printf("No placement possible!\n");
            ansiColorSet(NONE);
            break;
        }
        printf("DFS max: %d\n",dfsr.score);


        placement=dfsr.bestPlacement;
        pr=doPlacement(gs.getBoard(),placement);

        gs.applyPlacement(placement);
        printf("Step %d \n",gs.getCurrentStepNum());
        printf("Score: %d (+%d)\n",gs.getScore(),pr.scoreDelta);
        drawBoardFancy(lastBoard,pr.preClear,pr.finalResult);

        lastBoard=pr.finalResult;


        //printf("Enter to coninue...\n");
        //waitForEnter();
    }

    printf("Ending game.\n");

    return 0;
}
