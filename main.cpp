#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "assert.h"
#include "limits.h"
#include "time.h"

#include <thread>
#include <iostream>
#include <chrono>
#include <mutex>

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
#define MIN_RANDSEARCH_ITER 10

#define PREVIEW_PIECES 10

// Connect to a game server.
//#define SERVER_GAME


// Disable board-fitness heuristic
//#define DISABLE_BOARD_FITNESS_HEURISTIC


#define NUM_THREADS 4

#define TARGET_MILLISEC_PER_TURN 5000

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
        if (cellcount<5) n+=10*(5-cellcount);
        //else if (cellcount<10) n+=2;
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
    return -(islandnessP+islandnessN*3)+emptycells*2;
    //return emptycells;
    //return -(islandnessN)+emptycells*10;
}
#endif
#ifdef DISABLE_BOARD_FITNESS_HEURISTIC
int calculateBoardFitness(Board b){
    return 0;
}
#endif
struct DFSResult{
    Placement bestPlacement;
    int32_t boardFitness;
    int32_t scoreDelta;
    bool valid;
    bool computationInterrupted;
};
typedef struct DFSResult DFSResult;

int32_t calculateCompositeScore(int sd,int bf){
    return sd*100+bf*1;
    //return bf*10;
    //return sd*10;
}

