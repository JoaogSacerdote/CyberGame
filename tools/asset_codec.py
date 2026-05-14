"""
asset_codec.py — encoder de imagens do CyberGame (fonte unica do pipeline).

Converte PNG -> RGB565/RGB565A8. Usado por:
  - convert_assets.py : gera os arrays C embarcados (legado, ate a Fase 6)
  - asset_uploader.py : gera os blobs gravados na NAND

Espelha o contrato de components/assets/include/asset_blob.h.

Requer: pip install pillow
"""
from __future__ import annotations

import struct

from PIL import Image

# ---- Contrato do header de blob (espelho de asset_blob.h) ----
ASSET_BLOB_MAGIC   = 0x424C4241          # 'ABLB'
ASSET_BLOB_VERSION = 1

PIXFMT_RGB565   = 0
PIXFMT_RGB565A8 = 1

# magic, version, w, h, stride, off_x, off_y, data_size, pixel_format, reserved[11]
_HEADER_FMT = "<IHHHHhhIB11s"
HEADER_SIZE = struct.calcsize(_HEADER_FMT)
assert HEADER_SIZE == 32, f"header deveria ter 32 bytes, tem {HEADER_SIZE}"


def encode_rgb565(r: int, g: int, b: int) -> int:
    """Codifica um pixel RGB888 em RGB565 (5-6-5)."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def png_to_pixels(png_path, crop: bool = True):
    """PNG -> (pixel_bytes, w, h, off_x, off_y, pixfmt).

    Em RGB565A8, pixel_bytes = bloco RGB565 contiguo + bloco A8 contiguo
    (mesmo layout do lv_image_dsc_t e do convert_assets.py).

    crop=True recorta ao bounding box dos pixels visiveis e devolve o offset
    original em (off_x, off_y) — economiza flash/NAND. crop=False preserva a
    imagem inteira (necessario p/ sprite-sheets como o do player).

    Otimizacao: imagem com canal alpha 100% opaco vira RGB565 (descarta A8).
    """
    img = Image.open(png_path)
    has_alpha = img.mode in ("RGBA", "LA") or "transparency" in img.info

    if has_alpha:
        img = img.convert("RGBA")
        if crop:
            bbox = img.getbbox()
            if bbox:
                off_x, off_y = bbox[0], bbox[1]
                img = img.crop(bbox)
            else:
                off_x, off_y = 0, 0
        else:
            off_x, off_y = 0, 0
        # se todos os pixels sao 100% opacos, descarta o alpha
        if img.split()[3].getextrema() == (255, 255):
            img = img.convert("RGB")
            has_alpha = False
    else:
        img = img.convert("RGB")
        off_x, off_y = 0, 0

    w, h = img.size
    px = img.load()
    rgb = bytearray()
    if has_alpha:
        alpha = bytearray()
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                v = encode_rgb565(r, g, b)
                rgb.append(v & 0xFF)
                rgb.append((v >> 8) & 0xFF)
                alpha.append(a)
        return bytes(rgb) + bytes(alpha), w, h, off_x, off_y, PIXFMT_RGB565A8

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            v = encode_rgb565(r, g, b)
            rgb.append(v & 0xFF)
            rgb.append((v >> 8) & 0xFF)
    return bytes(rgb), w, h, off_x, off_y, PIXFMT_RGB565


def build_blob(png_path, crop: bool = True) -> bytes:
    """PNG -> blob pronto pra gravar na NAND: header(32 B) + pixels."""
    pixels, w, h, off_x, off_y, pixfmt = png_to_pixels(png_path, crop=crop)
    header = struct.pack(_HEADER_FMT, ASSET_BLOB_MAGIC, ASSET_BLOB_VERSION,
                         w, h, w * 2, off_x, off_y, len(pixels), pixfmt,
                         b"\x00" * 11)
    return header + pixels


def parse_blob(blob: bytes) -> dict:
    """Valida o header de um blob e devolve seus campos + os bytes de pixel."""
    if len(blob) < HEADER_SIZE:
        raise ValueError(f"blob curto demais: {len(blob)} < {HEADER_SIZE} bytes")
    (magic, version, w, h, stride, off_x, off_y,
     data_size, pixfmt, _) = struct.unpack(_HEADER_FMT, blob[:HEADER_SIZE])
    if magic != ASSET_BLOB_MAGIC:
        raise ValueError(f"magic invalido: 0x{magic:08X} (esperado 0x{ASSET_BLOB_MAGIC:08X})")
    pixels = blob[HEADER_SIZE:HEADER_SIZE + data_size]
    if len(pixels) != data_size:
        raise ValueError(f"data_size={data_size} mas so ha {len(pixels)} bytes de pixel")
    return {"version": version, "w": w, "h": h, "stride": stride,
            "off_x": off_x, "off_y": off_y, "pixel_format": pixfmt, "pixels": pixels}


def blob_to_png(blob: bytes, out_path) -> tuple[int, int]:
    """Reconstroi um PNG a partir de um blob (RGB565[A8] -> RGBA).

    Usado pelo `asset_uploader.py download --png` para inspecionar visualmente
    o que esta gravado na NAND. Retorna (w, h)."""
    info = parse_blob(blob)
    w, h, pixfmt, pixels = info["w"], info["h"], info["pixel_format"], info["pixels"]
    rgb_len = w * h * 2
    out = Image.new("RGBA", (w, h))
    dst = out.load()
    for i in range(w * h):
        v = pixels[i * 2] | (pixels[i * 2 + 1] << 8)
        r5, g6, b5 = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
        # expande 5/6/5 -> 8 bits replicando os bits altos
        r = (r5 << 3) | (r5 >> 2)
        g = (g6 << 2) | (g6 >> 4)
        b = (b5 << 3) | (b5 >> 2)
        a = pixels[rgb_len + i] if pixfmt == PIXFMT_RGB565A8 else 255
        dst[i % w, i // w] = (r, g, b, a)
    out.save(out_path)
    return w, h
