import math

F = r"C:/Users/JGril0/Desktop/CyberGame/hardware/pcb/PCB/PCB.kicad_pcb"
txt = open(F, encoding="utf-8").read()

def tokenize(s):
    toks=[];i=0;n=len(s)
    while i<n:
        c=s[i]
        if c in '()':toks.append(c);i+=1
        elif c=='"':
            j=i+1;buf=[]
            while j<n:
                if s[j]=='\\':buf.append(s[j+1]);j+=2;continue
                if s[j]=='"':break
                buf.append(s[j]);j+=1
            toks.append(('STR',''.join(buf)));i=j+1
        elif c.isspace():i+=1
        else:
            j=i
            while j<n and s[j] not in '()"' and not s[j].isspace():j+=1
            toks.append(('ATOM',s[i:j]));i=j
    return toks

def parse(toks):
    pos=0
    def h():
        nonlocal pos
        t=toks[pos]
        if t=='(':
            pos+=1;lst=[]
            while toks[pos]!=')':lst.append(h())
            pos+=1;return lst
        else:
            pos+=1;return t
    out=[]
    while pos<len(toks):out.append(h())
    return out

tree=parse(tokenize(txt));root=tree[0]
def nm(node):
    if isinstance(node,list) and node and isinstance(node[0],tuple) and node[0][0]=='ATOM':return node[0][1]
    return None
def ch(node,key):return [c for c in node if isinstance(c,list) and nm(c)==key]
def first(node,key):
    cs=ch(node,key);return cs[0] if cs else None
def av(x):return x[1] if isinstance(x,tuple) else None
def get_at(node):
    a=first(node,'at')
    if not a:return None
    v=[av(x) for x in a[1:]]
    return (float(v[0]),float(v[1]),float(v[2]) if len(v)>2 and v[2] is not None else 0.0)

# collect pads
pads=[]  # (label, x, y, net)
for fp in ch(root,'footprint'):
    fat=get_at(fp) or (0,0,0)
    fx,fy,frot=fat
    ref=None
    for p in ch(fp,'property'):
        if av(p[1])=='Reference':ref=av(p[2]) if len(p)>2 else None
    lib=av(fp[1]) if len(fp)>1 else '?'
    short=lib.split(':')[-1][:18]
    for pad in ch(fp,'pad'):
        pn=av(pad[1])
        pat=get_at(pad)
        net=''
        nn=first(pad,'net')
        if nn and len(nn)>1:net=av(nn[1])
        if pat:
            px,py,_=pat;th=math.radians(frot)
            ax=fx+(px*math.cos(th)-py*math.sin(th));ay=fy+(px*math.sin(th)+py*math.cos(th))
        else:ax,ay=fx,fy
        pads.append([f"{short}/{ref}.{pn}",round(ax,3),round(ay,3),net])

# collect segments (and arcs/vias) on copper
segs=[]
for s in ch(root,'segment'):
    st=first(s,'start');en=first(s,'end')
    net=first(s,'net')
    netname=''
    if net and len(net)>1:netname=av(net[1])
    sx,sy=float(av(st[1])),float(av(st[2]))
    ex,ey=float(av(en[1])),float(av(en[2]))
    ly=first(s,'layer'); lyn=av(ly[1]) if ly else '?'
    segs.append((sx,sy,ex,ey,netname,lyn))

vias=[]
for v in ch(root,'via'):
    at=first(v,'at')
    vx,vy=float(av(at[1])),float(av(at[2]))
    vias.append((vx,vy))

print(f"# pads={len(pads)}  # segments={len(segs)}  # vias={len(vias)}")
print("=== segment nets ===")
from collections import Counter
print(Counter(s[4] for s in segs))
print(f"vias: {vias}")

# Connectivity: union-find over 'points'. We treat pad centers and segment endpoints as nodes; merge if within tol.
TOL=0.15
nodes=[]  # list of (x,y, kind, label)
for lbl,x,y,net in pads:nodes.append((x,y,'pad',lbl,net))
for i,(sx,sy,ex,ey,net,ly) in enumerate(segs):
    nodes.append((sx,sy,'seg',f"S{i}a",net));nodes.append((ex,ey,'seg',f"S{i}b",net))
for i,(vx,vy) in enumerate(vias):nodes.append((vx,vy,'via',f"V{i}",''))

parent=list(range(len(nodes)))
def find(a):
    while parent[a]!=a:parent[a]=parent[parent[a]];a=parent[a]
    return a
def union(a,b):
    ra,rb=find(a),find(b)
    if ra!=rb:parent[ra]=rb

# merge endpoints of same segment
base=len(pads)
for i in range(len(segs)):
    union(base+2*i, base+2*i+1)

# merge any nodes within TOL (spatial)
for i in range(len(nodes)):
    for j in range(i+1,len(nodes)):
        if abs(nodes[i][0]-nodes[j][0])<=TOL and abs(nodes[i][1]-nodes[j][1])<=TOL:
            union(i,j)

# group pads by their root
groups={}
for idx,(x,y,kind,lbl,net) in enumerate(nodes):
    if kind!='pad':continue
    r=find(idx)
    groups.setdefault(r,[]).append((lbl,net,x,y))

print("\n=== ELECTRICAL GROUPS (pads connected by copper) ===")
gi=0
for r,members in groups.items():
    if len(members)<2:continue
    gi+=1
    nets=set(m[1] for m in members if m[1])
    print(f"\nGroup {gi}  nets={nets if nets else '(none)'}")
    for lbl,net,x,y in sorted(members):
        print(f"    {lbl:<30} net='{net}'  @({x},{y})")
print(f"\n(groups with >=2 pads: {gi})")