DFSResult search(GameState initialState,int depth,int targetDepth,int32_t baseScore, PieceQueue *pq, bool *killRequest){

    DFSResult nullResult;
    nullResult.valid=false;
    nullResult.computationInterrupted=false;
    nullResult.boardFitness=-123456;
    nullResult.scoreDelta=-123457;

    if (*killRequest) {
        nullResult.computationInterrupted=true;
        return nullResult;
    }
    assert (depth<targetDepth);


    Piece currentPiece;

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
                dr.scoreDelta=inState.getScore()-baseScore;
                dr.boardFitness=calculateBoardFitness(pr.finalResult);
                dr.bestPlacement=pl;
                dr.valid=true;
                dr.computationInterrupted=false;

                // Try recursing
                if (depth+1<targetDepth){
                    DFSResult dr_recursed=search(inState,depth+1,targetDepth,baseScore, pq, killRequest);
                    if (dr_recursed.valid){
                        // Take the final score
                        dr.scoreDelta=dr_recursed.scoreDelta;
                        dr.boardFitness=dr_recursed.boardFitness;
                    }else{
                        // If none of the the futures lead anywhere
                        // this branch is dead
                        dr.valid=false;
                    }
                }

                if (dr.valid){
                    if (!optimalResult.valid){
                        optimalResult=dr;
                    }
                    // Copy result into optimal if score greatest
                    int32_t cs_this=calculateCompositeScore(
                        dr.scoreDelta,dr.boardFitness
                    );
                    int32_t cs_optimal=calculateCompositeScore(
                        optimalResult.scoreDelta,optimalResult.boardFitness
                    );
                    if (cs_this>cs_optimal){
                        //printf("Optimmal found %d X %d Y %d\n",depth,x,y);
                        optimalResult=dr;
                    }
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

struct SearchRequest{
    PieceQueue pq;
    GameState gs;
    int depth;
    bool started;
    bool finished;
    DFSResult result;
};
typedef SearchRequest SearchRequest;

std::mutex threadMtx;
bool threadKillRequest;
std::thread workerThreads[NUM_THREADS];

SearchRequest threadData[MAX_SEARCH_DEPTH*MAX_RANDSEARCH_ITER];
int nextWorkIdx;
int workCount;
int doneCount;

int fordisp_completecount[MAX_SEARCH_DEPTH];
int fordisp_workcount[MAX_SEARCH_DEPTH];
int fordisp_inprogcount[MAX_SEARCH_DEPTH];

void threadFunc();
void initializeThreadData(GameState gs,PieceQueue *pq){
    threadKillRequest=false;
    nextWorkIdx=0;
    workCount=0;
    doneCount=0;
    uint32_t currentStep=gs.getCurrentStepNum();
/*
    printf("\nITD PQ:\n");
    printf("CSN %d\n",gs.getCurrentStepNum());
    drawPieceQueue(pq,gs.getCurrentStepNum(),5,5);*/

    for (int di=1;di<MAX_SEARCH_DEPTH;di++){
        int iters=1;
        for(int i=0;i<di;i++){
            if (!pq->isVisible(currentStep+i)) {
                iters=MAX_RANDSEARCH_ITER;
                break;
            }
        }

        fordisp_workcount[di]=iters;
        fordisp_completecount[di]=0;
        fordisp_inprogcount[di]=0;


        for(int ri=0;ri<iters;ri++){
            SearchRequest srq;
            srq.pq=(*pq);
/*
            printf("SRQ ri %d LOOPSTART \n",
                    ri);
            printf("PQ\n");
            drawPieceQueue(pq,currentStep,5,5);
            printf("SRQ.PQ\n");
            drawPieceQueue(&srq.pq,currentStep,5,5);*/

            for (int i=0;i<di;i++){
                /*
                printf("SRQ PQ i %d di %d CS %d Vis%d \n",
                       i,di,currentStep,
                       srq.pq.isVisible(currentStep+i));
                drawPieceQueue(&srq.pq,currentStep,5,5);*/

                if (!srq.pq.isVisible(currentStep+i)){
                    srq.pq.setPiece(
                        currentStep+i,
                        getGlobalPG()->generate());
                }

            }
            srq.gs=gs;
            srq.started=false;
            srq.finished=false;
            srq.depth=di;
            /*
            printf("Queueing: depth %d ri %d\n",
                   srq.depth,ri);*/

            threadData[workCount]=srq;
            workCount++;
        }
    }
}
void threadFunc(){
    while(1){
        if (threadKillRequest) return;

        threadMtx.lock();
        if (nextWorkIdx==workCount){
             // No work left.
            threadMtx.unlock();
            return;
        }


        int thisIndex=nextWorkIdx;
        nextWorkIdx++;
        GameState gs=threadData[thisIndex].gs;
        int depth=threadData[thisIndex].depth;
        PieceQueue pq=threadData[thisIndex].pq;
        fordisp_inprogcount[depth]++;
        threadData[thisIndex].started=true;
        threadMtx.unlock();

        DFSResult dfsr=search(gs,
                              0,
                              depth,
                              gs.getScore(),
                              &pq,
                              &threadKillRequest);

        threadMtx.lock();
        if (!threadKillRequest){
            assert (!dfsr.computationInterrupted);
            threadData[thisIndex].result=dfsr;
            threadData[thisIndex].finished=true;
            fordisp_completecount[depth]++;
            fordisp_inprogcount[depth]--;
            doneCount++;
        }
        threadMtx.unlock();
    }
}
void sleepMillis(uint32_t ms){
    usleep(ms*1000);
}
SearchResult searchHL(GameState gs, PieceQueue *pq, uint64_t timelimit){
    Piece nextPiece=pq->getPiece(gs.getCurrentStepNum());
    /*
    printf("SHL PQ:\n");
    printf("CSN %d\n",gs.getCurrentStepNum());
    drawPieceQueue(pq,gs.getCurrentStepNum(),5,5);*/


    initializeThreadData(gs,pq);
    for(int i=0;i<NUM_THREADS;i++){
        workerThreads[i]=std::thread(threadFunc);
    }

    while(1){
        uint64_t t=timeSinceEpochMillisec();
        printf("\rSearching");

        for (int d=1;d<MAX_SEARCH_DEPTH;d++){
            int total=fordisp_workcount[d];
            int complete=fordisp_completecount[d];
            int inprog=fordisp_inprogcount[d];
            // Not all work is done
            if (complete != total){
                if (inprog != 0) {
                    //ansiColorSet(BLINK);
                    ansiColorSet(YELLOW_BRIGHT);
                }
                else {
                    ansiColorSet(WHITE_DIM);
                }
            }else ansiColorSet(BLUE);

            printf("%2d ", d);

            ansiColorSet(NONE);

        }
        /*
        int n=(t/100)%8;
        for (int i=0;i<8;i++){
            if (i<n) printf(".");
            else printf(" ");
        }
        printf("        ");*/

        if (t<timelimit){
            printf("%5d ms",(int)(timelimit-t));
            fflush(stdout);
        }else{
            threadKillRequest=true;
            printf("<-  Timeout\n");
            break;
        }
        if (doneCount==workCount){
            printf("<- Work done\n");
            break;
        }
        sleepMillis(30);
    }


    fflush(stdout);
    for(int i=0;i<NUM_THREADS;i++){
        workerThreads[i].join();
    }

    SearchResult res;
    res.isValid=false;
    res.searchDepth=0;

    for (int di=1;di<MAX_SEARCH_DEPTH;di++){
        Placement uniquePlacements[MAX_RANDSEARCH_ITER];
        int numUniquePlacements=0;
        int32_t bfSums[MAX_RANDSEARCH_ITER];
        int32_t sdx100Sums[MAX_RANDSEARCH_ITER];
        int uniquePlacementCount[MAX_RANDSEARCH_ITER];
        int maxCount=0;
        int maxIdx=-1;
        int invalids=0;
        int numEntries=0;
        int numFinished=0;
        int numStarted=0;

        for (int ri=0; ri<workCount;ri++){
            SearchRequest srq=threadData[ri];
            /*
            printf("RI %d SRQ %d\n",
                   ri,srq.depth);*/
            if (srq.depth != di) continue;

            numEntries++;

            if (srq.started) numStarted++;

            if (!srq.finished) continue;
            DFSResult dfsr=srq.result;
            numFinished++;
            if (dfsr.valid){
                // sanity
                Piece placementPiece=dfsr.bestPlacement.piece;
                if (!placementPiece.equal(nextPiece)){
                    printf("Piece mismatch!\n");
                    drawPiece(placementPiece);
                    printf("----\n");
                    drawPiece(nextPiece);
                }
                assert (placementPiece.equal(nextPiece));
                assert(!dfsr.computationInterrupted);
                assert(dfsr.boardFitness>-100000);
                assert(dfsr.scoreDelta>-100000);
                /*
                printf("    DFSR[%d] X %d Y %d BF %d SD %d CI%d\n",
                    i,
                    dfsr.bestPlacement.x,
                    dfsr.bestPlacement.y,
                    dfsr.boardFitness,
                    dfsr.scoreDelta,
                    dfsr.computationInterrupted);*/
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
                    uniquePlacementCount[numUniquePlacements]=0;
                    bfSums[numUniquePlacements]=0;
                    sdx100Sums[numUniquePlacements]=0;
                    duplicateOf=numUniquePlacements;
                    numUniquePlacements++;
                }
                uniquePlacementCount[duplicateOf]++;
                bfSums[duplicateOf]+=dfsr.boardFitness;
                sdx100Sums[duplicateOf]+=dfsr.scoreDelta*100;
                if (uniquePlacementCount[duplicateOf]>maxCount){
                    maxCount=uniquePlacementCount[duplicateOf];
                    maxIdx=duplicateOf;
                }
            }else{
                //printf("    DFSR[%d] invalid\n",i);
                invalids++;
            }
        }

        bool sufficientIterations=(numFinished>=numEntries) || (numFinished>=MIN_RANDSEARCH_ITER);

        SearchResult sr;
        sr.searchDepth=di;
        sr.isValid=false;

        if (sufficientIterations){
            printf("  Depth %2d | %2d/%2d",
                di,numFinished,numEntries);
            for(int i=0;i<numUniquePlacements;i++){
                if (i!=maxIdx) continue;
                Placement pl=uniquePlacements[i];
                int count=uniquePlacementCount[i];
                int percentage=count*100/numFinished;
                int32_t bfAvg=bfSums[i]/count;
                int32_t sdx100Avg=sdx100Sums[i]/count;
                printf(" | X%2d Y%2d BFavg %4d SDavg %6.2f",
                    pl.x,pl.y,
                    bfAvg,sdx100Avg/100.0);
                if (numEntries==1) {
                    ansiColorSet(GREEN_DIM);
                    printf(" (Determined)");
                    ansiColorSet(NONE);
                }else{
                    if (percentage>70) ansiColorSet(GREEN_BRIGHT);
                    else if (percentage>30) ansiColorSet(YELLOW_BRIGHT);
                    else ansiColorSet(MAGENTA_BRIGHT);
                    printf(" (%2d/%2d = %3d%%) ",
                        count,numFinished,
                        percentage
                        );
                    ansiColorSet(NONE);
                }
                /*
                if (uniquePlacementCount[i]==maxCount) printf(" *");
                if (i==maxIdx) printf(" <<");
                printf("\n");*/
            }
            if (invalids>0){
                ansiColorSet(RED_BRIGHT);
                printf(" | Invalid:%2d (%3d%%)",
                    invalids,
                    invalids*100/numFinished);
                ansiColorSet(NONE);
            }
            printf("\n");

            if (maxIdx==-1){
                // No results
                sr.isValid=false;

            }else {
                sr.isValid=true;
                sr.optimalPlacement=uniquePlacements[maxIdx];
            }

        }else if (numStarted>0){
            ansiColorSet(WHITE_DIM);
            printf("  Depth %2d | %2d/%2d (Insufficient)",
                di,numFinished,numEntries);
            ansiColorSet(NONE);
            printf("\n");
            sr.isValid=false;
        }else{
            //just don't do anything
        }

        if (sr.isValid) res=sr;

    }





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

#ifdef SERVER_GAME
    WoodokuClient wc("127.0.0.1","14311");
#endif

    PieceQueue pq;
    PieceGenerator *pgen=getGlobalPG();
    Board lastBoard;
    GameState gs;
    while (1){
        printf("\n\n\n");

        uint32_t turnIndex=gs.getCurrentStepNum();

#ifndef SERVER_GAME

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
        while(!pq.isVisible(gs.getCurrentStepNum()+15)) pq.addPiece(pgen->generate());
#endif
#ifdef SERVER_GAME
        ServerState ss;
        int waitN=0;
        while (!wc.recvServerStateUpdate(&ss)){
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

            printf("Press Enter to continue.\n");
            waitForEnter();
        }

        if (turnIndex != ss.turnIndex){
            ansiColorSet(RED);
            printf("Turn Index mismatch\n");
            ansiColorSet(NONE);
            printf("Local %d Server %d\n",
                   turnIndex,ss.turnIndex);

            return -1;

        }


        Piece servPieces[3];
        for (int x=0;x<5;x++){
            for (int y=0;y<5;y++){
                int idx=x+y*5;
                for (int pidx=0;pidx<3;pidx++){
                    if (ss.pieces[pidx][idx]) servPieces[pidx].addBlock(x,y);
                }
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



        ansiColorSet(BLUE_BRIGHT);
        printf("   ===== Turn %d =====   \n",turnIndex);
        ansiColorSet(NONE);

        SearchResult sr;
        sr=searchHL(gs,&pq,timeSinceEpochMillisec()+TARGET_MILLISEC_PER_TURN);
        printf("Taking result from depth %d\n",sr.searchDepth);
        drawPieceQueue(&pq,gs.getCurrentStepNum(),PREVIEW_PIECES,sr.searchDepth);

        PlacementResult pr;
        Placement placement;
        if (!sr.isValid){
            //No placement! Dead probs.
            ansiColorSet(RED);
            printf("No placement possible!\n");
            ansiColorSet(NONE);
#ifdef SERVER_GAME
            printf("Sending Retire...\n");
            wc.sendRetire(turnIndex);
#endif
            break;
        }


        placement=sr.optimalPlacement;
        pr=doPlacement(gs.getBoard(),placement);

        gs.applyPlacement(placement);
        printf("Step %d \n",gs.getCurrentStepNum());
        printf("Score: %d (+%d)\n",gs.getScore(),pr.scoreDelta);
        drawBoardFancy(lastBoard,pr.preClear,pr.finalResult);

        lastBoard=pr.finalResult;
#ifndef DISABLE_BOARD_FITNESS_HEURISTIC
        ansiColorSet(WHITE_DIM);
        printf("islandnessP %d\n",
               calculateIslandness(pr.finalResult));
        printf("islandnessN %d\n",
               calculateIslandness(pr.finalResult.bitwiseNOT()));
        printf("cell# %d\n",
               pr.finalResult.bitwiseNOT().countCells());
        printf("Fitness %d\n",
               calculateBoardFitness(pr.finalResult));
        ansiColorSet(NONE);
#endif
#ifdef SERVER_GAME
        printf("Sending Move...\n");
        ClientMove cm;
        for(int x=0;x<5;x++){
            for (int y=0;y<5;y++){
                cm.shape[x+y*5]=placement.piece.hasBlockAt(x,y);
            }
        }
        cm.x=placement.x;
        cm.y=placement.y;
        wc.sendMove(turnIndex,&cm);
#endif
        //printf("Enter to coninue...\n");
    }

    printf("Ending game.\n");

    return 0;
}
