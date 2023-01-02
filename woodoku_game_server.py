

# Packet: Server -> Client
# [0] 0x41
# [1..81] Board state (row-major bool array)
# [82] Number of pieces (uint8)
# [83..107] Piece 1 Shape (row-major bool array) or zeros
# [108..132] Piece 2
# [133..157] Piece 3
# [158..161] Turn Index (int32, BIG-ENDIAN)
# [162] 0x42
# 163 bytes


# Packet: Client -> Server
# [0] 0x22
# [1..25] Piece Shape
# [26] Piece X location (uint8)
# [27] Piece Y location (uint8)
# [28..31] Turn Index (int32, BIG-ENDIAN)
# [32] Retire? (bool)
# [33] 0x23
# 34 bytes


import time
import socket
import sys
import random
import threading
import tkinter
import argparse

from woodoku_common import *


ap=argparse.ArgumentParser()
ap.add_argument("--android-connect",
                help="Connect to an android phone via ADB.",
                action="store_true")
ap.add_argument("--manual-move",
                help="Do not send touch events to phone via ADB, do it manually instead.",
                action="store_true")
ap.add_argument("-p","--port",
                help="Server port",
                default=21991)
ap.add_argument("--no-window",
                help="Do not open a game window",
                action="store_true")
pargs=ap.parse_args()

if pargs.android_connect:
    import android_woodoku_driver

def generate_ssup(board,nexts,turnindex):
    ba=bytearray()

    assert len(ba)==0
    ba.append(0x41)

    assert len(ba)==1
    ba.extend(board.to_boolarray())

    assert len(ba)==82
    ba.append(len(nexts))

    assert len(ba)==83
    for i in range(3):
        if i<len(nexts):
            ba.extend(nexts[i].to_boolarray())
        else:
            ba.extend(bytes([0]*25))

    assert len(ba)==158
    ba.extend(turnindex.to_bytes(length=4,byteorder="big"))
    assert len(ba)==162

    ba.append(0x42)
    assert len(ba)==163

    return bytes(ba)

def parse_clientmove(b):
    assert len(b)==34
    if (b[0] != 0x22) or (b[33] != 0x23):
        print("Invalid magic!")
        0/0
    piece=Piece.from_boolarray(b[1:26])
    x=b[26]
    y=b[27]
    turnindex=int.from_bytes(b[28:32],byteorder="big")
    retire=b[32] != 0
    return piece,x,y,turnindex,retire


