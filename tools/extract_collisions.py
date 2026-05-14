#!/usr/bin/env python3
"""
Extrai colisoes e areas de gatilho dos PNGs em assets/sprites/.

Para cada sala (recepcao, empresa):
- PAREDES_E_OBJETOS_02.png: blobs de pixels nao-transparentes -> bbox por blob.
  Cada blob vira um retangulo OBSTACULO.
- AREA_*_NULL.png: 1 retangulo por arquivo. Tipo identificado pela cor predominante:
    - vermelho (#FF0000 +- tolerancia): PORTA ou TAREFA (decide pelo nome do arquivo)
    - marrom (#A04A40 +- tolerancia): NPC_INTERACAO

Uso:
  python tools/extract_collisions.py

Saida:
  components/assets/collision/collision_data.h
  components/assets/collision/collision_recepcao.c
  components/assets/collision/collision_empresa.c
  components/assets/CMakeLists.txt e' atualizado automaticamente para incluir os .c
"""
from pathlib import Path
from PIL import Image
from collections import deque
import re

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "sprites"
OUT_COL = ROOT / "components" / "assets" / "collision"
OUT_INC = ROOT / "components" / "assets" / "include"
CMK = ROOT / "components" / "assets" / "CMakeLists.txt"

ALPHA_THRESHOLD = 128

# Mapeamento de NOME DO ARQUIVO _NULL -> (tipo C, comentario)
NULL_NAME_TO_TYPE = [
    # ordem importa: regex match (mais especifico primeiro)
    (r"AREA_QUE_LIBERA_INTERACAO_COM_RECEPCIONISTA", "AREA_INTERACAO_NPC", "Recepcionista"),
    (r"AREA_PARA_ACESSAR_ESCRITORIO",                "AREA_PORTA_EMPRESA", "Saida pra Empresa"),
    (r"AREA_PORTA_RECEPCAO",                         "AREA_PORTA_RECEPCAO", "Saida pra Recepcao"),
    (r"AREA_DE_INTERACAO_PARA_TAREFA_VERDE",         "AREA_TAREFA_VERDE", "Tarefa verde"),
    (r"AREA_DE_INTERACAO_DIREITA_DO_NPC_TI",         "AREA_INTERACAO_NPC_TI_DIREITA", "NPC TI olhando direita"),
    (r"AREA_DE_INTERACAO_BAIXO_DO_NPC_TI",           "AREA_INTERACAO_NPC_TI_BAIXO", "NPC TI olhando baixo"),
    (r"^SPAWN",                                      "AREA_SPAWN", "Spawn do player"),
]


def classify_null(filename):
    base = filename.upper()
    for pat, type_name, comment in NULL_NAME_TO_TYPE:
        if re.search(pat, base):
            return type_name, comment
    return "AREA_GENERICA", "?"


def bbox_of_opaque(img):
    """Retorna (x, y, w, h) do bbox de pixels com alpha > threshold, ou None."""
    img = img.convert("RGBA")
    alpha = img.split()[3]
    # Pillow getbbox() ja respeita alpha=0
    bb = alpha.point(lambda v: 255 if v > ALPHA_THRESHOLD else 0).getbbox()
    if not bb:
        return None
    x0, y0, x1, y1 = bb
    return (x0, y0, x1 - x0, y1 - y0)


TILE = 16              # tamanho do tile em pixels
TILE_OPAQUE_RATIO = 0.25   # >= 25% dos pixels do tile opacos = tile colidivel


