#include "game.h"
#include "piece.h"
int Board::coord2idx(int x, int y){
    return x+y*BOARD_SIZE;
}
Vec2u8 Board::idx2coord(int idx){
    Vec2u8 res;
    res.x=idx%BOARD_SIZE;
    res.y=idx/BOARD_SIZE;
    return res;
}
Board::Board(){
    bitfield[0]=0;
    bitfield[1]=0;
    bitfield[2]=0;
}
bool Board::read(int x, int y){
    int idx=coord2idx(x,y);
    int varnum=idx/32;
    int bitnum=idx%32;
    return ((bitfield[varnum])>>bitnum) & 1;
}
void Board::write(int x, int y,bool value){
    int idx=coord2idx(x,y);
    int varnum=idx/32;
    int bitnum=idx%32;
    if (value) bitfield[varnum] |= (1<<bitnum);
    else bitfield[varnum] &= ~(1<<bitnum);
}
bool Board::isEmpty(){
    return (bitfield[0] | bitfield[1] |  bitfield[2])==0;
}
Vec2u8 Board::getFirstFilledCell(){
    int idx=0;
    uint32_t bitselect=1;
    for(int i=0;i<3;i++){
        if (bitfield[i] !=0){ //there's a bit here
            while (1){
                if ((bitfield[i])&bitselect){
                    return idx2coord(idx);
                }
                bitselect<<=1;
                idx++;
            }
        }
        idx+=32;
    }
    return Vec2u8();
}
Board Board::bitwiseAND(Board other){
    Board res;
    for (int i=0;i<3;i++){
        res.bitfield[i]=bitfield[i] & other.bitfield[i];
    }
    return res;
}
Board Board::bitwiseOR(Board other){
    Board res;
    for (int i=0;i<3;i++){
        res.bitfield[i]=bitfield[i] | other.bitfield[i];
    }
    return res;
}
Board Board::bitwiseNOT(){
    Board res;
    for (int i=0;i<3;i++){
        res.bitfield[i]= ~bitfield[i];
    }
    return res;
}
int Board::countCells(){
    int res=0;
    for (int i=0;i<3;i++){
        for (int b=0;b<32;b++){
            if (bitfield[i] & (1<<b)) res++;
        }
    }
    return res;
}

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


GameState::GameState(PieceQueue *pieceQueue){
    score=0;
    board=Board();
    pq=pieceQueue;
    currentPieceIndex=0;
}
void GameState::incrementPieceQueue(){
    currentPieceIndex++;
}
Piece GameState::getCurrentPiece(){
    return pq->getPiece(currentPieceIndex);
}
bool GameState::currentPieceVisible(){
    return pq->isVisible(currentPieceIndex);
}
uint32_t GameState::getCurrentStepNum(){
    return currentPieceIndex;
}
Board GameState::getBoard(){
    return board;
}
int32_t GameState::getScore(){
    return score;
}
PlacementResult GameState::applyPlacement(Placement pl){
    PlacementResult pr=doPlacement(getBoard(),pl);
    if (pr.success){
        board=pr.finalResult;
        score+=pr.scoreDelta;
        incrementPieceQueue();
    }
    return pr;
}
PieceQueue* GameState::getPQ(){
    return pq;
}
