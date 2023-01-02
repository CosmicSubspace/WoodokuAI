#include "piece.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>

#include <vector>

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
void Piece::debug_print2(){
    Vec2u8 bbox=calculateBoundingBox();
    for (int y=0;y<=bbox.y;y++){
        for (int x=0;x<=bbox.x;x++){
            if (hasBlockAt(x,y)) printf("#");
            else printf(" ");
        }
        printf("\n");
    }
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
void PieceGenerator::debugPrint(){
    printf("\nPieceGenerator: %d pieces.\n",pps);
    for(int i=0;i<pps;i++){
        printf("Piece %d:\n",i);
        pp[i].debug_print2();
        printf("\n");
    }
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

PieceGenerator* readPieceDef(const char *filename){
    FILE *f=fopen(filename,"r");

    int x=0;
    int y=0;
    Piece p;
    bool emptyLine=true;
    std::vector<Piece> *pieces=new std::vector<Piece>();
    while (1){
        int r=fgetc(f);
        if (r==EOF) break;
        switch (r){
            case '\n':
                x=0;
                y++;

                if (emptyLine){
                    // Empty line - end of piece!
                    if (p.numBlocks()>0){
                        pieces->push_back(p);
                    }
                    y=0;
                    p=Piece();
                }

                emptyLine=true;

                break;
            case ' ':
                emptyLine=false;
                x++;
                break;
            case '#':
                emptyLine=false;
                p.addBlock(x,y);
                x++;
                break;
            default:
                printf("Invalid character in piece definition file: %c",r);
                exit(1);
        }
    }

    fclose(f);

    return new PieceGenerator(pieces->data(),pieces->size());
}





