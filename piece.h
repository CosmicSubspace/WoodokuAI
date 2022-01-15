#pragma once

#include "stdint.h"

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
    Piece();

    int numBlocks();
    Vec2u8 getBlock(int idx);
    Vec2u8 calculateBoundingBox();
    void addBlock(int x, int y);

    void debug_print();
    bool hasBlockAt(int x, int y);
};


class PieceGenerator{
private:
    Piece *pp;
    int pps;
public:
    PieceGenerator(Piece *piecePool, int piecePoolSize);
    Piece generate();
};

PieceGenerator* getGlobalPG();

#define PIECEQUEUE_SIZE 20
class PieceQueue{
private:
    uint32_t baseIndex;
    uint32_t hiddenStart;
    Piece pieces[PIECEQUEUE_SIZE];
    int queue_size;
public:
    PieceQueue();
    void increment();
    void rebase(uint32_t newidx);
    void addPiece(Piece p);
    Piece getPiece(uint32_t idx);
    bool isVisible(uint32_t idx);
    int getQueueLength();
};
