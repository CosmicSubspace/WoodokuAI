#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "assert.h"

struct Block{
    uint8_t x;
    uint8_t y;
};
typedef struct Block Block;

typedef uint32_t Piece;
const Piece initialPiece=0xFFFFFFFF;
int numBlocks(Piece p){
    int n=0;
    while ((p&(0x3F)) != 0x3F){
        n++;
        p=p>>6;
        if (n==5) break;
    }
    return n;
}
Block getBlock(Piece p, int idx){
    p=p>>(6*idx);
    Block b;
    b.x=p&(0x07);
    p=p>>3;
    b.y=p&(0x07);
    return b;
}

Piece buildPiece(Piece p, int x, int y){
    uint32_t block=0;
    uint32_t mask=0x3F;
    block+=y;
    block=block<<3;
    block+=x;
    for(int i=0;i<5;i++){
        if ((p&mask)==mask){
            // untouched... write here.
            return (p^mask)|block;
        }
        mask=mask<<6;
        block=block<<6;
    }
    return 0xFFFFFFFF;

}


struct Placement{
    Piece piece;
    uint8_t x;
    uint8_t y;
};
typedef struct Placement Placement;

#define MAX_GAME_STEPS 100
struct PlacementSequence{
    Placement placements[MAX_GAME_STEPS];
    int length;
};
typedef struct PlacementSequence PlacementSequence;
PlacementSequence emptyPSq(){
    PlacementSequence psq;
    psq.length=0;
    return psq;
}
void addPlacement(PlacementSequence *psq, Placement p){
    psq->placements[psq->length]=p;
    psq->length++;
}


#define BOARD_SIZE 9
struct Board{
    uint32_t bitfield[3];
};
typedef struct Board Board;
Board newBoard(){
    Board b;
    b.bitfield[0]=0;
    b.bitfield[1]=0;
    b.bitfield[2]=0;
    return b;
}

