"""
gerar_layout.py — gera assets/layout/recepcao.json e assets/layout/empresa.json
a partir das fontes autoritativas:

  CyberGameCore/CONSULTA/Aseprite Projeto/Secretaria/posicao.txt + INTERACOES.txt
  CyberGameCore/CONSULTA/Aseprite Projeto/Escritorio/posicao.txt + INTERACOES.txt
  CyberGameCore/CONSULTA/Aseprite Projeto/ENTIDADES.json  (dimensoes sprite + colisao)
  assets/asset_registry.json                              (IDs de asset para o firmware)

Regras aplicadas:
  - render_x = pivot_x - sprite_w / 2   (formula pivot bottom-center)
  - render_y = pivot_y - sprite_h
  - coll_offset_x = -floor(coll_w / 2)
  - coll_offset_y = -coll_h
  - Linhas comentadas (#) no posicao.txt sao ignoradas
  - Entidades com sala="ui" sao ignoradas (sao criadas pelo screen_*.c)
  - NPCs: resolve asset para o frame _IDLE no registry

Uso: python tools/gerar_layout.py

Saida:
  assets/layout/recepcao.json         <- layout carregado pelo firmware do SD
  assets/layout/empresa.json
  CONSULTA/Aseprite Projeto/PREVIEW_RECEPCAO.png   (requer pip install pillow)
  CONSULTA/Aseprite Projeto/PREVIEW_EMPRESA.png
"""
from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path

ROOT     = Path(__file__).resolve().parent.parent
CONSULTA = ROOT / "CyberGameCore" / "CONSULTA" / "Aseprite Projeto"
SEC      = CONSULTA / "Secretaria"
ESC      = CONSULTA / "Escritorio"
REGISTRY_PATH = ROOT / "assets" / "asset_registry.json"
CATALOG_PATH  = CONSULTA / "ENTIDADES.json"
OUT_DIR  = ROOT / "assets" / "layout"
SPRITES  = ROOT / "assets" / "sprites"

PLAYER_ASSET_ID = 28   # Sprite_PLAYER.png no registry
PLAYER_FRAME_W  = 32
PLAYER_FRAME_H  = 48
PLAYER_COLL_W   = 16   # hitbox dos pes (calibrado em jogo)
PLAYER_COLL_H   = 12


# ─── Loaders ──────────────────────────────────────────────────────────────────

def load_registry() -> dict[str, dict]:
    """Retorna {basename_lower: {"id": int, "atype": int}}."""
    data = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    atype_int = {"sprite": 0, "font": 1, "sound": 2}
    reg = {}
    for a in data["assets"]:
        key = Path(a["file"]).name.lower()
        reg[key] = {
            "id":    a["id"],
            "atype": atype_int.get(a.get("type", "sprite"), 0),
        }
    return reg


def load_catalog() -> dict[str, dict]:
    """Retorna {ENTITY_ID_UPPER: {file, sala, sprite_w, sprite_h, coll_w, coll_h}}."""
    data = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    cat = {}
    for e in data["entities"]:
        cat[e["id"].upper()] = {
            "file":     e["file"],
            "sala":     e.get("sala", ""),
            "sprite_w": e["sprite"]["w"],
            "sprite_h": e["sprite"]["h"],
            "coll_w":   e["collision"]["w"],
            "coll_h":   e["collision"]["h"],
        }
    return cat


# ─── Parsers ──────────────────────────────────────────────────────────────────

def _xy(s: str) -> tuple[int, int] | None:
    """Extrai (x, y) do primeiro 'numero,numero' encontrado na string."""
    m = re.search(r"(\d+)\s*,\s*(\d+)", s)
    return (int(m.group(1)), int(m.group(2))) if m else None


def parse_posicao(path: Path) -> list[tuple[str, int, int]]:
    """
    Retorna [(ENTITY_ID, x, y), ...] das linhas ativas.
    Ignora: linhas vazias, comentarios (#...), linhas sem '|',
    linhas de cabecalho com ':' no nome.
    """
    out = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line or "|" not in line:
            continue
        parts = [p.strip() for p in line.split("|")]
        eid = parts[0].upper()
        if ":" in eid or "FORMATO" in eid:
            continue
        coords = _xy(parts[-1])
        if coords is None:
            continue
        out.append((eid, coords[0], coords[1]))
    return out


