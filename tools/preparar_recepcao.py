"""
preparar_recepcao.py — prepara os sprites individuais da recepcao para o
pipeline do CyberGame.

Faz duas coisas:
  1. Copia os sprites de mobiliario/paredes da CONSULTA para assets/sprites/recepcao/
  2. Extrai o frame idle do NPC_01 (spritesheet 144x192, 3x4 frames de 48x48)

Uso:
    python tools/preparar_recepcao.py

Requer: pip install pillow
"""
from __future__ import annotations

import shutil
from pathlib import Path
from PIL import Image

ROOT      = Path(__file__).resolve().parent.parent
CONSULTA  = ROOT / "CyberGameCore" / "CONSULTA" / "Aseprite Projeto"
SEC       = CONSULTA / "Secretaria"
ESC       = CONSULTA / "Escritorio"
OUT       = ROOT / "assets" / "sprites" / "recepcao"

OUT.mkdir(parents=True, exist_ok=True)

# ── 1. Sprites de mobiliario: copiar da CONSULTA para assets/sprites/recepcao/ ──

# Da pasta Secretaria (recepcao)
SPRITES_SEC = [
    "BANCO_P_01.png",
    "BEBEDOURO_01.png",
    "CADEIRA_ESQUERDA_01.png",
    "CADEIRA_COSTA_01_REC.png",
    "CADEIRA_DIREITA_01.png",
    "CADEIRA_FRENTE_01.png",
    "MESA_L_RECEPCAO_01.png",
    "MESA_CENTRO_01.png",
    "SOFA_COSTA_03.png",
    "PLANTA_VASO_01.png",
    "PAREDE_REC_01.png",
    "PAREDE_REC_02.png",
]

# Da pasta Escritorio (LIXEIRA aparece na recepcao tambem)
SPRITES_ESC = [
    "LIXEIRA_01.png",
]

copied, skipped, missing = 0, 0, 0

for fname in SPRITES_SEC:
    src = SEC / fname
    dst = OUT / fname
    if not src.exists():
        print(f"  [FALTANDO]  {src}")
        missing += 1
        continue
    if dst.exists():
        print(f"  [JA EXISTE] {fname}")
        skipped += 1
        continue
    shutil.copy2(src, dst)
    print(f"  [COPIADO]   {fname}")
    copied += 1

for fname in SPRITES_ESC:
    src = ESC / fname
    dst = OUT / fname
    if not src.exists():
        print(f"  [FALTANDO]  {src}")
        missing += 1
        continue
    if dst.exists():
        print(f"  [JA EXISTE] {fname}")
        skipped += 1
        continue
    shutil.copy2(src, dst)
    print(f"  [COPIADO]   {fname} (de Escritorio/)")
    copied += 1

# ── 2. Extrair frame idle do NPC_01 ──────────────────────────────────────────
#
# NPC_01.png: spritesheet 144x192, 3 colunas x 4 linhas, cada frame 48x48.
# Frame idle "de frente" = coluna=1, linha=2 (base 0).
# Tambem extrai frame "dialog" = coluna=1, linha=0 (frente andando/olhando).

NPC_SHEET = SEC / "NPC_01.png"
FRAME_W, FRAME_H = 48, 48

npc_frames = {
    "NPC_01_IDLE.png":   (1, 3),   # idle frente (LPC: row3=down/front, row2=right)
    "NPC_01_DIALOG.png": (1, 3),   # mesmo frame frontal para o dialogo
}

if NPC_SHEET.exists():
    sheet = Image.open(NPC_SHEET).convert("RGBA")
    for out_name, (col, row) in npc_frames.items():
        x0 = col * FRAME_W
        y0 = row * FRAME_H
        frame = sheet.crop((x0, y0, x0 + FRAME_W, y0 + FRAME_H))
        dst = OUT / out_name
        frame.save(dst)
        print(f"  [EXTRAIDO]  {out_name} ({col=},{row=})")
else:
    print(f"  [FALTANDO]  {NPC_SHEET}")
    missing += 1

print(f"\nFeito: {copied} copiados, {skipped} ja existiam, {missing} faltando.")
if missing:
    print("ATENCAO: arquivos faltando — verifique a pasta CONSULTA.")