int coord2idx(int x, int y){
    return x+y*BOARD_SIZE;
}
 bool readBoard(Board *b, int x, int y){
    int idx=coord2idx(x,y);
    int varnum=idx/32;
    int bitnum=idx%32;
    return ((b->bitfield[varnum])>>bitnum) & 1;
}
 void writeBoard(Board *b, int x, int y,bool value){
    int idx=coord2idx(x,y);
    int varnum=idx/32;
    int bitnum=idx%32;
    if (value) b->bitfield[varnum] |= (1<<bitnum);
    else b->bitfield[varnum] &= ~(1<<bitnum);
}
void drawBoard(Board b){
    char buffer[(BOARD_SIZE*2+1)*BOARD_SIZE+1];
    int idx=0;
    for (int y=0;y<BOARD_SIZE;y++){
        for (int x=0;x<BOARD_SIZE;x++){
            if (readBoard(&b,x,y)!=0) {
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


struct PlacementResult{
    Board result;
    bool success;
    Board afterDeletion;
    int scoreDelta;
};
typedef struct PlacementResult PlacementResult;
PlacementResult doPlacement(Board b,Placement pl){
    PlacementResult pr;

    int n=numBlocks(pl.piece);
    //printf("NumBlocks: %d\n",n);
    bool success=true;
    for(int i=0;i<n;i++){
        Block block=getBlock(pl.piece,i);
        int blockX=block.x;
        int blockY=block.y;
        int absX=blockX+pl.x;
        int absY=blockY+pl.y;
        //printf("Block:%d %d\n",absX,absY);

        if (absX<0 || absX>=BOARD_SIZE) success=false;
        else if (absY<0 || absY>=BOARD_SIZE) success=false;
        else if (readBoard(&b,absX,absY) !=0) success=false;

        writeBoard(&b,absX,absY,true);
    }

    pr.result=b;
    pr.success=success;

    //check lines
    uint8_t checks[27];
    for (int i=0;i<27;i++) checks[i]=0xFF;
    uint8_t *xLines=&checks[0];
    uint8_t *yLines=&(checks[9]);
    uint8_t *squares=&(checks[18]);
    for (int x=0;x<BOARD_SIZE;x++){
        for (int y=0;y<BOARD_SIZE;y++){
            uint8_t cell=readBoard(&b,x,y);
            int square=3*(y/3)+(x/3);
            xLines[x] &= cell;
            yLines[y] &= cell;
            squares[square] &= cell;
        }
    }

    int count=0;
    for (int i=0;i<27;i++) {
        if (checks[i]) count++;
    }

    int bonus=0;
    if (count>1) bonus=10*(count-1);
    int score= count*18+bonus+n;

    for (int x=0;x<BOARD_SIZE;x++){
        for (int y=0;y<BOARD_SIZE;y++){
            int square=3*(y/3)+(x/3);
            if (xLines[x] != 0) writeBoard(&b,x,y,false);
            if (yLines[y] != 0) writeBoard(&b,x,y,false);
            if (squares[square] != 0) writeBoard(&b,x,y,false);
        }
    }
    pr.afterDeletion=b;
    pr.scoreDelta=score;

    return pr;
}


int numPieces=0;
Piece pieces[1000];



struct PieceQueue{
    int numPieces;
    Piece *pieces;
    int currentIndex;
};
typedef struct PieceQueue PieceQueue;

struct GameState{
    int score;
    Board board;
    PieceQueue pq;
    PlacementSequence psq;
};
typedef struct GameState GameState;
GameState initialGameState(PieceQueue pq){
    GameState gs;
    gs.score=0;
    gs.board=newBoard();
    gs.pq=pq;
    gs.psq=emptyPSq();
    return gs;
}

GameState search(GameState gs,int depth){
    if (depth<=0) return gs;



    GameState optimalState;
    optimalState.score=-1;
    for (int x=0;x<9;x++){
        for (int y=0;y<9;y++){
            Placement pl;
            pl.piece=gs.pq.pieces[gs.pq.currentIndex];
            pl.x=x;
            pl.y=y;
            PlacementResult pr=doPlacement(gs.board,pl);
            if (pr.success){
                GameState inState;
                inState.board=pr.result;
                inState.score=gs.score+pr.scoreDelta;
                inState.pq=gs.pq;
                inState.pq.currentIndex++;
                inState.psq=gs.psq;
                addPlacement(&inState.psq,pl);
                GameState outState=search(inState,depth-1);

                if (outState.score>optimalState.score) optimalState=outState;
            }
        }
    }
    return optimalState;
}


int main(){
    Piece p,t1,r1,o1,x1,i1;

    p=initialPiece;
    p=buildPiece(initialPiece,0,0);
    p=buildPiece(p,1,0);
    p=buildPiece(p,2,0);
    p=buildPiece(p,1,1);
    t1=p;
    pieces[numPieces] = t1;
    numPieces++;

    p=initialPiece;
    p=buildPiece(p,0,0);
    p=buildPiece(p,1,0);
    p=buildPiece(p,0,1);
    r1=p;
    pieces[numPieces] = r1;
    numPieces++;

    p=initialPiece;
    p=buildPiece(p,0,1);
    p=buildPiece(p,1,1);
    p=buildPiece(p,1,0);
    p=buildPiece(p,1,2);
    p=buildPiece(p,2,1);
    x1=p;
    pieces[numPieces] = x1;
    numPieces++;

    p=initialPiece;
    p=buildPiece(p,0,1);
    p=buildPiece(p,0,0);
    p=buildPiece(p,0,3);
    p=buildPiece(p,0,2);
    i1=p;
    pieces[numPieces] = i1;
    numPieces++;

    PieceQueue pq;
    pq.numPieces=0;
    pq.currentIndex=0;
    pq.pieces= (Piece *) malloc(sizeof(Piece)*MAX_GAME_STEPS);

    pq.pieces[pq.numPieces++]=t1;
    pq.pieces[pq.numPieces++]=t1;
    pq.pieces[pq.numPieces++]=i1;
    pq.pieces[pq.numPieces++]=x1;
    pq.pieces[pq.numPieces++]=r1;
    pq.pieces[pq.numPieces++]=r1;


    Placement pl;
    pl.piece=p;
    pl.x=2;
    pl.y=2;


    Board b=newBoard();
    //b.bitfield[0]=0xDEADBEEF;
    writeBoard(&b,2,2,true);
    /*
    printf("Placing #1: %d\n",tryAndPlace(&piecedefs[0],board,0,0));
    printf("Placing #2: %d\n",tryAndPlace(&piecedefs[0],board,0,0));
    printf("Placing #3: %d\n",tryAndPlace(&piecedefs[0],board,3,3));*/

    PlacementResult pr=doPlacement(b,pl);
    printf("Placing #1: success? %d\n",pr.success);
    printf("Placing #1: Board\n");
    drawBoard(pr.result);
    printf("Placing #1: Deletion\n");
    drawBoard(pr.afterDeletion);
    printf("Placing #1: score %d\n",pr.scoreDelta);

    printf("DFS start...\n");
    GameState igs;
    igs.board=newBoard();
    igs.score=0;
    igs.pq=pq;
    GameState fgs=search(igs,4);
    printf("DFS max: %d\n",fgs.score);
    drawBoard(fgs.board);

    //Replay
    PlacementSequence rp_psq=fgs.psq;
    Board rp_b=newBoard();
    for(int i=0;i<rp_psq.length;i++){
        PlacementResult pr=doPlacement(rp_b,rp_psq.placements[i]);
        rp_b=pr.afterDeletion;
        printf("Step %d Score delta: %d\n",i,pr.scoreDelta);
        drawBoard(rp_b);
    }
    return 0;
}
