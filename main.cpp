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

#define PREVIEW_PIECES 6


uint64_t timeSinceEpochMillisec() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}


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
            currentPiece=getGlobalPG()->generate();
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


int main(){


    if (CONSTANT_SEED) srand(CONSTANT_SEED);
    else srand(time(nullptr));


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
