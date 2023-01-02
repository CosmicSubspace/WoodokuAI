# Screen coordinates.
# Tested on 1080x2400 6.4in display

# Board's (0,0) and (8,8) cell coordinates
board_00=(80,785)
board_88=(1000,1700)

# The program will sample a square area with a size double of this,
# centered on the cell center.
board_sampling_size=10

# Coordinates of the three pieces
piece_locations=[
    (200,2100),
    (500,2100),
    (850,2100)]

# Where the touch needs to be if placing a 3x3 piece
# on the top-left corner of the board
piece_placement_touchloc_3x3=(200,1190)

# Difference in touch location per block difference of the piece.
# As in, how different the touch location is between
# placing a (2,2) piece and (3,3) piece.
piece_placement_offset_perblock=(60,110)

# Area of the three pieces. Used for cropping.
piece_areas=[
    ((100,1900),(400,2300)),
    ((400,1900),(700,2300)),
    ((700,1900),(1000,2300))
    ]

# Cell size of the pieces
piece_cell_size=55


# If value greater than this, detect cell as filled.
brigheness_cutoff=120

import subprocess
import PIL.Image
import PIL.ImageStat
import woodoku_common
import math


def adb(*args):
    cmd("adb",*args)
def cmd(*args):
    print(">>",args)
    subprocess.run(args,check=True)

def screencap():
    path_phone="/sdcard/adb_screencap_tmp.png"
    path_comp="/tmp/adb_screencap_tmp.png"

    adb("shell","screencap",path_phone)
    adb("pull",path_phone,path_comp)

    img=PIL.Image.open(path_comp)

    adb("shell","rm",path_phone)
    cmd("rm",path_comp)

    return img

def swipe(origin,destination,duration):
    adb("shell","input","swipe",
            str(round(origin[0])),
            str(round(origin[1])),
            str(round(destination[0])),
            str(round(destination[1])),
            str(duration))

class Tuples:
    @classmethod
    def commonlength(cls,*ts):
        l=len(ts[0])
        for t in ts:
            if len(t) != l:
                raise ValueError("Mismatched length!")
        return l
    @classmethod
    def elementwise_operation(cls,f,*ts):
        res=[]
        #print("TS:",ts)
        for ei in range(cls.commonlength(*ts)):
            elements=[ts[ti][ei] for ti in range(len(ts))]
            #print("ELEM",ei,elements)
            #print(*elements)
            res.append(f(*elements))
        return tuple(res)
    @classmethod
    def add(cls,*ts):
        return cls.elementwise_operation(
            lambda *args:sum(args),*ts)
    @classmethod
    def neg(cls,t):
        return cls.elementwise_operation(
            lambda x:-x,t
            )
    @classmethod
    def sub(cls,a,b):
        return cls.add(a,cls.neg(b))
    @classmethod
    def mult(cls,n,t):
        return cls.elementwise_operation(
            lambda x:x*n,t
            )
    @classmethod
    def _multall(cls,*ns):
        res=1
        for n in ns:
            res*=n
        return res
    @classmethod
    def elementwise_mult(cls,*ts):
        return cls.elementwise_operation(
            cls._multall, *ts)
    @classmethod
    def round(cls,t):
        return cls.elementwise_operation(
            round,t)
    @classmethod
    def concat(cls,*ts):
        res=[]
        for t in ts:
            res.extend(t)
        return tuple(res)
    @classmethod
    def mag(cls,t):
        s=0
        for x in t:
            s+=(x*x)
        return math.sqrt(s)

def piece_placement_offset(xCells,yCells):
    cell_delta=Tuples.sub((xCells,yCells),(3,3))
    tl_3x3_delta=Tuples.elementwise_mult(
        piece_placement_offset_perblock,
        cell_delta)
    touchloc_board00=Tuples.add(
        piece_placement_touchloc_3x3,
        tl_3x3_delta)
    return Tuples.sub(
        touchloc_board00,
        board_00)

def cellLocation(x,y):
    board_size=Tuples.sub(board_88,board_00)
    cell_size=Tuples.mult(1/8,board_size)
    delta=Tuples.elementwise_mult(cell_size,(x,y))
    return Tuples.add(board_00,delta)

