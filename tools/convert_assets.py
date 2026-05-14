#!/usr/bin/env python3
"""
Converte PNGs em assets/sprites/ para arrays C no formato lv_image_dsc_t (LVGL 9).

Uso:
  python tools/convert_assets.py

Saida:
  components/assets/generated/img_recepcao.c  (todas as imagens da recepcao)
  components/assets/generated/img_empresa.c   (todas as imagens da empresa)
  components/assets/generated/img_player.c    (sprite do jogador)
  components/assets/include/assets_images.h   (declaracoes externas)
  components/assets/include/assets_meta.h     (offsets x/y de cada imagem)

Comportamento:
- Imagens com transparencia: cropadas ao bounding box dos pixels visiveis (economiza
  flash). O offset original (offset_x, offset_y) e exportado em assets_meta.h para
  o codigo de render restaurar a posicao correta.
- Sprite_PLAYER.png e player: NAO cropa (precisa do sheet 96x192 intacto pra
  funcionar com lv_image_set_offset).
- Camadas sem alpha (PISO_*) viram LV_COLOR_FORMAT_RGB565.
- Camadas com alpha viram LV_COLOR_FORMAT_RGB565A8 (RGB565 block + A8 block).
- Arquivos *_NULL.png sao IGNORADOS aqui — tratados por extract_collisions.py.
"""
from pathlib import Path

from asset_codec import PIXFMT_RGB565A8, png_to_pixels

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "sprites"
OUT_GEN = ROOT / "components" / "assets" / "generated"
OUT_INC = ROOT / "components" / "assets" / "include"

# Mapeamento de nome longo do PNG -> nome curto pra usar como variavel C.
NAME_MAP = {
    "PISO_CAMADA_00": "piso",
    "PAREDES_E_OBJETOS_02": "paredes",
    "COMPLEMENTO_SEMCOLISAO_PAREDES_E_OBJETOS_03": "complemento",
    "ICONE_DE_NOTIFICACAO_RECEPCIONISTA_04": "icone_notif",
    "RECEPCIONISTA_MECHENDO_NO_COMPUTADOR_SEM_INTERACAO_05": "recep_idle",
    "RECEPCIONISTA_OLHANDO_PARA_JOGADOR_ENQUANTO_ACONTECE_DIALOGO_06": "recep_dialog",
    "CAIXA_DE_DIALOGO_RECEPCIONISTA_07": "caixa_dialogo",
    "CAIXA_DELIMITADORA_ONDE_PODE_APARECER_O_TEXTO_DO_DIALOGO_08": "caixa_texto",
    "ICONE_TAREFA_VERDE_04": "icone_verde",
    "ICONE_TAREFA_AMARELA_05": "icone_amarelo",
    "NPC_TI_TAREFA_AMARELA_PARA_CIMA_06": "npc_ti_cima",
    "NPC_TI_TAREFA_AMARELA_PARA_DIREITA_07": "npc_ti_direita",
    "NPC_TI_TAREFA_AMARELA_PARA_BAIXO_08": "npc_ti_baixo",
}

SALA_PREFIX = {"recepcao": "rec", "empresa": "emp"}


def short_name(stem):
    return NAME_MAP.get(stem, stem.lower())