class GameThread(threading.Thread):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self._board=Board()
        self._prev_board=Board()
        self._preclear_board=Board()
        self._nexts=[]
    @property
    def board(self):
        return self._board
    @property
    def prev_board(self):
        return self._prev_board
    @property
    def preclear_board(self):
        return self._preclear_board
    @property
    def nexts(self):
        return self._nexts
    def run(self):
        print("Thread started")
        serv_ap=("",int(pargs.port))
        print(serv_ap)
        print("Waiting for client connect...")
        serversocket=socket.create_server(serv_ap)
        serversocket.listen(1)
        (clientsocket, address) = serversocket.accept()
        print("Socket opened.")

        buf=b''
        def socket_read(length):
            #Wrapper, handles broken-up data
            global buf
            res=b''
            buf+=clientsocket.recv(length)
            print("Received:",buf)
            if len(buf)>=length:
                res= buf[:length]
                buf=buf[length:]
            return res

        self._board=Board()
        self._nexts=[]
        print("Game start")
        lastTickTime=time.time()
        turn_index=0
        while True:
            print("\n\n === Turn",turn_index,"===")

            android_connect=pargs.android_connect
            automove=not pargs.manual_move
            if android_connect:
                if automove:
                    print("Sleep...")
                    time.sleep(1)
                else:
                    input("Enter to read state from phone...")
                print("Capturing phone...")
                phone_sc=android_woodoku_driver.screencap()
                phone_b=android_woodoku_driver.get_board_state(phone_sc)
                phone_p=android_woodoku_driver.get_pieces(phone_sc)
                if phone_b != self._board:
                    print("Mismatched board!")
                    print("Phone:")
                    print(phone_b)
                    print("Local:")
                    print(self._board)
                    input("Enter to override.")
                self._board=phone_b
                self._nexts=[i for i in phone_p if (i is not None)]
                self._nexts_raw=phone_p
                print("Got state from phone")


            if len(self._nexts)==0:
                if android_connect:
                    0/0
                while len(self._nexts)<3:
                    self._nexts.append(random.choice(pieces))

            print("Nexts:")
            nxs=[""]*5
            for n in self._nexts:
                lines=str(n).split("\n")
                for l in range(len(lines)):
                    nxs[l]+=lines[l]+" "
            print("\n".join(nxs))

            print("Board:")
            print(self._board)

            print("Sending data to client")
            dat=generate_ssup(self._board,self._nexts,turn_index)
            #print("Send:",dat)
            for i in range(len(dat)):
                pass#print("{} {:02x}".format(i,dat[i]))
            clientsocket.send(dat)



            clientdata=b''
            waitcount=0
            while True:
                try:
                    clientdata+=clientsocket.recv(34,socket.MSG_DONTWAIT)
                except BlockingIOError:
                    pass
                if len(clientdata)<34:
                    waitcount+=1
                    dots=waitcount%10
                    #print("wait...")
                    print("\rWaiting on client"+"."*dots+" "*(10-dots),
                          end='',flush=True)
                    time.sleep(0.1)
                    continue
                else:
                    break
            #print("Recv:",clientdata)
            print("\rReceived data from client")

            while time.time()<lastTickTime+1:
                time.sleep(0.1)
                print("Sleep cuz its too fast")

            clientpiece,px,py,tidx,rt=parse_clientmove(clientdata)
            if tidx != turn_index:
                print("TIDX mismatch!",turn_index,tidx)
            if rt:
                print("Retire!")
                break
            piece=None
            for p in pieces:
                if p==clientpiece:
                    piece=p
            if piece is None:
                print("Piece not in database???")
                print(clientpiece)
                0/0
            self._nexts.remove(piece)

            print(piece,"@ X",px,"Y",py)
            self._prev_board=self._board.clone()
            for lx,ly in piece.coords():
                x=lx+px
                y=ly+py
                if self._board.read(x,y):
                    print("Invalid placement!")
                    0/0
                #print("Write to board:",x,y)
                self._board.write(x,y,True)

            clear_x=[True]*9
            clear_y=[True]*9
            clear_c=[True]*9

            for x in range(9):
                for y in range(9):
                    c=(x//3)+(y//3)*3
                    if not self._board.read(x,y):
                        clear_x[x]=False
                        clear_y[y]=False
                        clear_c[c]=False
            self._preclear_board=self._board.clone()
            #print(clear_x)
            #print(clear_y)
            #print(clear_c)

            for x in range(9):
                for y in range(9):
                    c=(x//3)+(y//3)*3
                    if clear_x[x] or clear_y[y] or clear_c[c]:
                        self._board.write(x,y,False)


            if android_connect and automove:
                phone_piece_idx=None
                for i in range(len(self._nexts_raw)):
                    if self._nexts_raw[i] is not None:
                        if self._nexts_raw[i]==piece:
                            phone_piece_idx=i

                if phone_piece_idx is not None:
                    for tries in range(5):
                        print("Attempting move.",tries)
                        android_woodoku_driver.move_piece(
                            phone_piece_idx,
                            piece.size(),
                            (px,py)
                        )
                        phone_sc=android_woodoku_driver.screencap()
                        phone_b=android_woodoku_driver.get_board_state(phone_sc)
                        if self._board == phone_b:
                            print("Move successful")
                            break
                        elif self._prev_board == phone_b:
                            print("Move failed")
                            continue
                        else:
                            print("Board desyc? idk")
                            break
                else:
                    print("Piece not identified...?")
                    print("Not performing automove.")
            turn_index+=1


class Color:
    def __init__(self):
        self._r=0
        self._g=0
        self._b=0

    def __eq__(self,other):
        req= (self._r==other._r)
        geq= (self._g==other._g)
        beq= (self._b==other._b)
        return (req and geq and beq)
    @staticmethod
    def _float_to_8bit(f,clamp=True):
        if clamp:
            if f>1:
                f=1
            if f<0:
                f=0
        return round(f*255)
    @staticmethod
    def _8bit_to_float(n,clamp=True):
        f=n/255
        if clamp:
            if f>1:
                f=1
            if f<0:
                f=0
        return f

    @classmethod
    def from_RGB24(cls,ri,gi,bi):
        res=cls()
        res._r=self._8bit_to_float(ri)
        res._g=self._8bit_to_float(gi)
        res._b=self._8bit_to_float(bi)
        return res
    def to_RGB24(self):
        return (
            self._float_to_8bit(self._r),
            self._float_to_8bit(self._g),
            self._float_to_8bit(self._b))

    @classmethod
    def from_RGBf(cls,rf,gf,bf):
        res=cls()
        res._r=rf
        res._g=gf
        res._b=bf
        return res

    def to_hex6(self):
        return "#{:02x}{:02x}{:02x}".format(
            self._float_to_8bit(self._r),
            self._float_to_8bit(self._g),
            self._float_to_8bit(self._b)
            )

class CellGrid(tkinter.Frame):
    def __init__(self,parent,xsize,ysize,*args,**kwargs):
        super().__init__(parent,*args,**kwargs)
        self._cells=[]
        self._colorcache=[]
        self._xN=xsize
        self._yN=ysize
        self.configure(background="#101010")
        for y in range(ysize):
            for x in range(xsize):
                cell=tkinter.Frame(self,width=16,height=16)
                cell.grid(row=y,column=x,
                          padx=2,pady=2,
                          sticky="NEWS")
                clr=Color.from_RGBf(0,0,0)
                cell.configure(background=clr.to_hex6())
                self._cells.append(cell)
                self._colorcache.append(clr)

    def _coord2idx(self,x,y):
        if not (0<=x<self._xN):
            raise ValueError("Invalid X coord {} (range 0..{})".format(x,self._xN))
        if not (0<=y<self._yN):
            raise ValueError("Invalid Y coord {} (range 0..{})".format(y,self._yN))
        return x+y*self._xN

    def set_color(self,x,y,clr):
        idx=self._coord2idx(x,y)
        cached_color=self._colorcache[idx]
        if clr==cached_color:
            return
        self._cells[idx].configure(
            background=clr.to_hex6())
        self._colorcache[idx]=clr

gt=GameThread()
gt.start()

if not pargs.no_window:

    tkroot=tkinter.Tk()
    tkroot.configure(background="#000000")

    next_display=[]
    for i in range(3):
        cg=CellGrid(tkroot,5,5)
        cg.grid(column=i,row=1)
        next_display.append(cg)

    board_display=CellGrid(tkroot,9,9)
    board_display.grid(column=1,columnspan=3,row=2)

    def peroidic():
        board_current=gt.board
        board_prev=gt.prev_board
        board_preclear=gt.preclear_board
        for x in range(9):
            for y in range(9):
                clr=None

                current=board_current.read(x,y)
                prev=board_prev.read(x,y)
                preclear=board_preclear.read(x,y)
                state=(prev,preclear,current)
                if state==(True,True,True):
                    # Untouched on
                    clr=Color.from_RGBf(0.6,0.6,0.6)
                elif state==(True,True,False):
                    # Cleared
                    clr=Color.from_RGBf(0.8,0.5,0.5)
                elif state==(False,True,True):
                    # Newly placed
                    clr=Color.from_RGBf(1.0,1.0,1.0)
                elif state==(False,True,False):
                    # Placed then cleared
                    clr=Color.from_RGBf(1.0,0.9,0.9)
                elif state==(False,False,False):
                    # Untouched off
                    clr=Color.from_RGBf(0.2,0.2,0.2)
                else:
                    #Error? threading shenanigans
                    clr=Color.from_RGBf(1.0,0,0)
                board_display.set_color(x,y,clr)
        for i in range(3):
            for x in range(5):
                for y in range(5):
                    clr=Color.from_RGBf(0.1,0.1,0.1)
                    if i<len(gt.nexts):
                        if gt.nexts[i].read(x,y):
                            clr=Color.from_RGBf(0.5,0.5,1.0)
                    next_display[i].set_color(x,y,clr)

        tkroot.after(100,peroidic)
    peroidic()

    tkroot.mainloop()

gt.join()














