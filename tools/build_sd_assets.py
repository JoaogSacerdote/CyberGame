"""
build_sd_assets.py — gera os arquivos de asset para o cartao microSD do CyberGame.

Le assets/asset_registry.json, converte cada PNG (e o .txt de dialogo) usando
asset_codec.py, e grava cada blob como <out>/assets/<type>_<id>.bin.

Depois e so copiar a pasta <out>/assets/ para a RAIZ do cartao microSD. No
firmware o asset_loader abre /sd/assets/<type>_<id>.bin (mesmo formato de blob
que antes vivia na NAND).

Uso:
    python tools/build_sd_assets.py [--out sdcard]

Requer: pip install pillow
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# asset_codec.py vive na mesma pasta tools/
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asset_codec  # noqa: E402

# Espelha asset_type_t (components/asset_store/include/asset_store.h)
TYPE_MAP = {"sprite": 0, "font": 1, "sound": 2}

ROOT = Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "assets" / "asset_registry.json"
SPRITES_DIR = ROOT / "assets" / "sprites"


def main() -> int:
    ap = argparse.ArgumentParser(description="Gera os assets do cartao microSD")
    ap.add_argument("--out", default=str(ROOT / "sdcard"),
                    help="pasta de saida (default: <repo>/sdcard)")
    args = ap.parse_args()

    out_assets = Path(args.out) / "assets"
    out_assets.mkdir(parents=True, exist_ok=True)

    reg = json.loads(REGISTRY.read_text(encoding="utf-8"))

    written = 0
    missing = 0
    total_bytes = 0
    for a in reg["assets"]:
        if a["type"] not in TYPE_MAP:
            print(f"  ! tipo desconhecido '{a['type']}' em id={a['id']}", file=sys.stderr)
            continue
        type_int = TYPE_MAP[a["type"]]
        aid = a["id"]
        src = SPRITES_DIR / a["file"]

        if not src.exists():
            print(f"  ! FALTANDO: {src}", file=sys.stderr)
            missing += 1
            continue

        if src.suffix.lower() == ".txt":
            blob = asset_codec.build_dialog_blob(str(src))
            kind = "dialog"
        else:
            crop = a.get("crop", True)
            blob = asset_codec.build_blob(str(src), crop=crop)
            kind = "img"

        out_file = out_assets / f"{type_int}_{aid}.bin"
        out_file.write_bytes(blob)
        written += 1
        total_bytes += len(blob)
        print(f"  {out_file.name:10s} <- {a['name']:20s} ({kind}, {len(blob):>8d} B)")

    print(f"\n{written} assets gravados ({total_bytes/1024:.1f} KB) em {out_assets}")
    if missing:
        print(f"ATENCAO: {missing} arquivo(s) faltando — ver avisos acima.", file=sys.stderr)
    print(f"\nProximo passo: copie o conteudo de '{out_assets}' para a pasta")
    print("'/assets' na RAIZ do cartao microSD (deve ficar /assets/0_0.bin, etc.).")
    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
