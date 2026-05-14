#!/usr/bin/env python3
"""
gen_asset_ids.py — gera components/assets/include/asset_ids.h a partir de
assets/asset_registry.json.

O firmware referencia assets por simbolo (ASSET_REC_PISO) em vez de numero
magico. Rode este script sempre que mexer no registry.

Uso:
  python tools/gen_asset_ids.py
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT     = Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "assets" / "asset_registry.json"
OUT      = ROOT / "components" / "assets" / "include" / "asset_ids.h"


def main() -> int:
    assets = json.loads(REGISTRY.read_text(encoding="utf-8"))["assets"]

    # agrupa por tipo: 1 enum por categoria
    by_type: dict[str, list[dict]] = {}
    for e in assets:
        by_type.setdefault(e["type"], []).append(e)

    lines = [
        "#pragma once",
        "/* Gerado por tools/gen_asset_ids.py — NAO EDITE A MAO.",
        " * Fonte: assets/asset_registry.json",
        " * Os valores casam com o campo 'id' do registry e com o manifest do",
        " * asset_store. Use com asset_loader_load(ASSET_TYPE_*, ASSET_*, ...). */",
        "",
    ]
    for atype in sorted(by_type):
        entries = sorted(by_type[atype], key=lambda x: x["id"])
        ids = [e["id"] for e in entries]
        if len(ids) != len(set(ids)):
            raise SystemExit(f"ERRO: ids duplicados na categoria '{atype}'")
        syms = [("ASSET_" + e["name"].upper(), e["id"]) for e in entries]
        width = max(len(s) for s, _ in syms)
        lines.append(f"/* categoria: {atype} */")
        lines.append("enum {")
        for sym, aid in syms:
            lines.append(f"    {sym:<{width}} = {aid},")
        lines.append("};")
        lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"OK — {len(assets)} ids -> {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
