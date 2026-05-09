#!/usr/bin/env python3
"""B1 sanity test: envia PING ao ESP em modo recovery e espera PONG.

Uso:
    python test_ping.py <COM_PORT>

Exemplo:
    python test_ping.py COM21
"""

import sys

try:
    import serial
except ImportError:
    print("ERRO: pyserial nao instalado. Rode: pip install pyserial")
    sys.exit(2)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 1

    port = sys.argv[1]
    print(f"Conectando em {port}...")

    try:
        ser = serial.Serial(port, 115200, timeout=2)
    except serial.SerialException as e:
        print(f"FAIL: nao consegui abrir a porta: {e}")
        return 2

    print("Enviando PING...")
    ser.write(b"PING\n")
    resp = ser.readline().decode(errors="replace").strip()

    if resp == "PONG":
        print(f"OK — recebido: {resp}")
        return 0

    print(f"FAIL — esperado 'PONG', recebido: {resp!r}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
