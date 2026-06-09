"""
Gera prévia visual de todos os assets do ATT_CAIO em mosaicos por categoria.
Saída: ATT_CAIO/PREVIA_*.png
"""
from __future__ import annotations
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

BASE = Path(__file__).resolve().parent
SALAS = BASE / "Salas"
TAREFAS = BASE / "Tarefas"

SCREEN_W, SCREEN_H = 480, 320
BG_COLOR = (20, 20, 30)
LABEL_H = 18
LABEL_BG = (0, 0, 0, 200)
LABEL_FG = (255, 255, 255)
BORDER = (80, 80, 100)

try:
    FONT = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 11)
except Exception:
    FONT = ImageFont.load_default()


def label(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, text: str) -> None:
    draw.rectangle([x, y, x + w - 1, y + LABEL_H - 1], fill=(0, 0, 0))
    draw.text((x + 2, y + 2), text[:40], fill=LABEL_FG, font=FONT)


def border_rect(draw: ImageDraw.ImageDraw, x: int, y: int, w: int, h: int) -> None:
    draw.rectangle([x, y, x + w - 1, y + h - 1], outline=BORDER, width=1)


def load_fit(path: Path, max_w: int = SCREEN_W, max_h: int = SCREEN_H) -> Image.Image:
    img = Image.open(path).convert("RGBA")
    img.thumbnail((max_w, max_h), Image.LANCZOS)
    return img


