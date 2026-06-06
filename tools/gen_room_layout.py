"""
gen_room_layout.py — gera o layout de salas para o firmware a partir dos
arquivos-texto humanos (modelo de DUAS fontes):

  assets/layout/ENTIDADES.txt          -> catalogo GLOBAL de tipos reutilizaveis
  assets/layout/POSICOES_<sala>.txt    -> instancias posicionadas por sala

Combina catalogo + posicoes e escreve components/entity/room_layout_gen.c
(tabela room_layouts[] que o loader de UI consome). Dados leves; nao pesam.

Regras aplicadas:
  - asset deve existir no asset_registry.json (senao: aviso + pula)
  - colisao: offset_x = -(w//2), offset_y = -h quando so vem largura/altura
  - sort_offset_y default 0; from_image default false (se true, ignora x,y)
  - nomes de instancia unicos por sala (aviso se duplicado)
  - POSICAO pode sobrescrever flags/colisao_base/sort_offset_y do catalogo
  - falta de dado obrigatorio -> aviso claro

Uso: python tools/gen_room_layout.py
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "assets" / "asset_registry.json"
LAYOUT_DIR = ROOT / "assets" / "layout"
OUT = ROOT / "components" / "entity" / "room_layout_gen.c"

ASSET_TYPE_INT = {"sprite": 0, "font": 1, "sound": 2}
ENTITY_TYPE = {
    "player": "ENTITY_TYPE_PLAYER", "npc": "ENTITY_TYPE_NPC",
    "furniture": "ENTITY_TYPE_FURNITURE", "prop": "ENTITY_TYPE_PROP",
    "trigger": "ENTITY_TYPE_TRIGGER",
}
FLAG_MACRO = {
    "solid": "ENTITY_FLAG_SOLID", "movable": "ENTITY_FLAG_MOVABLE",
    "carryable": "ENTITY_FLAG_CARRYABLE", "interactable": "ENTITY_FLAG_INTERACTABLE",
    "trigger": "ENTITY_FLAG_TRIGGER", "visible": "ENTITY_FLAG_VISIBLE",
    "ysorted": "ENTITY_FLAG_YSORTED",
}

warnings = 0


def warn(msg):
    global warnings
    warnings += 1
    print(f"  ! {msg}", file=sys.stderr)


def load_registry():
    reg = json.loads(REGISTRY.read_text(encoding="utf-8"))
    return {a["name"]: (ASSET_TYPE_INT.get(a["type"], 0), a["id"]) for a in reg["assets"]}


def clean_lines(path):
    """Linhas sem comentarios (#...) nem vazias."""
    out = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            out.append(line)
    return out


def kv(line):
    """'chave: valor' -> (chave_lower, valor). None se nao tiver ':'."""
    if ":" not in line:
        return None
    k, v = line.split(":", 1)
    return k.strip().lower(), v.strip()


def parse_colisao(val):
    """'largura=32, altura=24' -> (w, h)."""
    w = h = 0
    for part in val.replace(";", ",").split(","):
        p = kv(part.replace("=", ":"))
        if not p:
            continue
        k, v = p
        if k in ("largura", "w", "width"):
            w = int(v)
        elif k in ("altura", "h", "height"):
            h = int(v)
    return w, h


def parse_flags(val):
    res = []
    for f in val.replace(";", ",").split(","):
        f = f.strip().lower()
        if not f:
            continue
        m = FLAG_MACRO.get(f)
        if m:
            res.append(m)
        else:
            warn(f"flag desconhecida ignorada: '{f}'")
    return res


def parse_blocks(lines, header_kw):
    """Divide em blocos iniciados pela palavra-chave (ENTIDADE/POSICAO).
    Retorna (preambulo_kv, [bloco_kv, ...]). preambulo = kv antes do 1o bloco."""
    pre = {}
    blocks = []
    cur = None
    for line in lines:
        if line.upper() == header_kw:
            cur = {}
            blocks.append(cur)
            continue
        p = kv(line)
        if not p:
            continue
        k, v = p
        if cur is None:
            pre[k] = v
        else:
            cur[k] = v
    return pre, blocks


def load_catalog():
    path = LAYOUT_DIR / "ENTIDADES.txt"
    if not path.exists():
        warn(f"catalogo nao encontrado: {path}")
        return {}
    _, blocks = parse_blocks(clean_lines(path), "ENTIDADE")
    cat = {}
    for b in blocks:
        name = b.get("nome")
        if not name:
            warn("ENTIDADE sem 'nome' — pulada")
            continue
        w, h = parse_colisao(b.get("colisao_base", "")) if "colisao_base" in b else (0, 0)
        cat[name] = {
            "asset": b.get("imagem"),
            "type": b.get("tipo", "prop"),
            "flags": parse_flags(b.get("flags", "")),
            "coll_w": w, "coll_h": h,
            "sort_offset_y": int(b.get("sort_offset_y", "0")),
        }
    return cat


def merge_room(path, cat, registry):
    pre, blocks = parse_blocks(clean_lines(path), "POSICAO")
    room = pre.get("sala")
    bg = pre.get("fundo")
    if not room:
        warn(f"{path.name}: sem 'SALA:' no cabecalho — arquivo pulado")
        return None
    bg_id = registry.get(bg, (0, 0xFFFF))[1] if bg else 0xFFFF
    if bg and bg not in registry:
        warn(f"[{room}] FUNDO '{bg}' nao esta no registry")

    seen_names = set()
    defs = []
    for b in blocks:
        ref = b.get("entidade")
        name = b.get("nome")
        if not ref or not name:
            warn(f"[{room}] POSICAO sem 'entidade' ou 'nome' — pulada")
            continue
        base = cat.get(ref)
        if base is None:
            warn(f"[{room}] entidade '{ref}' nao esta no catalogo ENTIDADES — '{name}' pulada")
            continue
        asset = base["asset"]
        if asset not in registry:
            warn(f"[{room}] asset '{asset}' (de '{ref}') nao esta no registry — '{name}' pulada")
            continue
        if name in seen_names:
            warn(f"[{room}] nome de instancia repetido: '{name}' (deve ser unico)")
        seen_names.add(name)

        atype, aid = registry[asset]
        # overrides opcionais na POSICAO
        flags = parse_flags(b["flags"]) if "flags" in b else list(base["flags"])
        if "ENTITY_FLAG_VISIBLE" not in flags:
            flags.append("ENTITY_FLAG_VISIBLE")
        if "colisao_base" in b:
            w, h = parse_colisao(b["colisao_base"])
        else:
            w, h = base["coll_w"], base["coll_h"]
        sort_y = int(b["sort_offset_y"]) if "sort_offset_y" in b else base["sort_offset_y"]
        from_image = str(b.get("from_image", "false")).lower() in ("true", "1", "sim", "yes")
        x = int(b.get("x", "0"))
        y = int(b.get("y", "0"))
        if not from_image and "x" not in b and "y" not in b:
            warn(f"[{room}] '{name}' sem x/y e sem from_image — usando (0,0)")

        defs.append({
            "name": name, "type": ENTITY_TYPE.get(base["type"], "ENTITY_TYPE_PROP"),
            "atype": atype, "aid": aid,
            "flags": " | ".join(flags) if flags else "0",
            "x": x, "y": y,
            "w": w, "h": h, "ox": -(w // 2), "oy": -h,
            "sort": sort_y, "from_image": "true" if from_image else "false",
        })
    if not defs:
        warn(f"[{room}] nenhuma instancia valida — sala pulada")
        return None
    print(f"  {room}: {len(defs)} entidades (bg id {bg_id})")
    return {"room": room, "bg_id": bg_id, "defs": defs}


def main() -> int:
    if not LAYOUT_DIR.exists():
        warn(f"pasta {LAYOUT_DIR} nao existe")
        return 1
    registry = load_registry()
    cat = load_catalog()
    print(f"Catalogo: {len(cat)} tipos")

    rooms = []
    for pfile in sorted(LAYOUT_DIR.glob("POSICOES_*.txt")):
        r = merge_room(pfile, cat, registry)
        if r:
            rooms.append(r)

    out = [
        "/* Gerado por tools/gen_room_layout.py — NAO EDITE A MAO.",
        " * Fonte: assets/layout/ENTIDADES.txt + POSICOES_*.txt */",
        '#include "room_layout.h"',
        "#include <string.h>",
        "",
    ]
    for r in rooms:
        safe = "".join(c if c.isalnum() else "_" for c in r["room"])
        arr = f"room_{safe}_entities"
        out.append(f"static const room_entity_def_t {arr}[] = {{")
        for d in r["defs"]:
            out.append(
                f'    {{ .name="{d["name"]}", .asset_type={d["atype"]}, .asset_id={d["aid"]}, '
                f'.type={d["type"]}, .flags=({d["flags"]}), '
                f'.x={d["x"]}, .y={d["y"]}, .coll_w={d["w"]}, .coll_h={d["h"]}, '
                f'.coll_offset_x={d["ox"]}, .coll_offset_y={d["oy"]}, '
                f'.sort_offset_y={d["sort"]}, .from_image={d["from_image"]} }},'
            )
        out.append("};")
        out.append("")
        r["arr"] = arr

    out.append("const room_layout_t room_layouts[] = {")
    for r in rooms:
        out.append(
            f'    {{ .room="{r["room"]}", .bg_asset_id={r["bg_id"]}, .entities={r["arr"]}, '
            f'.count=sizeof({r["arr"]})/sizeof({r["arr"]}[0]) }},'
        )
    out.append("};")
    out.append("const size_t room_layouts_count = sizeof(room_layouts)/sizeof(room_layouts[0]);")
    out.append("")
    out.append("const room_layout_t *room_layout_find(const char *room) {")
    out.append("    for (size_t i = 0; i < room_layouts_count; ++i)")
    out.append("        if (strcmp(room_layouts[i].room, room) == 0) return &room_layouts[i];")
    out.append("    return NULL;")
    out.append("}")

    OUT.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"\n-> {OUT}  ({len(rooms)} salas)")
    if warnings:
        print(f"{warnings} aviso(s).", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
