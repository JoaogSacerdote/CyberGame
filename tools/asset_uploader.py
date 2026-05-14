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
  download <tipo> <id> <arquivo>  baixa um asset cru pra um arquivo .bin
  erase <tipo>                  apaga todos os assets de uma categoria
  reset                         factory reset — apaga TODOS os assets
  selftest                      validacao fisica do NAND (~5-10 s, destrutiva
                                nos blocos de diagnostico)

  <tipo> = sprite | font | sound

  (o comando `upload` chega na etapa 4b, junto com o codec de imagem)

Pre-requisito: pip install pyserial
"""
from __future__ import annotations

import argparse
import sys

try:
    import serial  # pyserial
    from recovery_protocol import (RecoveryClient, ProtocolError, NackError,
                                   ASSET_TYPE_NAMES)
except ImportError as e:
    print(f"ERRO: dependencia faltando ({e}). Rode: pip install pyserial")
    sys.exit(2)

# nome de categoria (CLI) -> asset_type_t
_TYPE_BY_NAME = {name: val for val, name in ASSET_TYPE_NAMES.items()}


def _parse_type(name: str) -> int:
    try:
        return _TYPE_BY_NAME[name.lower()]
    except KeyError:
        raise SystemExit(f"tipo invalido: '{name}'. Use: "
                         + ", ".join(sorted(_TYPE_BY_NAME)))


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


def cmd_download(cli: RecoveryClient, args) -> int:
    atype = _parse_type(args.tipo)
    blob = cli.get(atype, args.id)
    with open(args.arquivo, "wb") as f:
        f.write(blob)
    print(f"OK — {len(blob)} bytes de {args.tipo} id={args.id} -> {args.arquivo}")
    return 0


def cmd_erase(cli: RecoveryClient, args) -> int:
    atype = _parse_type(args.tipo)
    cli.erase_category(atype)
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

    p_dl = sub.add_parser("download", help="baixa um asset cru pra .bin")
    p_dl.add_argument("tipo", help="sprite | font | sound")
    p_dl.add_argument("id", type=int, help="id do asset")
    p_dl.add_argument("arquivo", help="arquivo .bin de saida")
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
    except NackError as e:
        print(f"FAIL: {e}")
        return 1
    except ProtocolError as e:
        print(f"FAIL: {e}")
        return 1
    except OSError as e:
        print(f"FAIL: erro de I/O na porta: {e}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
