"""
Gera um mosaico de preview de todos os 66 assets do CyberGame.
Saída: assets/PREVIEW/  (um PNG por categoria + um completo)
Uso: python tools/gerar_previa_assets.py
"""

import json, os, sys, textwrap
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT        = Path(__file__).resolve().parent.parent
REGISTRY    = ROOT / "assets" / "asset_registry.json"
SPRITES_DIR = ROOT / "assets" / "sprites"
OUT_DIR     = ROOT / "assets" / "PREVIEW"

# ── Visual -------------------------------------------------------------------
THUMB  = 160          # células quadradas
PAD    = 12           # padding interno da célula
LABEL  = 36           # altura reservada para o texto abaixo do sprite
COLS   = 8            # colunas por linha
BG     = (18, 18, 28) # fundo geral
CELL   = (30, 30, 44) # fundo da célula
BORDER = (60, 60, 90)
WHITE  = (255, 255, 255)
GRAY   = (160, 160, 180)
YELLOW = (255, 220,  40)
CYAN   = ( 80, 220, 220)
GREEN  = (100, 220, 100)
ORANGE = (240, 160,  40)

# Cor do cabeçalho por categoria
CATEGORY_COLOR = {
    "Recepção":       (100, 180, 255),
    "Empresa":        (100, 255, 180),
    "Player":         (255, 200, 80),
    "Tarefa Verde":   (80, 230, 100),
    "Tarefa Amarela": (255, 200, 40),
    "Tarefa Vermelha":(255, 80,  80),
    "Telas Finais":   (200, 120, 255),
}

CATEGORIES = [
    ("Recepção",        [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,41,42,43,44,45]),
    ("Empresa",         [20,21,22,23,24,25,26,27,30,31,32,33,34,35,36,37,38,39,40,46,47,48,49,50,51,52]),
    ("Player",          [28]),
    ("Tarefa Verde",    [53,54]),
    ("Tarefa Amarela",  [55,56,57,58]),
    ("Tarefa Vermelha", [59,60,61,62,63]),
    ("Telas Finais",    [64,65]),
]

# ── Helpers ------------------------------------------------------------------

def load_font(size):
    try:
        return ImageFont.truetype("C:/Windows/Fonts/consola.ttf", size)
    except:
        try:
            return ImageFont.truetype("C:/Windows/Fonts/cour.ttf", size)
        except:
            return ImageFont.load_default()

FONT_ID   = load_font(11)
FONT_NAME = load_font(10)
FONT_FILE = load_font(9)
FONT_HDR  = load_font(14)

def short_file(f):
    """Abrevia o caminho para caber na célula."""
    p = Path(f)
    s = p.name
    if len(s) > 22:
        s = s[:10] + "…" + s[-10:]
    return s

def draw_cell(draw, ox, oy, asset, img_src):
    """Desenha uma célula de THUMB×(THUMB+LABEL) na posição (ox, oy)."""
    cw = THUMB
    ch = THUMB + LABEL

    # Fundo e borda
    draw.rectangle([ox, oy, ox + cw - 1, oy + ch - 1], fill=CELL, outline=BORDER)

    # Sprite (letter-box)
    if img_src is not None:
        max_w = cw - PAD * 2
        max_h = THUMB - PAD * 2
        img = img_src.copy()
        img.thumbnail((max_w, max_h), Image.LANCZOS)
        ix = ox + PAD + (max_w - img.width) // 2
        iy = oy + PAD + (max_h - img.height) // 2
        if img.mode == "RGBA":
            # Compositar sobre fundo da célula
            bg = Image.new("RGBA", img.size, CELL + (255,))
            bg.paste(img, (0, 0), img)
            img = bg.convert("RGB")
        draw._image.paste(img, (ix, iy))
    else:
        # Placeholder para assets sem PNG
        draw.rectangle([ox + PAD, oy + PAD, ox + cw - PAD, oy + THUMB - PAD],
                       fill=(50, 30, 30), outline=(120, 60, 60))
        draw.text((ox + cw // 2, oy + THUMB // 2), "N/A",
                  fill=(180, 80, 80), font=FONT_NAME, anchor="mm")

    # Labels
    id_text   = f"#{asset['id']}"
    name_text = asset["name"]
    file_text = short_file(asset.get("file", ""))

    lx = ox + 4
    ly = oy + THUMB + 2
    draw.text((lx, ly),      id_text,   fill=YELLOW, font=FONT_ID)
    draw.text((lx, ly + 12), name_text, fill=CYAN,   font=FONT_NAME)
    draw.text((lx, ly + 22), file_text, fill=GRAY,   font=FONT_FILE)


def make_category_image(cat_name, ids, registry_by_id):
    """Gera uma imagem para uma categoria."""
    n = len(ids)
    rows = (n + COLS - 1) // COLS
    cell_h = THUMB + LABEL
    hdr_h  = 28

    W = COLS * THUMB
    H = hdr_h + rows * cell_h + 4

    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)
    draw._image = img  # hack para poder fazer paste via draw

    # Cabeçalho
    color = CATEGORY_COLOR.get(cat_name, WHITE)
    draw.rectangle([0, 0, W - 1, hdr_h - 1], fill=(25, 25, 40))
    draw.text((8, 5), f"{cat_name}  ({n} assets)", fill=color, font=FONT_HDR)

    for i, aid in enumerate(ids):
        row = i // COLS
        col = i % COLS
        ox = col * THUMB
        oy = hdr_h + row * cell_h

        asset = registry_by_id.get(aid)
        if asset is None:
            continue

        img_path = SPRITES_DIR / asset.get("file", "")
        img_src = None
        if img_path.suffix.lower() == ".png" and img_path.exists():
            try:
                img_src = Image.open(img_path).convert("RGBA")
            except:
                pass

        draw.text  # dummy keep ref
        draw_cell(draw, ox, oy, asset, img_src)

    return img


def make_full_image(categories, registry_by_id):
    """Gera o mosaico completo com todas as categorias empilhadas."""
    imgs = []
    for cat_name, ids in categories:
        imgs.append(make_category_image(cat_name, ids, registry_by_id))

    total_h = sum(i.height for i in imgs) + (len(imgs) - 1) * 8
    W = imgs[0].width
    full = Image.new("RGB", (W, total_h), BG)
    y = 0
    for im in imgs:
        full.paste(im, (0, y))
        y += im.height + 8

    return full


# ── Main ---------------------------------------------------------------------

def main():
    with open(REGISTRY, encoding="utf-8") as f:
        data = json.load(f)

    by_id = {a["id"]: a for a in data["assets"]}

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    total = 0
    print(f"Gerando previews em {OUT_DIR} ...")

    # Um PNG por categoria
    for cat_name, ids in CATEGORIES:
        img = make_category_image(cat_name, ids, by_id)
        safe = cat_name.replace(" ", "_").replace("ã", "a").replace("ç", "c").replace("é", "e").replace("ê", "e")
        path = OUT_DIR / f"PREVIA_{safe.upper()}.png"
        img.save(path)
        print(f"  {path.name}  ({len(ids)} sprites)")
        total += len(ids)

    # Mosaico completo
    full = make_full_image(CATEGORIES, by_id)
    full_path = OUT_DIR / "PREVIA_COMPLETA_TODOS_OS_ASSETS.png"
    full.save(full_path)
    print(f"  {full_path.name}  ({total} sprites no total)")
    print(f"\nPronto! {len(CATEGORIES) + 1} arquivos gerados em assets/PREVIEW/")

if __name__ == "__main__":
    main()
