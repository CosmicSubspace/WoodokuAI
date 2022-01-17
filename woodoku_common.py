

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

for p in pieces:
    print(p)

class Board:
    def __init__(self):
        self._board=[False]*81
    def __eq__(self,other):
        for i in range(81):
            if self._board[i] != other._board[i]:
                return False
        return True
    def clone(self):
        res=Board()
        for i in range(81):
            res._board[i]=self._board[i]
        return res
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
