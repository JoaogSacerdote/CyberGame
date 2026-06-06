from PIL import Image, ImageDraw
import os, json, shutil

BASE = r'C:/Users/JGril0/Desktop/CyberGame/CyberGameCore/CONSULTA/Aseprite Projeto'
SEC  = os.path.join(BASE, 'Secretaria')
ESC  = os.path.join(BASE, 'Escritorio')
TS   = os.path.join(SEC,  'Tilesets')

# ─── 1. COMPOSIÇÃO DA PREVIEW 480x320 ─────────────────────────────
CANVAS_W, CANVAS_H = 480, 320
WALL_COLOR  = (176, 190, 210, 255)
WALL_Y = 140

t1 = Image.open(os.path.join(TS,'Tileset1.png')).convert('RGBA')
t2 = Image.open(os.path.join(TS,'Tileset2.png')).convert('RGBA')
t3 = Image.open(os.path.join(TS,'Tileset3.png')).convert('RGBA')
t4 = Image.open(os.path.join(TS,'Tileset4.png')).convert('RGBA')
floor_tile = Image.new('RGBA',(32,32))
floor_tile.paste(t1,(0,0));  floor_tile.paste(t2,(16,0))
floor_tile.paste(t3,(0,16)); floor_tile.paste(t4,(16,16))

canvas = Image.new('RGBA',(CANVAS_W, CANVAS_H), WALL_COLOR)
for fy in range(WALL_Y, CANVAS_H, 32):
    for fx in range(0, CANVAS_W, 32):
        canvas.paste(floor_tile, (fx, fy), floor_tile)

draw = ImageDraw.Draw(canvas)
draw.line([(0, WALL_Y),(CANVAS_W, WALL_Y)], fill=(100,100,120,200), width=2)

# (id, arquivo, pasta, pivot_x, pivot_y)
entities = [
    ('PAREDE_REC_02',   'PAREDE_REC_02.png',      SEC, 457, 149),
    ('PAREDE_REC_01',   'PAREDE_REC_01.png',       SEC, 470, 149),
    ('BEBEDOURO_01',    'BEBEDOURO_01.png',         SEC, 420, 182),
    ('LIXEIRA_01',      'LIXEIRA_01.png',           ESC,  34, 215),
    ('BANCO_P_01_a',    'BANCO_P_01.png',           SEC, 351, 233),
    ('BANCO_P_01_b',    'BANCO_P_01.png',           SEC, 399, 233),
    ('MESA_CENTRO_01',  'MESA_CENTRO_01.png',       SEC, 415, 232),
    ('PLANTA_VASO_01',  'PLANTA_VASO_01.png',       SEC, 442, 309),
    ('SOFA_COSTA_03',   'SOFA_COSTA_03.png',        SEC, 375, 319),
    ('CADEIRA_ESQ_01',  'CADEIRA_ESQUERDA_01.png',  SEC, 127, 319),
    ('MESA_L_REC_01',   'MESA_L_RECEPCAO_01.png',   SEC,  71, 319),
]

placed, missing = [], []
for eid, fname, folder, px, py in sorted(entities, key=lambda e: e[4]):
    path = os.path.join(folder, fname)
    if not os.path.exists(path):
        missing.append(fname)
        continue
    img = Image.open(path).convert('RGBA')
    w, h = img.size
    dx = px - w//2
    dy = py - h
    canvas.paste(img, (dx, dy), img)
    placed.append((eid, dx, dy, w, h))

# placeholder NPC_01
draw.rectangle([55, 258, 117, 306], outline=(255,80,80,200), width=2)
draw.text((59, 268), 'NPC_01', fill=(255,80,80,255))
draw.text((59, 279), 'sem PNG', fill=(255,80,80,255))

out_preview = os.path.join(BASE, 'PREVIEW_RECEPCAO.png')
canvas.save(out_preview)
print('[OK] PREVIEW_RECEPCAO.png salvo')
for eid, dx, dy, w, h in placed:
    print(f'  {eid:25s} -> ({dx:4d},{dy:4d}) {w}x{h}')
