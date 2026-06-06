import sys, math, re

F = r"C:/Users/JGril0/Desktop/CyberGame/hardware/pcb/PCB/PCB.kicad_pcb"
txt = open(F, encoding="utf-8").read()

# --- minimal s-expression parser ---
def tokenize(s):
    toks = []
    i = 0; n = len(s)
    while i < n:
        c = s[i]
        if c in '()':
            toks.append(c); i += 1
        elif c == '"':
            j = i+1; buf = []
            while j < n:
                if s[j] == '\\':
                    buf.append(s[j+1]); j += 2; continue
                if s[j] == '"':
                    break
                buf.append(s[j]); j += 1
            toks.append(('STR', ''.join(buf))); i = j+1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < n and s[j] not in '()"' and not s[j].isspace():
                j += 1
            toks.append(('ATOM', s[i:j])); i = j
    return toks

def parse(toks):
    pos = 0
    def helper():
        nonlocal pos
        tok = toks[pos]
        if tok == '(':
            pos += 1
            lst = []
            while toks[pos] != ')':
                lst.append(helper())
            pos += 1
            return lst
        else:
            pos += 1
            return tok
    out = []
    while pos < len(toks):
        out.append(helper())
    return out

tree = parse(tokenize(txt))
root = tree[0]  # kicad_pcb

def name(node):
    if isinstance(node, list) and node and isinstance(node[0], tuple) and node[0][0]=='ATOM':
        return node[0][1]
    return None

def children(node, key):
    return [c for c in node if isinstance(c, list) and name(c)==key]

def first(node, key):
    cs = children(node, key)
    return cs[0] if cs else None

def atomval(x):
    if isinstance(x, tuple):
        return x[1]
    return None

def get_at(node):
    a = first(node, 'at')
    if not a: return None
    vals = [atomval(v) for v in a[1:]]
    x = float(vals[0]); y = float(vals[1])
    rot = float(vals[2]) if len(vals)>2 and vals[2] is not None else 0.0
    return (x,y,rot)

def get_str_after(node, key):
    # property "Reference" "REF**"
    pass

footprints = children(root, 'footprint')
rows = []
for fp in footprints:
    libname = atomval(fp[1]) if len(fp)>1 else "?"
    fat = get_at(fp)
    # reference
    ref = None; val = None
    for p in children(fp, 'property'):
        pname = atomval(p[1])
        pval = atomval(p[2]) if len(p)>2 else None
        if pname == 'Reference': ref = pval
        if pname == 'Value': val = pval
    fx, fy, frot = (fat if fat else (0,0,0))
    pads = []
    for pad in children(fp, 'pad'):
        pnum = atomval(pad[1])
        pat = get_at(pad)
        netnode = first(pad, 'net')
        net = atomval(netnode[1]) if netnode and len(netnode)>1 else ''
        # absolute pad position
        if pat:
            px, py, prot = pat
            # rotate pad offset by footprint rotation
            th = math.radians(frot)
            ax = fx + (px*math.cos(th) - py*math.sin(th))
            ay = fy + (px*math.sin(th) + py*math.cos(th))
        else:
            ax, ay = fx, fy
        pads.append((pnum, round(ax,3), round(ay,3), net))
    rows.append((ref, val, libname, round(fx,3), round(fy,3), frot, pads))

# sort by x then y
rows.sort(key=lambda r:(r[3], r[4]))
for ref,val,lib,fx,fy,frot,pads in rows:
    print(f"FP ref={ref} val={val} lib={lib} at=({fx},{fy}) rot={frot}")
    for pnum,ax,ay,net in pads:
        print(f"    pad {pnum:>4}  ({ax:>9},{ay:>9})  net='{net}'")
