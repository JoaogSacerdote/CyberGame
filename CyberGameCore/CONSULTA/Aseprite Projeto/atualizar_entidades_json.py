from __future__ import annotations
import json
import re
from pathlib import Path
from PIL import Image

BASE    = Path(__file__).resolve().parent
SEC     = BASE / "Secretaria"
ESC     = BASE / "Escritorio"
ENT_TXT = BASE / "ENTIDADES.txt"
ENT_JSON = BASE / "ENTIDADES.json"
POS_SEC = SEC / "posicao.txt"
POS_ESC = ESC / "posicao.txt"


def read_lines(path: Path):
    with path.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            yield line


def parse_pair(text: str):
    nums = re.findall(r"\d+", text)
    if len(nums) >= 2:
        return int(nums[0]), int(nums[1])
    return None


def parse_entidades_txt(path: Path):
    data = {}
    for line in read_lines(path):
        if "|" not in line:
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) < 3:
            continue
        eid = parts[0]
        sprite    = parse_pair(parts[1])
        collision = parse_pair(parts[2])
        if sprite is None or collision is None:
            continue
        data[eid] = {
            "sprite":    {"w": sprite[0],    "h": sprite[1]},
            "collision": {"w": collision[0], "h": collision[1]},
        }
    return data


def parse_posicoes_ids(path: Path) -> list[str]:
    ids: list[str] = []
    seen: set[str] = set()
    for line in read_lines(path):
        if "FORMATO" in line.upper() or "NIVEL" in line.upper():
            continue
        if "|" not in line:
            continue
        eid = line.split("|", 1)[0].strip()
        if eid and eid not in seen:
            ids.append(eid)
            seen.add(eid)
    return ids


def choose_file(eid: str, sec_pngs: list[str], esc_pngs: list[str]):
    """Returns (filename, folder) — searches SEC first, then ESC."""
    exact = f"{eid}.png"
    if exact in sec_pngs:
        return exact, SEC
    if exact in esc_pngs:
        return exact, ESC
    stem_sec = {Path(n).stem.upper(): n for n in sec_pngs}
    if eid.upper() in stem_sec:
        return stem_sec[eid.upper()], SEC
    stem_esc = {Path(n).stem.upper(): n for n in esc_pngs}
    if eid.upper() in stem_esc:
        return stem_esc[eid.upper()], ESC
    return exact, None


def infer_sala(eid: str, sec_ids: set[str], esc_ids: set[str]) -> str:
    if eid == "DIALOGO_01":
        return "ui"
    in_sec = eid in sec_ids
    in_esc = eid in esc_ids
    if in_sec and not in_esc:
        return "recepcao"
    if in_esc and not in_sec:
        return "empresa"
    return ""


def build_json():
    ent_txt  = parse_entidades_txt(ENT_TXT)

    sec_list = parse_posicoes_ids(POS_SEC) if POS_SEC.exists() else []
    esc_list = parse_posicoes_ids(POS_ESC) if POS_ESC.exists() else []
    sec_ids  = set(sec_list)
    esc_ids  = set(esc_list)

    # Ordem: Secretaria primeiro, depois IDs novos do Escritório
    all_ids: list[str] = []
    seen: set[str] = set()
    for eid in sec_list + esc_list:
        if eid not in seen:
            all_ids.append(eid)
            seen.add(eid)

    sec_pngs = sorted(p.name for p in SEC.glob("*.png"))
    esc_pngs = sorted(p.name for p in ESC.glob("*.png"))

    entities = []
    missing_in_txt: list[str] = []
    missing_png:    list[str] = []

    for eid in all_ids:
        info = ent_txt.get(eid)
        if info is None:
            missing_in_txt.append(eid)
            continue

        file_name, folder = choose_file(eid, sec_pngs, esc_pngs)
        if folder is None:
            missing_png.append(eid)

        entity: dict = {
            "id":        eid,
            "sala":      infer_sala(eid, sec_ids, esc_ids),
            "file":      file_name,
            "sprite":    info["sprite"],
            "collision": info["collision"],
        }

        if folder is not None:
            img_path = folder / file_name
            if img_path.exists():
                try:
                    with Image.open(img_path) as im:
                        entity["real_size"] = {"w": im.size[0], "h": im.size[1]}
                except Exception:
                    pass

        entities.append(entity)

    doc = {
        "_doc":     "ENTIDADES.json — gerado por atualizar_entidades_json.py. NAO EDITE A MAO.",
        "version":  1,
        "entities": entities,
    }

    with ENT_JSON.open("w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)

    print(f"[OK] ENTIDADES.json: {len(entities)} entidades  "
          f"(sec={len(sec_ids)}, esc={len(esc_ids)})")
    if missing_in_txt:
        print(f"[ATENCAO] IDs ausentes em ENTIDADES.txt: {missing_in_txt}")
    if missing_png:
        print(f"[ATENCAO] IDs sem PNG correspondente: {missing_png}")


if __name__ == "__main__":
    build_json()
