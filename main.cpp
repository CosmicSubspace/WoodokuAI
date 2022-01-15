#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "assert.h"
#include "limits.h"
#include "time.h"


#include <iostream>
#include <chrono>


#include "piece.h"
#include "printutil.h"
#include "game.h"


// Crucial constant, determines the DFS search depth
// Has very heavy impact on speed and smartness.
// set to 0 for dynamic search depth adjustment.
#define CONSTANT_SEARCH_DEPTH 0

// Dynamic depth parameters.
// The program will go for an additional DFS level
// If the current DFS took less than ADDITIONAL_DEPTH_THRESH milliseconds.
#define MAX_SEARCH_DEPTH 10
#define ADDITIONAL_DEPTH_THRESH 100

// Maximum game length. 0 for unlimited.
#define MAX_GAME_STEPS 0

// A lot of code assumes 9x9 board size implicitly.
// You should probably leave this alone.
#define BOARD_SIZE 9

// Use constant seed. Useful for performance measurement.
// Use 0 to disable.
#define CONSTANT_SEED 0

#define INVISBLE_PIECE_MONTE_CARLO_ITER 10

#define PREVIEW_PIECES 5

// Randomly prune branches off when doing
// random selection on invisible pieces
// 0 - disable
// 100 - prune probability 1/(iter*100/100)
// Makes the AI super dumb, I recommend keeping it at 0.
#define MONTE_CARLO_PRUNING_RATIO 0


uint64_t timeSinceEpochMillisec() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
Board floodFillBoard(Board b, Vec2u8 start){
    Board boundary;
    Board fillResult;
    boundary.write(start.x,start.y,true);
    while (!boundary.isEmpty()){
        Vec2u8 target=boundary.getFirstFilledCell();
        if (b.read(target.x,target.y)){
            // is filled.
            fillResult.write(target.x,target.y,true);

            Vec2u8 candidate;
            for(int i=0;i<4;i++){
                if (i==0){ //+X
                    if ((target.x+1)<BOARD_SIZE){
                        candidate.x=target.x+1;
                        candidate.y=target.y;
                    }else continue;
                }
                if (i==1){ //+Y
                    if ((target.y+1)<BOARD_SIZE){
                        candidate.y=target.y+1;
                        candidate.x=target.x;
                    }else continue;
                }
                if (i==2){ //-X
                    if ((target.x)>0){
                        candidate.x=target.x-1;
                        candidate.y=target.y;
                    }else continue;
                }
                if (i==3){ //-Y
                    if ((target.y)>0){
                        candidate.y=target.y-1;
                        candidate.x=target.x;
                    }else continue;
                }

                if (fillResult.read(candidate.x,candidate.y)) continue;
                if (boundary.read(candidate.x,candidate.y)) continue;

                boundary.write(candidate.x,candidate.y,true);

            }
        }
        boundary.write(target.x,target.y,false);
    }
    return fillResult;
}

int calculateIslandness(Board b){
    int n=0;
    while (!b.isEmpty()){
        Vec2u8 start=b.getFirstFilledCell();
        Board island=floodFillBoard(b,start);

        int cellcount=island.countCells();
        if (cellcount<6) n+=10*(6-cellcount);
        else if (cellcount<10) n+=2;
        //remove this island
        b=b.bitwiseAND(island.bitwiseNOT());
    }
    return n;
}

int calculateBoardFitness(Board b){
    Board negboard=b.bitwiseNOT();
    int islandnessP=calculateIslandness(b);
    int islandnessN=calculateIslandness(negboard);
    int emptycells=negboard.countCells();
    return -(islandnessP+islandnessN*3)+emptycells*4;
}

struct DFSResult{
    Placement bestPlacement;
    int32_t compositeScore;
    bool valid;
};
typedef struct DFSResult DFSResult;

int32_t calculateCompositeScore(int sd,int bf){
    return sd*10+bf*5;
    //return bf*10;
    //return sd*10;
}