if missing:
    print(f'[FALTANDO] {", ".join(missing)}')

# ─── 2. JSON ──────────────────────────────────────────────────────
raw = [
    # id, sala, sprite_w, sprite_h, col_w, col_h, arquivo_canonico
    ('CADEIRA_ESQUERDA_01',  'recepcao',   32,  64, 32, 20, 'CADEIRA_ESQUERDA_01.png'),
    ('CADEIRA_COSTA_01_REC', 'recepcao',   32,  64, 32, 20, 'CADEIRA_COSTA_01_REC.png'),
    ('CADEIRA_DIREITA_01',   'recepcao',   32,  64, 32, 20, 'CADEIRA_DIREITA_01.png'),
    ('CADEIRA_FRENTE_01',    'recepcao',   32,  64, 32, 20, 'CADEIRA_FRENTE_01.png'),
    ('CADEIRA_SHEET_01',     'recepcao',  128,  64, 32, 20, 'CADEIRA_SHEET_01.png'),
    ('MESA_L_RECEPCAO_01',   'recepcao',  112, 101,112, 77, 'MESA_L_RECEPCAO_01.png'),
    ('BANCO_P_01',           'recepcao',   32,  32, 32, 21, 'BANCO_P_01.png'),
    ('MESA_CENTRO_01',       'recepcao',   80,  56, 80, 40, 'MESA_CENTRO_01.png'),
    ('PLANTA_VASO_01',       'recepcao',   32,  56, 20, 12, 'PLANTA_VASO_01.png'),
    ('SOFA_COSTA_03',        'recepcao',   80,  48, 80, 32, 'SOFA_COSTA_03.png'),
    ('PAREDE_REC_01',        'recepcao',   17,  39, 17, 17, 'PAREDE_REC_01.png'),
    ('PAREDE_REC_02',        'recepcao',   13,  39, 13,  7, 'PAREDE_REC_02.png'),
    ('BEBEDOURO_01',         'recepcao',   26,  60, 26, 30, 'BEBEDOURO_01.png'),
    ('LIXEIRA_01',           'recepcao',   24,  30, 24, 18, 'LIXEIRA_01.png'),
    ('NPC_01',               'recepcao',   38,  46, 25,  8, 'NPC_01.png'),
    ('DIALOGO_01',           'ui',        320, 100,320, 30, 'DIALOGO_01.png'),
    ('CADEIRA_COSTA_01',     'escritorio', 32,  48, 32, 20, 'CADEIRA_COSTA_01.png'),
    ('CADEIRA_ESQUERDA_02',  'escritorio', 32,  48, 32, 16, 'CADEIRA_ESQUERDA_02.png'),
    ('CAFETEIRA_01',         'escritorio', 48,  60, 48, 40, 'CAFETEIRA_01.png'),
    ('IMPRESSORA_01',        'escritorio', 48,  53, 48, 32, 'IMPRESSORA_01.png'),
    ('MESA_L_ESCRITORIO_01', 'escritorio',208,  80,208, 60, 'MESA_L_ESCRITORIO_01.png'),
    ('MESA_L_ESCRITORIO_02', 'escritorio', 32,  73, 32, 40, 'MESA_L_ESCRITORIO_02.png'),
    ('SERVIDOR_01',          'escritorio', 32,  96, 32, 66, 'SERVIDOR_01.png'),
    ('PLANTA_VASO_02',       'escritorio', 22,  51, 22, 21, 'PLANTA_VASO_02.png'),
    ('PLANTA_VASO_03',       'escritorio', 32,  36, 32, 12, 'PLANTA_VASO_03.png'),
    ('PAREDE_REC_03',        'escritorio', 12,  21, 12,  9, 'PAREDE_REC_03.png'),
    ('PAREDE_REC_04',        'escritorio', 12,  32, 12, 12, 'PAREDE_REC_04.png'),
    ('PAREDE_REC_05',        'escritorio', 32,  32, 32, 20, 'PAREDE_REC_05.png'),
    ('PAREDE_REC_06',        'escritorio', 16,  32, 16, 20, 'PAREDE_REC_06.png'),
    ('PAREDE_REC_07',        'escritorio', 32,  54, 32, 14, 'PAREDE_REC_07.png'),
    ('NPC_02',               'escritorio', 38,  46, 25,  8, 'NPC_02.png'),
    ('NPC_03',               'escritorio', 38,  46, 25,  8, 'NPC_03.png'),
    ('PROTAGONISTA',         'player',     32,  48, 32, 16, 'PROTAGONISTA_01.png'),
]

