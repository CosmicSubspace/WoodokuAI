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
#include "woodoku_client.h"


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

#define MAX_RANDSEARCH_ITER 30
#define MIN_RANDSEARCH_ITER 5

#define PREVIEW_PIECES 5

// Connect to a game server.
#define SERVER_GAME


// Disable board-fitness heuristic
#define DISABLE_BOARD_FITNESS_HEURISTIC



uint64_t timeSinceEpochMillisec() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

#ifndef DISABLE_BOARD_FITNESS_HEURISTIC
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
    //return emptycells;
}
#endif
#ifdef DISABLE_BOARD_FITNESS_HEURISTIC
int calculateBoardFitness(Board b){
    return 0;
}
#endif
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

DFSResult search(GameState initialState,int depth,int targetDepth,int32_t baseScore, PieceQueue *pqOverride){

    DFSResult nullResult;
    nullResult.compositeScore=-100000;
    nullResult.valid=false;

    assert (depth<targetDepth);


    Piece currentPiece;
    PieceQueue *pq;
    if (pqOverride != nullptr){
        pq=pqOverride;
    }else{
        pq=initialState.getPQ();
    }
    assert (pq->isVisible(initialState.getCurrentStepNum()));
    currentPiece=pq->getPiece(initialState.getCurrentStepNum());
    //printf("Depth %d\n",depth);
    //drawPiece(currentPiece);





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
                    DFSResult dr_recursed=search(inState,depth+1,targetDepth,baseScore, pqOverride);

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

    return optimalResult;
}

struct SearchResult{
    Placement optimalPlacement;
    int searchDepth;
    bool isValid;
};
typedef struct SearchResult SearchResult;

SearchResult searchHL(GameState gs, int depth, uint64_t timelimit){
    //DFSResult dfsrs[MAX_RANDSEARCH_ITER];
    bool hasInvisible=false;
    int actualRandSearchN=0;
    Piece nextPiece=gs.getPQ()->getPiece(gs.getCurrentStepNum());


    for (int i=0;i<depth;i++){
        if (!gs.getPQ()->isVisible(gs.getCurrentStepNum()+i)){
            hasInvisible=true;
        }}

    Placement uniquePlacements[MAX_RANDSEARCH_ITER];
    int numUniquePlacements=0;
    int uniquePlacementCount[MAX_RANDSEARCH_ITER];
    int maxCount=0;
    int maxIdx=-1;
    int invalids=0;

    int mci=0;
    while (1){
        printf("\rSearching... depth %d, randiter %d   ",depth,mci);
        fflush(stdout);
        PieceQueue tmpPQ=*(gs.getPQ());

        for (int i=0;i<depth;i++){
            if (!tmpPQ.isVisible(gs.getCurrentStepNum()+i)){
                tmpPQ.setPiece(
                    gs.getCurrentStepNum()+i,
                    getGlobalPG()->generate());
            }
        }

        DFSResult dfsr=search(gs,0,depth,gs.getScore(),&tmpPQ);
        //dfsrs[mci]=dfsr;
        actualRandSearchN++;

        if (dfsr.valid){
            // sanity
            Piece placementPiece=dfsr.bestPlacement.piece;
            assert (placementPiece.equal(nextPiece));

            int duplicateOf=-1;
            Placement p1=dfsr.bestPlacement;
            for(int i=0;i<numUniquePlacements;i++){
                Placement p2=uniquePlacements[i];
                if ((p1.x==p2.x) && (p1.y==p2.y)){
                    duplicateOf=i;
                    break;
                }
            }
            if (duplicateOf==-1){
                uniquePlacements[numUniquePlacements]=p1;
                uniquePlacementCount[numUniquePlacements]=1;
                duplicateOf=numUniquePlacements;
                numUniquePlacements++;
            }else{
                uniquePlacementCount[duplicateOf]++;
            }
            if (uniquePlacementCount[duplicateOf]>maxCount){
                maxCount=uniquePlacementCount[duplicateOf];
                maxIdx=duplicateOf;
            }
        }else{
            invalids++;
        }
        //All pieces visible - just return now
        if (!hasInvisible) {
            printf("\n  Determined future - breaking!\n");
            break;
        }
        if (timeSinceEpochMillisec()>timelimit){
            printf("\n  Timeout\n");
            break;
        }


        mci++;
        if (mci==MAX_RANDSEARCH_ITER){
            printf("\n  Max randiter reached\n");
            break;
        }


    }

    bool sufficientIterations=(!hasInvisible) || (mci>=MIN_RANDSEARCH_ITER);

    SearchResult sr;
    sr.searchDepth=depth;
    if (sufficientIterations){
        for(int i=0;i<numUniquePlacements;i++){
            printf("  X %d Y %d (%d hits)",
                    uniquePlacements[i].x,
                    uniquePlacements[i].y,
                    uniquePlacementCount[i]);
            if (uniquePlacementCount[i]==maxCount) printf(" *");
            if (i==maxIdx) printf(" <<");
            printf("\n");
        }
        if (invalids>0){
            ansiColorSet(RED_BRIGHT);
            printf("  Invalid:%d\n",invalids);
            ansiColorSet(NONE);
        }

        if (maxIdx==-1){
            // No results
            sr.isValid=false;

        }else {
            sr.isValid=true;
            sr.optimalPlacement=uniquePlacements[maxIdx];
        }

    }else{
        printf("  Insufficient iterations, ignoring...\n");
        sr.isValid=false;
    }





    return sr;
}