def parse_spawns(path: Path) -> dict[str, tuple[int, int]]:
    """Retorna {SPOWN_ID_UPPER: (x, y)} de todas as linhas SPOWN_* no arquivo."""
    out = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if "|" not in line:
            continue
        parts = [p.strip() for p in line.split("|")]
        sid = parts[0].upper()
        if not sid.startswith("SPOWN"):
            continue
        coords = _xy(parts[-1])
        if coords:
            out[sid] = coords
    return out


# ─── Resolucao de asset ───────────────────────────────────────────────────────

def resolve_asset(eid: str, cat_entry: dict, reg: dict) -> tuple[int, int] | None:
    """
    Retorna (asset_type_int, asset_id) para a entidade, ou None se nao encontrado.
    NPCs: tenta <stem>_idle.png antes do nome original.
    """
    fname = cat_entry["file"].lower()
    if eid.startswith("NPC_"):
        idle = Path(fname).stem + "_idle.png"
        if idle in reg:
            r = reg[idle]
            return r["atype"], r["id"]
    if fname in reg:
        r = reg[fname]
        return r["atype"], r["id"]
    return None


# ─── Construtores de entidade JSON ────────────────────────────────────────────

def _ent(name, atype, aid, etype, flags, x, y, cw, ch) -> dict:
    return {
        "name":          name,
        "asset_type":    atype,
        "asset_id":      aid,
        "type":          etype,
        "flags":         flags,
        "x":             x,
        "y":             y,
        "coll_w":        cw,
        "coll_h":        ch,
        "coll_offset_x": -(cw // 2),
        "coll_offset_y": -ch,
        "sort_offset_y": 0,
        "from_image":    False,
    }


def player_ent(x: int, y: int) -> dict:
    return _ent("player_spawn", 0, PLAYER_ASSET_ID, "player",
                ["solid", "ysorted", "visible"],
                x, y, PLAYER_COLL_W, PLAYER_COLL_H)


def furniture_ent(name, atype, aid, x, y, cw, ch) -> dict:
    return _ent(name, atype, aid, "furniture",
                ["solid", "ysorted", "visible"], x, y, cw, ch)


def npc_ent(name, atype, aid, x, y, cw, ch) -> dict:
    e = _ent(name, atype, aid, "npc",
             ["solid", "interactable", "ysorted", "visible"], x, y, cw, ch)
    # NPCs renderizam na frente de moveis proximos (sort_y efetivo +30)
    e["sort_offset_y"] = 30
    return e


# ─── Gerador de sala ──────────────────────────────────────────────────────────

def generate_room(
    room_name:       str,
    posicao_path:    Path,
    interacoes_path: Path,
    spawn_key:       str,
    bg_asset_id:     int,
    cat:             dict,
    reg:             dict,
) -> dict:
    entries = parse_posicao(posicao_path)
    spawns  = parse_spawns(interacoes_path)

    # Conta ocorrencias para sufixos unicos (_a, _b, ...)
    counts = Counter(eid for eid, _, _ in entries)
    idx: dict[str, int] = {}
    entities: list[dict] = []
    warns = 0

    for eid, x, y in entries:
        cat_entry = cat.get(eid)
        if cat_entry is None:
            print(f"  ! [{room_name}] '{eid}' nao esta no catalogo — pulado")
            warns += 1
            continue
        if cat_entry["sala"] == "ui":
            continue  # overlays UI (DIALOGO_01, etc.) criados pelo screen_*.c
        if eid.upper().startswith("CHAO_"):
            continue  # piso/fundo: referenciado via bg_asset_id, nao como entidade

        result = resolve_asset(eid, cat_entry, reg)
        if result is None:
            print(f"  ! [{room_name}] '{eid}' sem asset correspondente no registry — pulado")
            warns += 1
            continue

        atype, aid = result
        idx[eid] = idx.get(eid, 0) + 1
        suffix = "" if counts[eid] == 1 else "_" + chr(ord("a") + idx[eid] - 1)
        name = eid.lower() + suffix

        cw, ch = cat_entry["coll_w"], cat_entry["coll_h"]
        if eid.startswith("NPC_"):
            entities.append(npc_ent(name, atype, aid, x, y, cw, ch))
        else:
            entities.append(furniture_ent(name, atype, aid, x, y, cw, ch))

    # Player sempre como primeira entidade
    if spawn_key in spawns:
        sx, sy = spawns[spawn_key]
        entities.insert(0, player_ent(sx, sy))
    else:
        print(f"  ! [{room_name}] spawn '{spawn_key}' nao encontrado em INTERACOES.txt — sem player")
        warns += 1

    print(f"  {room_name}: {len(entities)} entidades  ({warns} aviso(s))")
    return {
        "_doc": (
            f"Layout sala {room_name}. "
            "Gerado por tools/gerar_layout.py — NAO EDITE A MAO. "
            "Para alterar posicoes: edite posicao.txt / INTERACOES.txt / ENTIDADES.json "
            "e rode novamente este script."
        ),
        "room":        room_name,
        "bg_asset_id": bg_asset_id,
        "entities":    entities,
    }


# ─── Preview PNG (requer Pillow) ─────────────────────────────────────────────

def generate_preview(room: dict, out_path: Path) -> None:
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("  [preview] Pillow nao instalado (pip install pillow) — pulando")
        return

    reg_data = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    id_to_file: dict[int, str] = {
        a["id"]: a["file"] for a in reg_data["assets"] if "file" in a
    }

    canvas = Image.new("RGBA", (480, 320), (30, 30, 30, 255))

    # Piso
    bg_file = id_to_file.get(room["bg_asset_id"])
    if bg_file:
        piso_path = SPRITES / bg_file
        if piso_path.exists():
            piso = Image.open(piso_path).convert("RGBA")
            canvas.paste(piso, (0, 0), piso)

    # Entidades ordenadas por y (depth sort)
    sorted_ents = sorted(room["entities"], key=lambda e: e["y"])
    draw = ImageDraw.Draw(canvas)

    for ent in sorted_ents:
        aid = ent.get("asset_id")
        if aid is None or aid not in id_to_file:
            continue
        fpath = SPRITES / id_to_file[aid]
        if not fpath.exists() or fpath.suffix.lower() == ".txt":
            continue
        try:
            img = Image.open(fpath).convert("RGBA")
        except Exception:
            continue

        px, py = ent["x"], ent["y"]

        # Player: mostra apenas o primeiro frame (32x48)
        if ent.get("type") == "player":
            img = img.crop((0, 0, PLAYER_FRAME_W, PLAYER_FRAME_H))

        w, h = img.size
        canvas.paste(img, (px - w // 2, py - h), img)

        # Marca pivot com cruz amarela
        draw.line([(px - 4, py), (px + 4, py)], fill=(255, 220, 0, 200), width=1)
        draw.line([(px, py - 4), (px, py + 4)], fill=(255, 220, 0, 200), width=1)

    canvas.save(out_path)
    print(f"  [preview] -> {out_path.name}")


# ─── Main ─────────────────────────────────────────────────────────────────────

SALAS = [
    dict(
        room_name       = "recepcao",
        posicao_path    = SEC / "posicao.txt",
        interacoes_path = SEC / "INTERACOES.txt",
        spawn_key       = "SPOWN_INICIO_DO_JOGO",
        bg_asset_id     = 0,
    ),
    dict(
        room_name       = "empresa",
        posicao_path    = ESC / "posicao.txt",
        interacoes_path = ESC / "INTERACOES.txt",
        spawn_key       = "SPOWN_ENTRADA_ESCRITORIO",
        bg_asset_id     = 20,
    ),
]


def main() -> int:
    for required in (REGISTRY_PATH, CATALOG_PATH):
        if not required.exists():
            print(f"ERRO: arquivo nao encontrado: {required}")
            return 1

    reg = load_registry()
    cat = load_catalog()
    print(f"Registry: {len(reg)} assets  |  Catalogo: {len(cat)} tipos de entidade\n")

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    for sala in SALAS:
        layout = generate_room(**sala, cat=cat, reg=reg)

        out_json = OUT_DIR / f"{sala['room_name']}.json"
        out_json.write_text(
            json.dumps(layout, indent=2, ensure_ascii=False), encoding="utf-8"
        )
        print(f"  -> {out_json}")

        preview_out = CONSULTA / f"PREVIEW_{sala['room_name'].upper()}.png"
        generate_preview(layout, preview_out)
        print()

    print("Proximo passo: python tools/build_sd_assets.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
