from __future__ import annotations
import json
import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

CANVAS_W = 480
CANVAS_H = 320
BG_COLOR = (60, 70, 84, 255)
PIVOT_COLOR = (255, 60, 60, 255)
SPRITE_BOX_COLOR = (255, 220, 0, 220)
COLLISION_COLOR = (0, 255, 180, 220)
TEXT_BG = (0, 0, 0, 170)
TEXT_FG = (255, 255, 255, 255)

BASE = Path(__file__).resolve().parent
SEC = BASE / "Secretaria"
ENTIDADES_JSON = BASE / "ENTIDADES.json"
POSICOES_TXT = SEC / "posicao.txt"
OUT_PNG = BASE / "VALIDACAO_SECRETARIA_PREVIEW.png"
OUT_REPORT = BASE / "VALIDACAO_SECRETARIA_RELATORIO.txt"


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
    catalog = {}
    for item in data.get("entities", []):
        catalog[item["id"]] = item
    return catalog


def parse_positions() -> list[dict]:
    rows = []
    with POSICOES_TXT.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            if line.startswith("#"):
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


def find_exact_image(file_name: str) -> Path | None:
    p = SEC / file_name
    return p if p.exists() else None


def make_canvas() -> Image.Image:
    return Image.new("RGBA", (CANVAS_W, CANVAS_H), BG_COLOR)


def draw_one(canvas, draw, row, ent, report_lines):
    file_name = ent.get("file")
    sprite = ent.get("sprite", {})
    collision = ent.get("collision", {})
    x, y = row["x"], row["y"]
    sw, sh = sprite.get("w"), sprite.get("h")

    if sw is None or sh is None:
        report_lines.append(f"ERRO_SPRITE {row['id']} -> sprite ausente no catalogo")
        return

    img_path = find_exact_image(file_name)
    dx = x - sw // 2
    dy = y - sh

    if img_path is None:
        draw.rectangle([dx, dy, dx + sw, dy + sh], outline=(255, 100, 100, 255), width=2)
        draw_label(draw, (dx + 2, dy + 2), f"MISS {row['id']}")
        report_lines.append(f"MISS_IMAGEM {row['id']} -> arquivo esperado {file_name} nao encontrado em {SEC}")
    else:
        img = Image.open(img_path).convert("RGBA")
        canvas.alpha_composite(img, (dx, dy))
        report_lines.append(f"OK {row['id']} -> {file_name} pivot=({x},{y}) draw=({dx},{dy}) sprite=({sw},{sh}) real=({img.size[0]},{img.size[1]})")
        draw.rectangle([dx, dy, dx + sw, dy + sh], outline=SPRITE_BOX_COLOR, width=1)

    draw.line((x - 3, y, x + 3, y), fill=PIVOT_COLOR, width=1)
    draw.line((x, y - 3, x, y + 3), fill=PIVOT_COLOR, width=1)

    if collision.get("w") is not None and collision.get("h") is not None:
        cw, ch = collision["w"], collision["h"]
        cx1 = x - cw // 2
        cy1 = y - ch
        cx2 = cx1 + cw
        cy2 = cy1 + ch
        draw.rectangle([cx1, cy1, cx2, cy2], outline=COLLISION_COLOR, width=2)


def main():
    if not SEC.exists():
        raise SystemExit(f"Pasta Secretaria nao encontrada: {SEC}")
    if not ENTIDADES_JSON.exists():
        raise SystemExit(f"Arquivo nao encontrado: {ENTIDADES_JSON}")
    if not POSICOES_TXT.exists():
        raise SystemExit(f"Arquivo nao encontrado: {POSICOES_TXT}")

    catalog = load_catalog()
    positions = parse_positions()
    canvas = make_canvas()
    draw = ImageDraw.Draw(canvas)
    report_lines = []
    missing_catalog = []

    for row in sorted(positions, key=lambda r: r["y"]):
        ent = catalog.get(row["id"])
        if ent is None:
            missing_catalog.append(row["id"])
            report_lines.append(f"MISS_CATALOGO {row['id']} -> linha {row['raw']}")
            continue
        draw_one(canvas, draw, row, ent, report_lines)

    draw_label(draw, (6, 6), f"Secretaria | posicoes={len(positions)}")
    draw_label(draw, (6, 20), f"faltando no catalogo: {', '.join(missing_catalog) if missing_catalog else 'nenhum'}")

    canvas.save(OUT_PNG)
    with OUT_REPORT.open("w", encoding="utf-8") as f:
        f.write("VALIDACAO SECRETARIA\n")
        f.write(f"base={BASE}\n")
        f.write(f"secretaria={SEC}\n")
        f.write(f"posicoes={len(positions)}\n")
        f.write(f"faltando_catalogo={missing_catalog}\n\n")
        for line in report_lines:
            f.write(line + "\n")

    print(f"[OK] Preview salva em: {OUT_PNG}")
    print(f"[OK] Relatorio salvo em: {OUT_REPORT}")
    if missing_catalog:
        print(f"[ATENCAO] IDs ausentes no catalogo: {missing_catalog}")


if __name__ == "__main__":
    main()