def extract_paredes_tilebased(png_path):
    """Le PAREDES_E_OBJETOS_02.png em tiles 16x16. Cada tile com >=25% de pixels
    nao-transparentes vira colidivel. Depois faz merge horizontal de tiles
    adjacentes na mesma linha (run-length) e merge vertical de retangulos
    com mesmo x/w em linhas consecutivas.
    """
    img = Image.open(png_path).convert("RGBA")
    w, h = img.size
    px = img.load()
    cols = w // TILE
    rows = h // TILE
    grid = [[False] * cols for _ in range(rows)]
    threshold_count = int(TILE * TILE * TILE_OPAQUE_RATIO)
    for ty in range(rows):
        for tx in range(cols):
            opaque = 0
            for py in range(ty * TILE, (ty + 1) * TILE):
                for ppx in range(tx * TILE, (tx + 1) * TILE):
                    if px[ppx, py][3] > ALPHA_THRESHOLD:
                        opaque += 1
                        if opaque >= threshold_count:
                            break
                if opaque >= threshold_count:
                    break
            grid[ty][tx] = opaque >= threshold_count

    # Run-length por linha (merge horizontal)
    row_rects = []  # (ty, x_tile, run_len)
    for ty in range(rows):
        x_start = None
        for tx in range(cols):
            if grid[ty][tx] and x_start is None:
                x_start = tx
            elif not grid[ty][tx] and x_start is not None:
                row_rects.append((ty, x_start, tx - x_start))
                x_start = None
        if x_start is not None:
            row_rects.append((ty, x_start, cols - x_start))

    # Merge vertical: retangulos com mesmo x/w em linhas consecutivas
    rects = []
    used = [False] * len(row_rects)
    for i, (ty, xs, run) in enumerate(row_rects):
        if used[i]:
            continue
        # tenta estender pra baixo
        y_end = ty
        for j in range(i + 1, len(row_rects)):
            ty2, xs2, run2 = row_rects[j]
            if used[j]:
                continue
            if ty2 == y_end + 1 and xs2 == xs and run2 == run:
                y_end = ty2
                used[j] = True
            elif ty2 > y_end + 1:
                break
        used[i] = True
        rects.append((xs * TILE, ty * TILE, run * TILE, (y_end - ty + 1) * TILE))
    return rects


def process_sala(sala):
    sala_dir = SRC / sala
    parts = [
        "/* Gerado por tools/extract_collisions.py — NAO EDITE A MAO */",
        "#include \"collision_data.h\"",
        "",
    ]

    # --- Obstaculos (PAREDES_E_OBJETOS_02) ---
    paredes_png = sala_dir / "PAREDES_E_OBJETOS_02.png"
    if paredes_png.exists():
        rects = extract_paredes_tilebased(paredes_png)
        parts.append(f"/* {len(rects)} obstaculos extraidos de PAREDES_E_OBJETOS_02 (tile-based 16px) */")
        parts.append(f"const collision_rect_t collision_{sala}_obstaculos[] = {{")
        for x, y, w, h in rects:
            parts.append(f"    {{ .x = {x:3d}, .y = {y:3d}, .w = {w:3d}, .h = {h:3d}, .kind = OBSTACULO }},")
        parts.append("};")
        parts.append(f"const size_t collision_{sala}_obstaculos_count = sizeof(collision_{sala}_obstaculos) / sizeof(collision_{sala}_obstaculos[0]);")
        parts.append("")
        print(f"  {sala}: {len(rects)} obstaculos")

    # --- Areas de gatilho (_NULL) ---
    parts.append("/* Areas de gatilho (extraidas dos arquivos _NULL) */")
    parts.append(f"const collision_rect_t collision_{sala}_gatilhos[] = {{")
    null_files = sorted(sala_dir.glob("*_NULL.png"))
    gatilhos = []  # (type_name, x, y, w, h, stem)
    for npng in null_files:
        bb = bbox_of_opaque(Image.open(npng))
        if not bb:
            print(f"  AVISO: {npng.name} sem pixels opacos — pulado")
            continue
        type_name, comment = classify_null(npng.stem)
        x, y, w, h = bb
        gatilhos.append((type_name, x, y, w, h, npng.stem))
        parts.append(f"    {{ .x = {x:3d}, .y = {y:3d}, .w = {w:3d}, .h = {h:3d}, .kind = {type_name} }}, /* {comment} ({npng.stem}) */")
        print(f"  {sala}: gatilho {type_name} @ ({x},{y}) {w}x{h}")
    parts.append("};")

    # Aviso de sobreposicao: 2 gatilhos no mesmo pixel sao ambiguos —
    # gatilho_at() so retorna o primeiro do array.
    for i in range(len(gatilhos)):
        for j in range(i + 1, len(gatilhos)):
            t1, x1, y1, w1, h1, s1 = gatilhos[i]
            t2, x2, y2, w2, h2, s2 = gatilhos[j]
            if x1 < x2 + w2 and x1 + w1 > x2 and y1 < y2 + h2 and y1 + h1 > y2:
                print(f"  *** AVISO: gatilhos SOBREPOSTOS em {sala}:")
                print(f"      {t1} ({s1}) e {t2} ({s2})")
                print(f"      -> repinte um deles em outro lugar (gatilho_at so ve o 1o)")
    parts.append(f"const size_t collision_{sala}_gatilhos_count = sizeof(collision_{sala}_gatilhos) / sizeof(collision_{sala}_gatilhos[0]);")

    out = OUT_COL / f"collision_{sala}.c"
    out.write_text("\n".join(parts), encoding="utf-8")
    print(f"  -> {out}")


