from PIL import Image
import os

ASSETS  = r'C:/Users/JGril0/Desktop/CyberGame/assets/sprites'
CONSULT = r'C:/Users/JGril0/Desktop/CyberGame/CyberGameCore/CONSULTA/Aseprite Projeto'
ESC_DIR = os.path.join(CONSULT, 'Escritorio')
OUT     = CONSULT

def load(path):
    return Image.open(path).convert('RGBA') if os.path.exists(path) else None

def npc_frame(path, col=1, row=2, fw=48, fh=48):
    img = load(path)
    if img is None:
        return None
    return img.crop((fw*col, fh*row, fw*(col+1), fh*(row+1)))

def place(canvas, img, pivot_x, pivot_y):
    if img is None:
        return
    w, h = img.size
    canvas.paste(img, (pivot_x - w//2, pivot_y - h), img)

def layer(canvas, path):
    img = load(path)
    if img:
        canvas.paste(img, (0, 0), img)

# ──────────────────────────────────────────────
# RECEPCAO
# ──────────────────────────────────────────────
REC = os.path.join(ASSETS, 'recepcao')

rec = load(os.path.join(REC, 'PISO_CAMADA_00.png'))
layer(rec, os.path.join(REC, 'PAREDES_E_OBJETOS_02.png'))
layer(rec, os.path.join(REC, 'RECEPCIONISTA_MECHENDO_NO_COMPUTADOR_SEM_INTERACAO_05.png'))
layer(rec, os.path.join(REC, 'COMPLEMENTO_SEMCOLISAO_PAREDES_E_OBJETOS_03.png'))
rec.save(os.path.join(OUT, 'PREVIEW_RECEPCAO.png'))
print('[OK] PREVIEW_RECEPCAO.png')

# ──────────────────────────────────────────────
# ESCRITORIO
# ──────────────────────────────────────────────
EMP = os.path.join(ASSETS, 'empresa')

esc = load(os.path.join(EMP, 'PISO_CAMADA_00.png'))
layer(esc, os.path.join(EMP, 'PAREDES_E_OBJETOS_02.png'))

# NPCs dinâmicos — frame idle-frente (col=1, row=2)
place(esc, npc_frame(os.path.join(ESC_DIR, 'NPC_02.png')), 190, 195)
place(esc, npc_frame(os.path.join(ESC_DIR, 'NPC_03.png')), 337, 127)

layer(esc, os.path.join(EMP, 'COMPLEMENTO_SEMCOLISAO_PAREDES_E_OBJETOS_03.png'))
esc.save(os.path.join(OUT, 'PREVIEW_ESCRITORIO.png'))
print('[OK] PREVIEW_ESCRITORIO.png')