DFSResult search(GameState initialState,int depth,int targetDepth,int32_t baseScore){

    DFSResult nullResult;
    nullResult.compositeScore=-100000;
    nullResult.valid=false;

    assert (depth<targetDepth);

    DFSResult res;
    res.valid=false;

    bool piece_visible=initialState.currentPieceVisible();
    Piece currentPiece;
    if (piece_visible) currentPiece=initialState.getCurrentPiece();
    //printf("Depth %d\n",depth);
    //drawPiece(currentPiece);

    int piece_count=1;
    if (!piece_visible) piece_count=INVISBLE_PIECE_MONTE_CARLO_ITER;

    int32_t cscore_totals=0;
    for (int pidx=0;pidx<piece_count;pidx++){
        if (!piece_visible) {
            //printf("Invidible piece!\n");
            currentPiece=getGlobalPG()->generate();
        }

        DFSResult optimalResult=nullResult;
        //Prune loops a little with some simple bounding box calculation
        Vec2u8 bbox;
        bbox=currentPiece.calculateBoundingBox();
        for (int x=0;x<(9-bbox.x);x++){
            for (int y=0;y<(9-bbox.y);y++){

                if (MONTE_CARLO_PRUNING_RATIO && (piece_count>1)){
                    int r=rand();
                    if ((r%1327)*piece_count<1327){
                        //do the loop
                    }else{
                        //maybe skip the loop?
                        if (r%100<MONTE_CARLO_PRUNING_RATIO) continue;
                    }
                }
                Placement pl;
                pl.piece=currentPiece;
                pl.x=x;
                pl.y=y;

                GameState inState=initialState;

                PlacementResult pr=inState.applyPlacement(pl);
                if (pr.success){
                    //printf("Depth %d X %d Y %d\n",depth,x,y);
                    // DFS result until here
                    DFSResult dr;
                    dr.compositeScore=calculateCompositeScore(
                        inState.getScore()-baseScore,
                        calculateBoardFitness(pr.finalResult)
                    );
                    dr.bestPlacement=pl;
                    dr.valid=true;

                    // Try recursing
                    if (depth+1<targetDepth){
                        DFSResult dr_recursed=search(inState,depth+1,targetDepth,baseScore);

                        // Take the final score
                        dr.compositeScore=dr_recursed.compositeScore;
                    }

                    // Copy result into optimal if score greatest
                    if (dr.compositeScore>optimalResult.compositeScore){
                        //printf("Optimmal found %d X %d Y %d\n",depth,x,y);
                        optimalResult=dr;
                    }
                }
            }
        }

        if (optimalResult.valid){
            cscore_totals+=optimalResult.compositeScore;
            res.valid=true; //vaild if at least one of the possibilities is valid.
            res.bestPlacement=optimalResult.bestPlacement;
        }else{
            // No valid result - penalize
            cscore_totals+=(-123);
        }
    }

    res.compositeScore=cscore_totals/piece_count;
    return res;
}


int main(){

    if (CONSTANT_SEED) srand(CONSTANT_SEED);
    else srand(time(nullptr));
    /*
    Board tb1,tb2;
    for (int x=0;x<BOARD_SIZE;x++){
        for (int y=0;y<BOARD_SIZE;y++){
            if ((rand()%10)<7) tb1.write(x,y,true);
            if ((rand()%10)<5) tb2.write(x,y,true);
        }
    }
    printf("Orig1\n");
    drawBoard(tb1);
    Vec2u8 ffs;
    ffs.x=1; ffs.y=1;
    Board ff=floodFillBoard(tb1,ffs);
    printf("FF'd\n");
    drawBoard(ff);

    printf("Orig1\n");
    drawBoard(tb1);
    printf("Orig2\n");
    drawBoard(tb2);
    printf("AND'd\n");
    drawBoard(tb1.bitwiseAND(tb2));
    printf("OR'd\n");
    drawBoard(tb1.bitwiseOR(tb2));
    printf("NOT'd\n");
    drawBoard(tb1.bitwiseNOT());

    printf("Orig1\n");
    drawBoard(tb1);
    printf("Islands:%d\n",numIslands(tb1));

    return 0;*/




    PieceQueue pq;
    PieceGenerator *pgen=getGlobalPG();
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
            pq.addPiece(pgen->generate());
            pq.addPiece(pgen->generate());
            pq.addPiece(pgen->generate());
        }

        // Constant forward queue
        //while(!pq.isVisible(gs.getCurrentStepNum()+15)) pq.addPiece(pgen->generate());

        pq.rebase(gs.getCurrentStepNum());





        DFSResult dfsr;
        int dfsDepth;
        if (CONSTANT_SEARCH_DEPTH){
            dfsDepth=CONSTANT_SEARCH_DEPTH;
            printf("DFS calculating... constant depth %d\n",dfsDepth);
            dfsr=search(gs,0,dfsDepth,gs.getScore());
        }else{
            dfsDepth=1;
            while (dfsDepth<MAX_SEARCH_DEPTH){
                printf("\rDFS calculating... dynamic depth %d  ",dfsDepth);
                fflush(stdout);
                uint64_t startT=timeSinceEpochMillisec();
                dfsr=search(gs,0,dfsDepth,gs.getScore());
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
        printf("DFS max comp.score: %d\n",dfsr.compositeScore);


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
