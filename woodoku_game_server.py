

# Packet: Server -> Client
# [0] 0x41
# [1..81] Board state (row-major bool array)
# [82..106] Piece 1 Shape (row-major bool array) or zeros
# [107..131] Piece 2
# [132..156] Piece 3
# [157] 0x42
# 158 bytes


# Packet: Client -> Server
# [0] 0x22
# [1..25] Piece Shape
# [26] Piece X location (uint8)
# [27] Piece Y location (uint8)
# [28] 0x23
# 29 bytes

import time
import socket
import sys
import random
import threading
import tkinter


class Piece:
    def __init__(self):
        self._coords=set()
    def __eq__(self,other):
        return self._coords==other._coords
    def add_block(self,x,y):
        self._coords.add((x,y))
    def coords(self):
        return tuple(self._coords)
    def read(self,x,y):
        return (x,y) in self._coords
    def write(self,x,y,v):
        if self.read(x,y)!=v:
            self.flip(x,y)
    def flip(self,x,y):
        if (x,y) in self._coords:
            self._coords.remove((x,y))
        else:
            self._coords.add((x,y))
    def to_boolarray(self):
        ba=bytearray()
        for y in range(5):
            for x in range(5):
                if self.read(x,y):
                    ba.append(1)
                else:
                    ba.append(0)
        return bytes(ba)
    @classmethod
    def from_boolarray(cls,b):
        res=cls()
        if len(b) != 25:
            raise ValueError("Invalid length")
        for i in range(25):
            x=i%5
            y=i//5
            if b[i] != 0:
                res.write(x,y,True)
        return res
    def __str__(self):
        s=''
        for y in range(5):
            for x in range(5):
                if self.read(x,y):
                    s+="[]"
                else:
                    s+="  "
            s+="\n"
        return s

with open("piecedefs.txt","r") as f:
    s=f.read()

groups=[]
current=[]
for line in s.split("\n"):
    if not line.strip():
        if current:
            groups.append(current)
        current=[]
    else:
        current.append(line)

pieces=[]
for g in groups:
    p=Piece()
    y=0
    for line in g:
        x=0
        for c in line:
            if c=="#":
                p.write(x,y,True)
            x+=1
        y+=1
    pieces.append(p)
'''
for p in pieces:
    print("{}\n{}".format(p.to_boolarray(),str(p)))
'''
class Board:
    def __init__(self):
        self._board=[False]*81
    @staticmethod
    def _coord2idx(x,y):
        return x+y*9
    def read(self,x,y):
        return self._board[self._coord2idx(x,y)]
    def write(self,x,y,v):
        self._board[self._coord2idx(x,y)]=v
    def __str__(self):
        s=''
        for y in range(9):
            for x in range(9):
                if self.read(x,y):
                    s+="[]"
                else:
                    s+="  "
            s+="\n"
        return s
    def to_boolarray(self):
        b=bytearray()
        for i in range(81):
            if self._board[i]:
                b.append(1)
            else:
                b.append(0)
        return bytes(b)

def generate_ssup(board,nexts):
    ba=bytearray()
    ba.append(0x41)
    ba.extend(board.to_boolarray())
    for i in range(3):
        if i<len(nexts):
            ba.extend(nexts[i].to_boolarray())
        else:
            ba.extend(bytes([0]*25))

    ba.append(0x42)
    assert len(ba)==158
    return bytes(ba)

def parse_clientmove(b):
    assert len(b)==29
    if (b[0] != 0x22) or (b[28] != 0x23):
        print("Invalid magic!")
        0/0
    piece=Piece.from_boolarray(b[1:26])
    x=b[26]
    y=b[27]
    return piece,x,y


class GameThread(threading.Thread):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self._board=Board()
        self._nexts=[]
    @property
    def board(self):
        return self._board
    @property
    def nexts(self):
        return self._nexts
    def run(self):
        print("Thread started")
        serversocket=socket.create_server(("127.0.0.1",14311))
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
        while True:
            print("\n\n")
            if len(self._nexts)==0:
                while len(self._nexts)<3:
                    self._nexts.append(random.choice(pieces))
            print("Send SSUP")
            clientsocket.send(generate_ssup(self._board,self._nexts))

            print("Nexts:")
            for n in self._nexts:
                print(str(n))
            print("Board:")
            print(self._board)

            print("Waiting input..")
            clientdata=b''
            while True:
                clientdata=clientsocket.recv(29)
                if not clientdata:
                    time.sleep(0.5)
                    print("No input...")
                    continue
                else:
                    break
            print("Recv:",clientdata)

            clientpiece,px,py=parse_clientmove(clientdata)
            piece=None
            for p in pieces:
                if p==clientpiece:
                    piece=p
            assert piece is not None
            self._nexts.remove(piece)

            print(piece,px,py)

            for lx,ly in piece.coords():
                x=lx+px
                y=ly+py
                if self._board.read(x,y):
                    print("Invalid placement!")
                    0/0
                print("Write to board:",x,y)
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

            print(clear_x)
            print(clear_y)
            print(clear_c)

            for x in range(9):
                for y in range(9):
                    c=(x//3)+(y//3)*3
                    if clear_x[x] or clear_y[y] or clear_c[c]:
                        self._board.write(x,y,False)

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
        res._bf=bf
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


tkroot=tkinter.Tk()
tkroot.configure(background="#000020")

next_display=[]
for i in range(3):
    cg=CellGrid(tkroot,5,5)
    cg.grid(column=i,row=1)
    next_display.append(cg)

board_display=CellGrid(tkroot,9,9)
board_display.grid(column=1,columnspan=3,row=2)

def peroidic():
    for x in range(9):
        for y in range(9):
            clr=Color.from_RGBf(0.1,0.1,0.1)
            if gt.board.read(x,y):
                clr=Color.from_RGBf(0.9,0.9,0.9)
            board_display.set_color(x,y,clr)
    for i in range(3):
        for x in range(5):
            for y in range(5):
                clr=Color.from_RGBf(0.1,0.1,0.1)
                if i<len(gt.nexts):
                    if gt.nexts[i].read(x,y):
                        clr=Color.from_RGBf(0.9,0.9,0.9)
                next_display[i].set_color(x,y,clr)

    tkroot.after(100,peroidic)
peroidic()

tkroot.mainloop()

gt.join()














