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
    void debug_print2();

    bool hasBlockAt(int x, int y);

    bool equal(Piece other);
};


class PieceGenerator{
private:
    Piece *pp;
    int pps;
public:
    PieceGenerator(Piece *piecePool, int piecePoolSize);
    Piece generate();
    void debugPrint();
};

PieceGenerator* readPieceDef(const char *filename);

#define PIECEQUEUE_SIZE 20
class PieceQueue{
private:
    uint32_t baseIndex;
    Piece pieces[PIECEQUEUE_SIZE];
    int queue_size;
public:
    PieceQueue();
    void increment();
    void rebase(uint32_t newidx);
    void addPiece(Piece p);
    void setPiece(uint32_t idx, Piece p);
    Piece getPiece(uint32_t idx);
    bool isVisible(uint32_t idx);
    int getQueueLength();
};