def write_header():
    h = [
        "#pragma once",
        "/* Gerado por tools/extract_collisions.py — NAO EDITE A MAO */",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
        "typedef enum {",
        "    OBSTACULO = 0,",
        "    AREA_INTERACAO_NPC,",
        "    AREA_INTERACAO_NPC_TI_DIREITA,",
        "    AREA_INTERACAO_NPC_TI_BAIXO,",
        "    AREA_PORTA_EMPRESA,",
        "    AREA_PORTA_RECEPCAO,",
        "    AREA_TAREFA_VERDE,",
        "    AREA_SPAWN,",
        "    AREA_GENERICA,",
        "} collision_kind_t;",
        "",
        "typedef struct {",
        "    int16_t x;",
        "    int16_t y;",
        "    int16_t w;",
        "    int16_t h;",
        "    collision_kind_t kind;",
        "} collision_rect_t;",
        "",
        "/* Por sala */",
        "extern const collision_rect_t collision_recepcao_obstaculos[];",
        "extern const size_t           collision_recepcao_obstaculos_count;",
        "extern const collision_rect_t collision_recepcao_gatilhos[];",
        "extern const size_t           collision_recepcao_gatilhos_count;",
        "",
        "extern const collision_rect_t collision_empresa_obstaculos[];",
        "extern const size_t           collision_empresa_obstaculos_count;",
        "extern const collision_rect_t collision_empresa_gatilhos[];",
        "extern const size_t           collision_empresa_gatilhos_count;",
    ]
    (OUT_INC / "collision_data.h").write_text("\n".join(h), encoding="utf-8")
    print(f"\nHeader: {OUT_INC / 'collision_data.h'}")


def update_cmake():
    txt = CMK.read_text(encoding="utf-8")
    if "collision_recepcao.c" in txt:
        return
    new = txt.replace(
        '"generated/img_player.c"',
        '"generated/img_player.c"\n'
        '        "collision/collision_recepcao.c"\n'
        '        "collision/collision_empresa.c"'
    )
    CMK.write_text(new, encoding="utf-8")
    print(f"CMakeLists atualizado: {CMK}")


def main():
    OUT_COL.mkdir(parents=True, exist_ok=True)
    OUT_INC.mkdir(parents=True, exist_ok=True)
    for sala in ("recepcao", "empresa"):
        if not (SRC / sala).exists():
            print(f"Pulando {sala}: pasta nao existe")
            continue
        print(f"\n=== {sala} ===")
        process_sala(sala)
    write_header()
    update_cmake()
    print("\nOK")


if __name__ == "__main__":
    main()
