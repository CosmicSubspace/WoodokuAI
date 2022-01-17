#pragma once

#include <cstdint>

#include "piece.h"

#define BOARD_SIZE 9

struct Placement{
    Piece piece;
    uint8_t x;
    uint8_t y;
};
typedef struct Placement Placement;



class Board{
private:
    uint32_t bitfield[3];
    int coord2idx(int x, int y);
    Vec2u8 idx2coord(int idx);
public:
    Board();
    bool read(int x, int y);
    void write(int x, int y,bool value);
    bool isEmpty();
    Vec2u8 getFirstFilledCell();
    Board bitwiseOR(Board other);
    Board bitwiseAND(Board other);
    Board bitwiseNOT();
    int countCells();
    bool equal(Board other);
};



struct PlacementResult{
    bool success;
    Board preClear;
    Board finalResult;
    int scoreDelta;
};
typedef struct PlacementResult PlacementResult;

PlacementResult doPlacement(Board b,Placement pl);


class GameState{
private:
    int32_t score;
    Board board;
    uint32_t currentPieceIndex;
public:
    GameState();
    void incrementPieceQueue();
    uint32_t getCurrentStepNum();
    Board getBoard();
    void setBoard(Board b);
    int32_t getScore();
    PlacementResult applyPlacement(Placement pl);
};
