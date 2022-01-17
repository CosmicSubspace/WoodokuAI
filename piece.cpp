#include "piece.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>

Piece::Piece(){
    data=0xFFFFFFFF;
}
int Piece::numBlocks(){
    uint32_t p=data;
    int n=0;
    while ((p&(0x3F)) != 0x3F){
        n++;
        p=p>>6;
        if (n==5) break;
    }
    return n;
}
Vec2u8 Piece::getBlock(int idx){
    uint32_t p=data;
    p=p>>(6*idx);
    Vec2u8 b;
    b.x=p&(0x07);
    p=p>>3;
    b.y=p&(0x07);
    return b;
}
Vec2u8 Piece::calculateBoundingBox(){
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
void Piece::addBlock(int x, int y){
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
void Piece::debug_print(){
    printf("Piece: ");
    for (int i=0;i<numBlocks();i++){
        Vec2u8 b=getBlock(i);
        printf("(%d,%d) ",b.x,b.y);
    }
    printf("\n");
}

bool Piece::hasBlockAt(int x, int y){
    // Very unoptimized. Only call in debug/infrequent code plz
    for(int i=0;i<numBlocks();i++){
        Vec2u8 b=getBlock(i);
        if ((b.x==x) && (b.y==y)) return true;
    }
    return false;
}
bool Piece::equal(Piece other){
    return data==other.data;
}

PieceGenerator::PieceGenerator(Piece *piecePool, int piecePoolSize){
    pp=piecePool;
    pps=piecePoolSize;
}
Piece PieceGenerator::generate(){
    return pp[rand()%pps];
}

PieceQueue::PieceQueue(){
    baseIndex=0;
    queue_size=0;
}
void PieceQueue::increment(){
    for (int i=0;i<(PIECEQUEUE_SIZE-1);i++){
        pieces[i]=pieces[i+1];
    }
    baseIndex++;
    queue_size--;
}
void PieceQueue::rebase(uint32_t newidx){
    int diff=newidx-baseIndex;
    if (diff==0) return;
    assert (diff>=0);
    assert (diff<queue_size);
    for (int i=0;i<diff;i++){
        increment();
    }
    assert(baseIndex==newidx);
}
void PieceQueue::addPiece(Piece p){
    pieces[queue_size]=p;
    queue_size++;
    assert(queue_size<=PIECEQUEUE_SIZE);
}
Piece PieceQueue::getPiece(uint32_t idx){
    int diff=idx-baseIndex;
    assert (diff>=0);
    assert (diff<queue_size);
    return pieces[diff];
}
bool PieceQueue::isVisible(uint32_t idx){
    return (idx<baseIndex+queue_size);
}
int PieceQueue::getQueueLength(){
    return queue_size;
}
void PieceQueue::setPiece(uint32_t idx, Piece p){
    int diff=idx-baseIndex;
    if (diff==queue_size) addPiece(p);
    else{
        assert (diff>=0);
        assert (diff<queue_size);
        pieces[diff]=p;
    }

}

#define NUM_PIECES 64
PieceGenerator *globalPG=nullptr;

void initializeGlobalPG(){
    Piece tmp;
    int pieceN=0;
    Piece *pieces= new Piece[NUM_PIECES];

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

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    tmp.addBlock(1,2);
    tmp.addBlock(2,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(2,1);
    tmp.addBlock(2,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(2,0);
    tmp.addBlock(2,1);
    tmp.addBlock(0,2);
    tmp.addBlock(1,2);
    tmp.addBlock(2,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(0,1);
    tmp.addBlock(0,2);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(2,0);
    tmp.addBlock(0,1);
    tmp.addBlock(1,1);
    tmp.addBlock(2,1);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(1,1);
    tmp.addBlock(0,2);
    tmp.addBlock(1,2);
    pieces[pieceN++]=tmp;

    tmp=Piece();
    tmp.addBlock(0,0);
    tmp.addBlock(1,0);
    tmp.addBlock(2,0);
    tmp.addBlock(0,1);
    tmp.addBlock(2,1);
    pieces[pieceN++]=tmp;


    globalPG= new PieceGenerator(pieces,pieceN);
}

PieceGenerator* getGlobalPG(){
    if (globalPG==nullptr){
        initializeGlobalPG();
    }
    return globalPG;
}







