#include <unistd.h>
#include "woodoku_client.h"

int main(){
    WoodokuClient wc=WoodokuClient("127.0.0.1","14311");

    int selected_piece=0;
    while (1){
        // Get server state
        ServerState ss;
        while (!wc.recvServerStateUpdate(&ss)){
            printf("Wait for server...\n");
            sleep(1);
        }

        printf("\n\nTurn %d\n",ss.turnIndex);
        for (int i=0;i<ss.numPieces;i++){
            printf("Piece %d\n",i);
            for (int y=0;y<5;y++){
                for (int x=0;x<5;x++){
                    if (ss.pieces[i][x+y*5]) printf("#");
                    else printf("_");
                }
                printf("\n");
            }
        }

        printf("Board:\n");
        for (int y=0;y<9;y++){
            for (int x=0;x<9;x++){
                if (ss.boardState[x+y*9]) printf("#");
                else printf("_");
            }
            printf("\n");
        }

        int x,y,p;
        printf("X/Y/Piece: \n");
        scanf("%d %d %d",&x,&y,&p);
        printf("X %d Y %d Piece %d\n",x,y,p);

        // Give up
        if (x<0){
            wc.sendRetire(ss.turnIndex);
            break;
        }

        ClientMove cm;
        for (int i=0;i<25;i++){
            cm.shape[i]=ss.pieces[p][i];
        }
        cm.x=x;
        cm.y=y;
        wc.sendMove(ss.turnIndex,&cm);
    }
}
