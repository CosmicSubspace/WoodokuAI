 
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

print(groups)
pieces=[]
for g in groups:
    coords=[]
    y=0
    for line in g:
        x=0
        for c in line:
            if c=="#":
                coords.append((x,y))
            x+=1
        y+=1
    pieces.append(coords)

for p in pieces:
    print("tmp=Piece();")
    for c in p:
        print("tmp.addBlock({},{});".format(c[0],c[1]))
    print("pieces[pieceN++]=tmp;")
    print()
