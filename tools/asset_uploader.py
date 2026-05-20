#!/usr/bin/env python3
"""
asset_uploader.py — gerencia os assets na NAND do CyberGame via USB CDC.

Fala o protocolo de recovery (recovery_protocol.py). O aparelho precisa estar
em modo recovery: boot segurando PWR+REC.

Uso:
  python tools/asset_uploader.py <PORTA> <comando> [args]

Comandos:
  ping                          sanity check (PING -> PONG)
  ls                            lista os assets gravados na NAND
  upload [nome]                 grava na NAND os assets do asset_registry.json
                                (todos, ou so o asset 'nome' se informado)
  download <tipo> <id> <arq>    baixa um asset; --png decodifica pra imagem
  erase <tipo>                  apaga todos os assets de uma categoria
  reset                         factory reset — apaga TODOS os assets
  selftest                      validacao fisica do NAND (~5-10 s, destrutiva
                                nos blocos de diagnostico)

  <tipo> = sprite | font | sound

Pre-requisitos: pip install pyserial   (upload/--png tambem precisam: pillow)
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    import serial  # pyserial
    from recovery_protocol import (RecoveryClient, ProtocolError, NackError,
                                   ASSET_TYPE_NAMES)
except ImportError as e:
    print(f"ERRO: dependencia faltando ({e}). Rode: pip install pyserial")
    sys.exit(2)

ROOT          = Path(__file__).resolve().parent.parent
REGISTRY_PATH = ROOT / "assets" / "asset_registry.json"
SPRITES_DIR   = ROOT / "assets" / "sprites"
DIALOGOS_DIR  = ROOT / "assets" / "dialogos"

# nome de categoria (CLI / registry) -> asset_type_t
_TYPE_BY_NAME = {name: val for val, name in ASSET_TYPE_NAMES.items()}


def _parse_type(name: str) -> int:
    try:
        return _TYPE_BY_NAME[name.lower()]
    except KeyError:
        raise SystemExit(f"tipo invalido: '{name}'. Use: "
                         + ", ".join(sorted(_TYPE_BY_NAME)))


def _load_registry() -> list[dict]:
    if not REGISTRY_PATH.exists():
        raise SystemExit(f"registry nao encontrado: {REGISTRY_PATH}")
    with open(REGISTRY_PATH, encoding="utf-8") as f:
        return json.load(f)["assets"]


def cmd_ping(cli: RecoveryClient, _args) -> int:
    print(f"PONG — protocolo v{cli.ping()}")
    return 0


def cmd_ls(cli: RecoveryClient, _args) -> int:
    entries = cli.list()
    if not entries:
        print("NAND vazia — nenhum asset gravado (cold start).")
        return 0
    print(f"{'TIPO':<8} {'ID':>5} {'TAMANHO':>10} {'CRC32':>10}  NOME")
    print("-" * 60)
    for e in sorted(entries, key=lambda x: (x["type"], x["id"])):
        tname = ASSET_TYPE_NAMES.get(e["type"], f"?{e['type']}")
        print(f"{tname:<8} {e['id']:>5} {e['size']:>10} "
              f"0x{e['crc']:08X}  {e['name']}")
    print(f"\n{len(entries)} asset(s).")
    return 0


def cmd_upload(cli: RecoveryClient, args) -> int:
    try:
        from asset_codec import build_blob, build_dialog_blob
    except ImportError as e:
        print(f"FAIL: upload precisa do Pillow ({e}). Rode: pip install pillow")
        return 2

    entries = _load_registry()
    if args.nome:
        entries = [e for e in entries if e["name"] == args.nome]
        if not entries:
            print(f"FAIL: asset '{args.nome}' nao esta no registry.")
            return 1

    print(f"Subindo {len(entries)} asset(s) de {REGISTRY_PATH.name}...")
    total = 0
    for e in entries:
        # Dispatch pelo sufixo do 'file': .txt -> dialog, demais -> sprite PNG
        if e["file"].lower().endswith(".txt"):
            src = DIALOGOS_DIR / e["file"]
            if not src.exists():
                print(f"  FAIL: arquivo nao encontrado: {src}")
                return 1
            blob = build_dialog_blob(src)
        else:
            src = SPRITES_DIR / e["file"]
            if not src.exists():
                print(f"  FAIL: arquivo nao encontrado: {src}")
                return 1
            blob = build_blob(src, crop=e.get("crop", True))
        cli.put(_parse_type(e["type"]), e["id"], e["name"], blob)
        total += len(blob)
        print(f"  OK  {e['type']:<7} id={e['id']:<3} {e['name']:<22} {len(blob):>8} bytes")
    print(f"\n{len(entries)} asset(s), {total} bytes gravados na NAND.")
    return 0


def cmd_download(cli: RecoveryClient, args) -> int:
    atype = _parse_type(args.tipo)
    blob = cli.get(atype, args.id)
    if args.png:
        try:
            from asset_codec import blob_to_png
        except ImportError as e:
            print(f"FAIL: --png precisa do Pillow ({e}). Rode: pip install pillow")
            return 2
        w, h = blob_to_png(blob, args.arquivo)
        print(f"OK — {args.tipo} id={args.id} decodificado {w}x{h} -> {args.arquivo}")
    else:
        with open(args.arquivo, "wb") as f:
            f.write(blob)
        print(f"OK — {len(blob)} bytes crus de {args.tipo} id={args.id} -> {args.arquivo}")
    return 0


def cmd_erase(cli: RecoveryClient, args) -> int:
    cli.erase_category(_parse_type(args.tipo))
    print(f"OK — categoria '{args.tipo}' apagada.")
    return 0


def cmd_reset(cli: RecoveryClient, _args) -> int:
    cli.factory_reset()
    print("OK — factory reset concluido. NAND de assets zerada.")
    return 0


def cmd_selftest(cli: RecoveryClient, _args) -> int:
    print("Rodando validacao fisica do NAND (pode levar ~5-10 s)...")
    cli.selftest()
    print("OK — selftest concluido (ver detalhes no log serial do ESP).")
    return 0


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        description="Gerencia assets na NAND do CyberGame via USB CDC.",
        epilog="O aparelho precisa estar em modo recovery (boot com PWR+REC).")
    ap.add_argument("port", help="porta serial CDC (ex.: COM20, /dev/ttyACM0)")
    sub = ap.add_subparsers(dest="cmd", required=True, metavar="comando")

    sub.add_parser("ping", help="sanity check PING -> PONG").set_defaults(fn=cmd_ping)
    sub.add_parser("ls", help="lista os assets na NAND").set_defaults(fn=cmd_ls)

    p_up = sub.add_parser("upload", help="grava os assets do registry na NAND")
    p_up.add_argument("nome", nargs="?", default=None,
                      help="opcional: nome de um unico asset (default: todos)")
    p_up.set_defaults(fn=cmd_upload)

    p_dl = sub.add_parser("download", help="baixa um asset da NAND")
    p_dl.add_argument("tipo", help="sprite | font | sound")
    p_dl.add_argument("id", type=int, help="id do asset")
    p_dl.add_argument("arquivo", help="arquivo de saida (.bin, ou .png com --png)")
    p_dl.add_argument("--png", action="store_true",
                      help="decodifica o blob pra imagem em vez de salvar cru")
    p_dl.set_defaults(fn=cmd_download)

    p_er = sub.add_parser("erase", help="apaga uma categoria inteira")
    p_er.add_argument("tipo", help="sprite | font | sound")
    p_er.set_defaults(fn=cmd_erase)

    sub.add_parser("reset", help="factory reset (apaga tudo)").set_defaults(fn=cmd_reset)
    sub.add_parser("selftest", help="validacao fisica do NAND").set_defaults(fn=cmd_selftest)
    return ap


def main() -> int:
    args = build_parser().parse_args()
    try:
        with RecoveryClient(args.port) as cli:
            return args.fn(cli, args)
    except serial.SerialException as e:
        print(f"FAIL: nao consegui abrir a porta {args.port}: {e}")
        return 2
    except (ProtocolError, NackError) as e:
        print(f"FAIL: {e}")
        return 1
    except OSError as e:
        print(f"FAIL: erro de I/O na porta: {e}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