def get_board_state(img):
    img=img.convert("L") #grayscale
    board=woodoku_common.Board()
    for y in range(9):
        for x in range(9):
            center=cellLocation(x,y)
            bbox_offset=(board_sampling_size,board_sampling_size)
            tl=Tuples.round(Tuples.sub(center,bbox_offset))
            br=Tuples.round(Tuples.add(center,bbox_offset))
            sample=img.crop(Tuples.concat(tl,br))
            stat=PIL.ImageStat.Stat(sample)
            mean=stat.mean[0]
            filled=mean>brigheness_cutoff
            if filled:
                board.write(x,y,True)
            #print(x,y,center,mean)
    #print(board)
    return board


def get_pieces(img):
    pieces=[]
    resize_factor=5
    img=img.convert("L") #grayscale

    for p in range(3):

        areacoord=piece_areas[p]

        #print("Piece",p,areacoord)
        area=img.crop(Tuples.concat(*areacoord))
        area=area.resize(
            Tuples.round(Tuples.mult(1/resize_factor,area.size))
            ,resample=PIL.Image.BILINEAR)

        xmin=+1000000
        xmax=-1000000
        ymin=+1000000
        ymax=-1000000
        cellcount=0
        for y in range(area.size[1]):
            for x in range(area.size[0]):
                if area.getpixel((x,y)) >brigheness_cutoff:
                    cellcount+=1
                    if x>xmax:
                        xmax=x
                    if x<xmin:
                        xmin=x
                    if y>ymax:
                        ymax=y
                    if y<ymin:
                        ymin=y
        #print("Cellcount",cellcount)
        if cellcount<10:
            pieces.append(None)
            continue
        ysize=ymax-ymin
        xsize=xmax-xmin

        ycells=round((ysize*resize_factor)/piece_cell_size)
        xcells=round((xsize*resize_factor)/piece_cell_size)
        #print("XY",xmin,ymin,"SZ",xsize,ysize,"CC",xcells,ycells)
        def piece_cell_coord(xi,yi):
            xcs=xsize/xcells
            ycs=ysize/ycells
            x=xmin+xcs*(0.5+xi)
            y=ymin+ycs*(0.5+yi)
            return x,y

        pc=woodoku_common.Piece()
        for y in range(ycells):
            for x in range(xcells):
                ss=piece_cell_size*0.7/resize_factor/2
                center=piece_cell_coord(x,y)
                tl=Tuples.round(Tuples.sub(center,(ss,ss)))
                br=Tuples.round(Tuples.add(center,(ss,ss)))
                sample=area.crop(Tuples.concat(tl,br))
                stat=PIL.ImageStat.Stat(sample)
                mean=stat.mean[0]
                #print(tl,br,x,y,mean)
                if mean>brigheness_cutoff:
                    pc.write(x,y,True)

        pieces.append(pc)
    return pieces
    '''
    img=img.resize(
        Tuples.round(Tuples.mult(1/resize_factor,img.size))
        ,resample=PIL.Image.BILINEAR)


    rx_min=pieces_area[0][0]//resize_factor
    rx_max=pieces_area[1][0]//resize_factor
    ry_min=pieces_area[0][1]//resize_factor
    ry_max=pieces_area[1][1]//resize_factor
    straypixels=set()
    for ry in range(ry_min,ry_max):
        for rx in range(rx_min,rx_max):
            rcoord=(rx,ry)
            px= img.getpixel(rcoord)
            if px>120:
                straypixels.add(rcoord)


    pixelgroups=[]
    while len(straypixels)>0:
        thisgroup=set()
        rcoord=straypixels.pop()
        for i in straypixels:
            dist=Tuples.mag(Tuples.sub(rcoord,i))
            if dist*resize_factor<50:
            '''

def move_piece(piece_index,piece_bbox,cell_coord):
    if not (0<=piece_index<3):
        raise ValueError("OOB index "+str(piece_index))
    origin=piece_locations[piece_index]
    ppo=piece_placement_offset(*piece_bbox)
    cloc=cellLocation(*cell_coord)
    dest=Tuples.add(cloc,ppo,(0,-20)) #overshoot a little
    swipe(origin,dest,1000)


if __name__=="__main__":
    for y in range(5):
        for x in range(5):
            print(x,y,piece_placement_offset(x,y))
    0/0
    sc=screencap()
    b=get_board_state(sc)
    p=get_pieces(sc)
    print(b)
    for i in p:
        print(i)