doc = {
    "_doc": "Entidades do CyberGame. pivot=bottom-center. collision_offset_x=-collision_w/2, collision_offset_y=-collision_h. Gerado de ENTIDADES.txt em 2026-06-05. NOTA: MESA_L_RECEPCAO_01 colisao_h corrigido de S77 para 77.",
    "version": 1,
    "entities": []
}
for eid, sala, sw, sh, cw, ch, arq in raw:
    doc["entities"].append({
        "id": eid, "sala": sala, "file": arq,
        "sprite": {"w": sw, "h": sh},
        "collision": {"w": cw, "h": ch}
    })

json_path = os.path.join(BASE, 'ENTIDADES.json')
with open(json_path,'w',encoding='utf-8') as f:
    json.dump(doc, f, indent=2, ensure_ascii=False)
print(f'[OK] ENTIDADES.json salvo ({len(doc["entities"])} entidades)')

# ─── 3. REFERÊNCIA DE RENOMEAÇÃO ──────────────────────────────────
renames = [
    ('Secretaria','CadeiraCosta.png',     'CADEIRA_COSTA_01_REC.png', '32x64 cadeira alta recep. costas'),
    ('Secretaria','CadeiraDireita.png',   'CADEIRA_DIREITA_01.png',   '32x64 cadeira alta recep. direita'),
    ('Secretaria','CadeiraFrente.png',    'CADEIRA_FRENTE_01.png',    '32x64 cadeira alta recep. frente'),
    ('Secretaria','CadeiraLados.png',     'CADEIRA_SHEET_01.png',     '128x64 spritesheet 4 orientacoes'),
    ('Secretaria','Protagonista32x32.png','PROTAGONISTA_01.png',      '96x256 spritesheet player (nome enganoso)'),
    ('Escritorio','CadeiraCosta.png',     'CADEIRA_COSTA_01.png',     '32x48 cadeira escrit. costas (canonical)'),
    ('Escritorio','CadeiraEsquerda.png',  'CADEIRA_ESQUERDA_02.png',  '32x48 cadeira escrit. lado (canonical)'),
    ('Escritorio','Bebedouro.png',        'BEBEDOURO_01_DUP.png',     'DUPLICATA de Secretaria/BEBEDOURO_01.png'),
]

lines = [
    '# REFERENCIA DE RENOMEACAO - Aseprite Projeto\n',
    '# Gerado em 2026-06-05\n',
    '# Formato: PASTA | ANTES -> DEPOIS | MOTIVO\n',
    '# Arquivos ja em UPPER_SNAKE_CASE canonico nao aparecem aqui.\n',
    '# BEBEDOURO_01_DUP.png pode ser apagado (mesmo asset).\n',
    '#\n',
]
for pasta, antes, depois, motivo in renames:
    lines.append(f'{pasta:12s} | {antes:30s} -> {depois:30s} | {motivo}\n')

ref_path = os.path.join(BASE, 'REFERENCIA_RENOMEACAO.txt')
with open(ref_path,'w',encoding='utf-8') as f:
    f.writelines(lines)
print('[OK] REFERENCIA_RENOMEACAO.txt salvo')

# ─── 4. RENOMEAR ──────────────────────────────────────────────────
for pasta, antes, depois, motivo in renames:
    src = os.path.join(BASE, pasta, antes)
    dst = os.path.join(BASE, pasta, depois)
    if os.path.exists(src):
        os.rename(src, dst)
        print(f'  RENAME {pasta}/{antes} -> {depois}')
    else:
        print(f'  [SKIP nao encontrado] {pasta}/{antes}')

print('\nPRONTO.')