SearchResult dynamicDepthSearch(GameState gs, uint64_t maxMillis){
    uint64_t timelimit=timeSinceEpochMillisec()+maxMillis;

    SearchResult searchres[MAX_SEARCH_DEPTH];
    SearchResult finalResult;
    finalResult.isValid=false;
    finalResult.searchDepth=0;
    int depth=1;
    while (depth<MAX_SEARCH_DEPTH){
        if (timeSinceEpochMillisec()>timelimit) break;
        SearchResult sr=searchHL(gs,depth,timelimit);
        searchres[depth]=sr;
        if (sr.isValid) finalResult=sr;
        depth++;
    }

    return finalResult;

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

#ifdef SERVER_GAME
    WoodokuClient wc;
#endif

    PieceQueue pq;
    PieceGenerator *pgen=getGlobalPG();
    Board lastBoard;
    GameState gs(&pq);
    while (1){
#ifndef SERVER_GAME
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
#endif
#ifdef SERVER_GAME
        ServerState ss;
        int waitN=0;
        while (!wc.getServerStateUpdate(&ss)){
            // wait for server
            printf("\rwait for server");
            waitN=(waitN+1)%10;
            for (int i=0;i<10;i++){
                if (i<waitN) printf(".");
                else printf(" ");
            }
            fflush(stdout);
            usleep(100*1000);
        }
        printf("\n");
        bool mismatched=false;
        Board serverBoard;
        for(int y=0;y<BOARD_SIZE;y++){
            for (int x=0;x<BOARD_SIZE;x++){
                bool boardcell=ss.boardState[x+y*BOARD_SIZE];
                serverBoard.write(x,y,boardcell);
            }
        }

        if (!serverBoard.equal(gs.getBoard())){
            ansiColorSet(RED);
            printf("Board mismatch\n");
            ansiColorSet(NONE);
            printf("Overridden board:\n");
            gs.setBoard(serverBoard);
            drawBoard(gs.getBoard());
        }


        Piece servPieces[3];
        for (int x=0;x<5;x++){
            for (int y=0;y<5;y++){
                int idx=x+y*5;
                if (ss.piece1[idx]) servPieces[0].addBlock(x,y);
                if (ss.piece2[idx]) servPieces[1].addBlock(x,y);
                if (ss.piece3[idx]) servPieces[2].addBlock(x,y);
            }
        }



        for (int i=0;i<3;i++){
            if (servPieces[i].numBlocks()>0){
                pq.setPiece(gs.getCurrentStepNum()+i,
                            servPieces[i]);
            }
        }
#endif
        pq.rebase(gs.getCurrentStepNum());






        SearchResult sr;
        sr=dynamicDepthSearch(gs,5000);
        printf("Search result: depth %d\n",sr.searchDepth);
        drawPieceQueue(&pq,gs.getCurrentStepNum(),PREVIEW_PIECES,sr.searchDepth);

        PlacementResult pr;
        Placement placement;
        if (!sr.isValid){
            //No placement! Dead probs.
            ansiColorSet(RED);
            printf("No placement possible!\n");
            ansiColorSet(NONE);
            break;
        }


        placement=sr.optimalPlacement;
        pr=doPlacement(gs.getBoard(),placement);

        gs.applyPlacement(placement);
        printf("Step %d \n",gs.getCurrentStepNum());
        printf("Score: %d (+%d)\n",gs.getScore(),pr.scoreDelta);
        drawBoardFancy(lastBoard,pr.preClear,pr.finalResult);

        lastBoard=pr.finalResult;
#ifdef SERVER_GAME
        ClientMove cm;
        for(int x=0;x<5;x++){
            for (int y=0;y<5;y++){
                cm.shape[x+y*5]=placement.piece.hasBlockAt(x,y);
            }
        }
        cm.x=placement.x;
        cm.y=placement.y;
        wc.commitMove(&cm);
#endif
        //printf("Enter to coninue...\n");
        //waitForEnter();
    }

    printf("Ending game.\n");

    return 0;
}
