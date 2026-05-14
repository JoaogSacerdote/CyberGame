#!/usr/bin/env python3
"""Smoke-test do protocolo de recovery: envia CMD_PING e valida o RESP_PONG.

Confirma enquadramento de frame, CRC32 e round-trip USB CDC end-to-end. A
logica de transporte vive em recovery_protocol.py (compartilhada com o
asset_uploader.py).

Uso:
    python test_ping.py <COM_PORT>

Pre-requisito : pip install pyserial
Pre-condicao  : aparelho em modo recovery (boot com PWR+REC).
"""
from __future__ import annotations

import sys

try:
    import serial  # pyserial
    from recovery_protocol import (RecoveryClient, ProtocolError, NackError,
                                   PROTO_VERSION)
except ImportError as e:
    print(f"ERRO: dependencia faltando ({e}). Rode: pip install pyserial")
    sys.exit(2)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    port = sys.argv[1]
    print(f"Conectando em {port}...")
    try:
        with RecoveryClient(port) as cli:
            print("-> PING")
            ver = cli.ping()
            print(f"<- PONG (protocolo v{ver})")
    except serial.SerialException as e:
        print(f"FAIL: nao consegui abrir a porta: {e}")
        return 2
    except (ProtocolError, NackError) as e:
        print(f"FAIL: {e}")
        return 1

    if ver == PROTO_VERSION:
        print("OK — framing, CRC e round-trip USB CDC validados.")
        return 0
    print(f"FAIL: versao do firmware (v{ver}) != script (v{PROTO_VERSION})")
    return 1


if __name__ == "__main__":
    sys.exit(main())
