from __future__ import annotations
import json
import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

CANVAS_W = 480
CANVAS_H = 320
BG_COLOR         = (40, 50, 64, 255)
PIVOT_COLOR      = (255, 60, 60, 255)
SPRITE_BOX_COLOR = (255, 220, 0, 220)
COLLISION_COLOR  = (0, 255, 180, 220)
TEXT_BG          = (0, 0, 0, 170)
TEXT_FG          = (255, 255, 255, 255)

BASE         = Path(__file__).resolve().parent
SEC          = BASE / "Secretaria"
ESC          = BASE / "Escritorio"
ENTIDADES_JSON = BASE / "ENTIDADES.json"
POSICOES_TXT   = ESC / "posicao.txt"
OUT_PNG        = BASE / "VALIDACAO_ESCRITORIO_PREVIEW.png"
OUT_REPORT     = BASE / "VALIDACAO_ESCRITORIO_RELATORIO.txt"


def parse_xy(text: str):
    nums = re.findall(r"-?\d+", text)
    if len(nums) >= 2:
        return int(nums[0]), int(nums[1])
    return None


def get_font():
    return ImageFont.load_default()


def draw_label(draw, xy, msg):
    f = get_font()
    bb = draw.textbbox(xy, msg, font=f)
    draw.rectangle([bb[0]-2, bb[1]-1, bb[2]+2, bb[3]+1], fill=TEXT_BG)
    draw.text(xy, msg, fill=TEXT_FG, font=f)


def load_catalog() -> dict:
    with ENTIDADES_JSON.open("r", encoding="utf-8") as f:
        data = json.load(f)
    return {item["id"]: item for item in data.get("entities", [])}


def parse_positions() -> list[dict]:
    rows = []
    with POSICOES_TXT.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "FORMATO" in line.upper() or "NIVEL" in line.upper():
                continue
            if "|" not in line:
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) < 2:
                continue
            xy = parse_xy(parts[-1])
            if xy is None:
                continue
            rows.append({"id": parts[0], "x": xy[0], "y": xy[1], "raw": line})
    return rows


def find_image(file_name: str) -> Path | None:
    for folder in (ESC, SEC):
        p = folder / file_name
        if p.exists():
            return p
    return None


def is_background(row: dict) -> bool:
    """Entidades que cobrem o canvas inteiro: renderizar como fundo primeiro."""
    return row["id"].startswith("CHAO_") or row["id"].startswith("PISO_")


def draw_one(canvas, draw, row, ent, report_lines):
    file_name = ent.get("file")
    sprite    = ent.get("sprite", {})
    collision = ent.get("collision", {})
    x, y = row["x"], row["y"]
    sw   = sprite.get("w")
    sh   = sprite.get("h")

    if sw is None or sh is None:
        report_lines.append(f"ERRO_SPRITE {row['id']} -> sprite ausente no catalogo")
        return

    img_path = find_image(file_name)
    dx = x - sw // 2
    dy = y - sh

    if img_path is None:
        draw.rectangle([dx, dy, dx + sw, dy + sh], outline=(255, 100, 100, 255), width=2)
        draw_label(draw, (dx + 2, max(dy + 2, 0)), f"MISS {row['id']}")
        report_lines.append(
            f"MISS_IMAGEM {row['id']} -> {file_name} nao encontrado em ESC nem SEC"
        )
    else:
        img = Image.open(img_path).convert("RGBA")
        canvas.alpha_composite(img, (max(dx, 0), max(dy, 0)))
        report_lines.append(
            f"OK {row['id']} -> {img_path.name} pivot=({x},{y}) "
            f"draw=({dx},{dy}) sprite=({sw},{sh}) real=({img.size[0]},{img.size[1]})"
        )
        draw.rectangle([dx, dy, dx + sw, dy + sh], outline=SPRITE_BOX_COLOR, width=1)

    draw.line((x - 3, y, x + 3, y), fill=PIVOT_COLOR, width=1)
    draw.line((x, y - 3, x, y + 3), fill=PIVOT_COLOR, width=1)

    cw = collision.get("w", 0)
    ch = collision.get("h", 0)
    if cw > 0 and ch > 0:
        cx1 = x - cw // 2
        cy1 = y - ch
        draw.rectangle([cx1, cy1, cx1 + cw, cy1 + ch], outline=COLLISION_COLOR, width=2)


def main():
    for req in (ENTIDADES_JSON, POSICOES_TXT):
        if not req.exists():
            raise SystemExit(f"Arquivo nao encontrado: {req}")

    catalog   = load_catalog()
    positions = parse_positions()

    # Separa fundo (CHAO_) das entidades normais
    bg_rows   = [r for r in positions if is_background(r)]
    fg_rows   = sorted(
        [r for r in positions if not is_background(r)],
        key=lambda r: r["y"]
    )

    canvas = Image.new("RGBA", (CANVAS_W, CANVAS_H), BG_COLOR)
    draw   = ImageDraw.Draw(canvas)
    report_lines: list[str] = []
    missing_catalog: list[str] = []

    # 1. Fundos primeiro
    for row in bg_rows:
        ent = catalog.get(row["id"])
        if ent is None:
            missing_catalog.append(row["id"])
            report_lines.append(f"MISS_CATALOGO {row['id']} -> {row['raw']}")
            continue
        draw_one(canvas, draw, row, ent, report_lines)

    # 2. Entidades ordenadas por y (Y-sort)
    for row in fg_rows:
        ent = catalog.get(row["id"])
        if ent is None:
            if row["id"] not in missing_catalog:
                missing_catalog.append(row["id"])
            report_lines.append(f"MISS_CATALOGO {row['id']} -> {row['raw']}")
            continue
        draw_one(canvas, draw, row, ent, report_lines)

    draw_label(draw, (6, 6),  f"Escritorio | posicoes={len(positions)}")
    draw_label(draw, (6, 20), f"miss_catalogo: {', '.join(missing_catalog) if missing_catalog else 'nenhum'}")

    canvas.save(OUT_PNG)

    with OUT_REPORT.open("w", encoding="utf-8") as f:
        f.write("VALIDACAO ESCRITORIO\n")
        f.write(f"base={BASE}\n")
        f.write(f"escritorio={ESC}\n")
        f.write(f"posicoes={len(positions)}\n")
        f.write(f"faltando_catalogo={missing_catalog}\n\n")
        for line in report_lines:
            f.write(line + "\n")

    print(f"[OK] Preview -> {OUT_PNG}")
    print(f"[OK] Relatorio -> {OUT_REPORT}")
    if missing_catalog:
        print(f"[ATENCAO] IDs ausentes no catalogo: {missing_catalog}")


if __name__ == "__main__":
    main()