def mosaic(items: list[tuple[str, Path]], cols: int, cell_w: int, cell_h: int, title: str) -> Image.Image:
    """items = [(label, path)]. Draws a grid."""
    rows = (len(items) + cols - 1) // cols
    pad = 6
    canvas_w = cols * (cell_w + pad) + pad
    canvas_h = rows * (cell_h + LABEL_H + pad) + pad + 24
    canvas = Image.new("RGBA", (canvas_w, canvas_h), BG_COLOR + (255,))
    draw = ImageDraw.Draw(canvas)
    draw.text((pad, 4), title, fill=(200, 200, 255), font=FONT)

    for i, (lbl, path) in enumerate(items):
        col = i % cols
        row = i // cols
        x = pad + col * (cell_w + pad)
        y = 24 + pad + row * (cell_h + LABEL_H + pad)
        # background cell
        draw.rectangle([x, y, x + cell_w - 1, y + cell_h + LABEL_H - 1], fill=(35, 35, 45))
        border_rect(draw, x, y, cell_w, cell_h + LABEL_H)
        if path.exists():
            try:
                img = load_fit(path, cell_w, cell_h)
                iw, ih = img.size
                ox = x + (cell_w - iw) // 2
                oy = y + (cell_h - ih) // 2
                canvas.paste(img, (ox, oy), img)
            except Exception as e:
                draw.text((x + 2, y + cell_h // 2), f"ERR: {e}", fill=(255, 80, 80), font=FONT)
        else:
            draw.text((x + 2, y + 4), "NOT FOUND", fill=(255, 80, 80), font=FONT)
        label(draw, x, y + cell_h, cell_w, lbl)

    return canvas


# ─── 1. Salas ─────────────────────────────────────────────────────────────────

def preview_salas() -> None:
    items: list[tuple[str, Path]] = []

    for sala in ("Secretaria", "Escritorio"):
        folder = SALAS / sala
        for png in sorted(folder.glob("*.png")):
            items.append((f"{sala}/{png.name}", png))

    canvas = mosaic(items, cols=6, cell_w=120, cell_h=100, title="SALAS — todos os sprites")
    out = BASE / "PREVIA_SALAS.png"
    canvas.save(out)
    print(f"  -> {out.name}")


# ─── 2. Tela de cada tarefa com elementos sobrepostos ─────────────────────────

def overlay_elements(base_img: Image.Image, posicao_txt: Path, folder: Path,
                     interacoes_txt: Path | None = None) -> Image.Image:
    """Sobrepoe sprites definidos em POSICAO.txt sobre base_img (480x320)."""
    canvas = Image.new("RGBA", (SCREEN_W, SCREEN_H), (20, 20, 30, 255))
    # coloca fundo
    bw, bh = base_img.size
    canvas.paste(base_img, ((SCREEN_W - bw) // 2, (SCREEN_H - bh) // 2))

    draw = ImageDraw.Draw(canvas)

    if posicao_txt.exists():
        for raw in posicao_txt.read_text(encoding="utf-8").splitlines():
            line = raw.split("#", 1)[0].strip()
            if not line or "|" not in line:
                continue
            parts = [p.strip() for p in line.split("|")]
            eid = parts[0].upper()
            if "FORMATO" in eid or ":" in eid:
                continue
            import re
            m = re.search(r"(\d+)\s*,\s*(\d+)", parts[-1])
            if not m:
                continue
            px, py = int(m.group(1)), int(m.group(2))

            # tenta achar PNG na pasta
            candidates = list(folder.glob(f"{eid}.png")) + list(folder.glob(f"{eid.lower()}.png"))
            if candidates:
                try:
                    spr = Image.open(candidates[0]).convert("RGBA")
                    sw, sh = spr.size
                    draw_x = px - sw // 2
                    draw_y = py - sh
                    canvas.paste(spr, (draw_x, draw_y), spr)
                    # pivot
                    draw.line([(px-3, py), (px+3, py)], fill=(255,220,0,180), width=1)
                    draw.line([(px, py-3), (px, py+3)], fill=(255,220,0,180), width=1)
                except Exception:
                    pass

    # desenha zonas de interacao
    if interacoes_txt and interacoes_txt.exists():
        import re
        for raw in interacoes_txt.read_text(encoding="utf-8").splitlines():
            line = raw.split("#", 1)[0].strip()
            if not line or "|" not in line:
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) < 3:
                continue
            eid = parts[0].upper()
            if "FORMATO" in eid:
                continue
            mwh = re.search(r"(\d+)\s*,\s*(\d+)", parts[1])
            mxy = re.search(r"(\d+)\s*,\s*(\d+)", parts[2])
            if not mwh or not mxy:
                continue
            iw, ih = int(mwh.group(1)), int(mwh.group(2))
            ix, iy = int(mxy.group(1)), int(mxy.group(2))
            # pivot -> rect
            rx = ix - iw // 2
            ry = iy - ih
            draw.rectangle([rx, ry, rx+iw-1, ry+ih-1],
                            outline=(0, 255, 120, 200), width=1)
            draw.text((rx+2, ry+2), eid[:20], fill=(0, 255, 120), font=FONT)

    return canvas


def preview_tarefa_verde() -> None:
    folder = TAREFAS / "VERDE" / "SENHA"
    base = Image.open(folder / "CREDENCIAL_GERAL.png").convert("RGBA")
    canvas = overlay_elements(base, folder / "POSICAO.txt", folder,
                               folder / "INTERACOES.txt")
    # adiciona opcoes
    opcoes_path = folder / "CREDENCIAL_OPCOES.png"
    if opcoes_path.exists():
        opc = Image.open(opcoes_path).convert("RGBA")
        # posicao.txt diz pivot 244,115 -> x=244-308//2=90, y=115-115=0 ... improvavel
        # usa posicao literal do txt
        canvas.paste(opc, (90, 0), opc)

    out = BASE / "PREVIA_TAREFA_VERDE.png"
    canvas.save(out)
    print(f"  -> {out.name}")


def preview_tarefa_amarela() -> None:
    folder = TAREFAS / "AMARELO" / "TROCA_HD"
    base = Image.open(folder / "FUNDO_TELA.png").convert("RGBA")
    canvas = overlay_elements(base, folder / "POSICAO.txt", folder,
                               folder / "INTERAÇÃO.txt")
    out = BASE / "PREVIA_TAREFA_AMARELA.png"
    canvas.save(out)
    print(f"  -> {out.name}")


def preview_tarefa_vermelha() -> None:
    folder = TAREFAS / "VERMELHO" / "DDOS"
    base = Image.open(folder / "DDOS_FUNDO.png").convert("RGBA")
    canvas = overlay_elements(base, folder / "POSICAO.txt", folder,
                               folder / "INTERACAO.txt")
    # mostra fogo na primeira posicao
    fogo_path = folder / "FOGO_DDOS.png"
    if fogo_path.exists():
        fogo = Image.open(fogo_path).convert("RGBA")
        canvas.paste(fogo, (114-16, 124-48), fogo)

    out = BASE / "PREVIA_TAREFA_VERMELHA.png"
    canvas.save(out)
    print(f"  -> {out.name}")


def preview_finais() -> None:
    vitoria = Image.open(TAREFAS / "FINAIS" / "VITORIA" / "TELA_VITORIA.png").convert("RGBA")
    derrota = Image.open(TAREFAS / "FINAIS" / "DERROTA" / "TELA_DERROTA.png").convert("RGBA")

    canvas = Image.new("RGBA", (SCREEN_W * 2 + 20, SCREEN_H + 30), BG_COLOR + (255,))
    draw = ImageDraw.Draw(canvas)
    draw.text((4, 4), "TELAS FINAIS", fill=(200, 200, 255), font=FONT)
    canvas.paste(vitoria, (0, 24), vitoria)
    canvas.paste(derrota, (SCREEN_W + 20, 24), derrota)
    draw.text((4, 24 + SCREEN_H + 2), "VITORIA", fill=(100, 255, 100), font=FONT)
    draw.text((SCREEN_W + 24, 24 + SCREEN_H + 2), "DERROTA", fill=(255, 80, 80), font=FONT)

    out = BASE / "PREVIA_TELAS_FINAIS.png"
    canvas.save(out)
    print(f"  -> {out.name}")


def preview_fogo() -> None:
    """Mostra as variações de animação de fogo."""
    fire_base = TAREFAS / "VERMELHO" / "fire asset red"
    groups = sorted(fire_base.iterdir())
    items = []
    for g in groups[:8]:  # primeiros 8 grupos
        frames = sorted(g.glob("*_1.png"))  # primeiro frame de cada variacao
        for f in frames[:5]:
            items.append((f"{g.name}/{f.name}", f))
    if items:
        canvas = mosaic(items, cols=8, cell_w=64, cell_h=80, title="FOGO — variacoes (frame 1)")
        out = BASE / "PREVIA_FOGO_VARIACOES.png"
        canvas.save(out)
        print(f"  -> {out.name}")


# ─── Main ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("Gerando previas ATT_CAIO...\n")
    preview_salas()
    preview_tarefa_verde()
    preview_tarefa_amarela()
    preview_tarefa_vermelha()
    preview_finais()
    preview_fogo()
    print("\nConcluido.")