def emit_c_array(name, data):
    lines = [f"static const LV_ATTRIBUTE_LARGE_CONST uint8_t {name}_data[] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def emit_image_dsc(var_name, w, h, has_alpha):
    cf = "LV_COLOR_FORMAT_RGB565A8" if has_alpha else "LV_COLOR_FORMAT_RGB565"
    # Stride e SEMPRE o do plano RGB565 (w*2). Pra RGB565A8, o canal A8
    # vem em bloco contiguo depois do bloco RGB565 — nao entra no stride.
    stride = w * 2
    return f"""
const lv_image_dsc_t {var_name} = {{
    .header = {{
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = {cf},
        .flags = 0,
        .w = {w},
        .h = {h},
        .stride = {stride},
    }},
    .data_size = sizeof({var_name}_data),
    .data = {var_name}_data,
}};
"""


def process_sala(sala_dir, prefix):
    files = sorted([p for p in sala_dir.glob("*.png") if "_NULL" not in p.name])
    parts = [
        "/* Gerado por tools/convert_assets.py — NAO EDITE A MAO */",
        "#include \"lvgl.h\"",
        "",
    ]
    meta = []  # (var_name, off_x, off_y, w, h)
    for png in files:
        short = short_name(png.stem)
        var = f"img_{prefix}_{short}"
        data, w, h, ox, oy, pixfmt = png_to_pixels(png, crop=True)
        has_alpha = pixfmt == PIXFMT_RGB565A8
        parts.append(emit_c_array(var, data))
        parts.append(emit_image_dsc(var, w, h, has_alpha))
        meta.append((var, ox, oy, w, h, has_alpha))
        print(f"  {png.name:<70} -> {var}  ({w}x{h}, off={ox},{oy}, {'RGB565A8' if has_alpha else 'RGB565'}, {len(data)/1024:.1f} KB)")
    return parts, meta


def main():
    OUT_GEN.mkdir(parents=True, exist_ok=True)
    OUT_INC.mkdir(parents=True, exist_ok=True)

    all_meta = {}

    for sala in ("recepcao", "empresa"):
        sala_dir = SRC / sala
        if not sala_dir.exists():
            print(f"Pulando {sala}: pasta nao existe")
            continue
        print(f"\n=== {sala} ===")
        prefix = SALA_PREFIX[sala]
        parts, meta = process_sala(sala_dir, prefix)
        out = OUT_GEN / f"img_{sala}.c"
        out.write_text("\n".join(parts), encoding="utf-8")
        print(f"  -> {out}")
        all_meta[sala] = meta

    # Player (sem crop)
    print("\n=== player ===")
    player_png = SRC / "Sprite_PLAYER.png"
    parts = [
        "/* Gerado por tools/convert_assets.py — NAO EDITE A MAO */",
        "#include \"lvgl.h\"",
        "",
    ]
    data, w, h, _, _, pixfmt = png_to_pixels(player_png, crop=False)
    has_alpha = pixfmt == PIXFMT_RGB565A8
    parts.append(emit_c_array("img_player", data))
    parts.append(emit_image_dsc("img_player", w, h, has_alpha))
    out = OUT_GEN / "img_player.c"
    out.write_text("\n".join(parts), encoding="utf-8")
    print(f"  Sprite_PLAYER.png -> img_player ({w}x{h}, {'RGB565A8' if has_alpha else 'RGB565'}, {len(data)/1024:.1f} KB)")
    print(f"  -> {out}")
    all_meta["player"] = [("img_player", 0, 0, w, h, has_alpha)]

    # Header de declaracoes
    h_lines = [
        "#pragma once",
        "/* Gerado por tools/convert_assets.py — NAO EDITE A MAO */",
        "#include \"lvgl.h\"",
        "",
        "#ifdef __cplusplus",
        "extern \"C\" {",
        "#endif",
        "",
    ]
    for sala, items in all_meta.items():
        h_lines.append(f"/* {sala} */")
        for var, *_ in items:
            h_lines.append(f"extern const lv_image_dsc_t {var};")
        h_lines.append("")
    h_lines.append("#ifdef __cplusplus")
    h_lines.append("}")
    h_lines.append("#endif")
    (OUT_INC / "assets_images.h").write_text("\n".join(h_lines), encoding="utf-8")
    print(f"\nHeader: {OUT_INC / 'assets_images.h'}")

    # Header de metadata (offset_x/y por imagem)
    m_lines = [
        "#pragma once",
        "/* Gerado por tools/convert_assets.py — NAO EDITE A MAO */",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    int16_t off_x;",
        "    int16_t off_y;",
        "    int16_t w;",
        "    int16_t h;",
        "} asset_meta_t;",
        "",
    ]
    for sala, items in all_meta.items():
        for var, ox, oy, w, h, _ in items:
            macro = var.upper() + "_META"
            m_lines.append(f"#define {macro} (asset_meta_t){{ .off_x = {ox}, .off_y = {oy}, .w = {w}, .h = {h} }}")
    (OUT_INC / "assets_meta.h").write_text("\n".join(m_lines), encoding="utf-8")
    print(f"Meta:   {OUT_INC / 'assets_meta.h'}")

    print("\nOK")


if __name__ == "__main__":
    main()
