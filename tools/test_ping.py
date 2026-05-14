#!/usr/bin/env python3
"""Smoke-test do protocolo de recovery (Fase 3): envia CMD_PING, valida RESP_PONG.

Confirma o enquadramento de frame, o CRC32 e o round-trip USB CDC end-to-end
antes de qualquer transferencia de asset. O contrato do protocolo esta em
`components/recovery/include/recovery_proto.h` — este script o espelha.

Uso:
    python test_ping.py <COM_PORT>

Exemplo:
    python test_ping.py COM21

Pre-requisito : pip install pyserial
Pre-condicao  : aparelho em modo recovery (boot com PWR+REC).
"""
from __future__ import annotations

import sys
import zlib

try:
    import serial  # pyserial
except ImportError:
    print("ERRO: pyserial nao instalado. Rode: pip install pyserial")
    sys.exit(2)

# ---- Constantes do protocolo (espelho de recovery_proto.h) ----
SOF           = 0xA5
CMD_PING      = 0x01
RESP_NACK     = 0x81
RESP_PONG     = 0x82
PROTO_VERSION = 1
MAX_PAYLOAD   = 2048


def esp_crc32_le(data: bytes, crc: int = 0) -> int:
    """Equivalente ao esp_rom_crc32_le(crc, data, len) do ESP-IDF.

    Confirmado empiricamente: o esp_rom_crc32_le do ESP e a CRC-32 padrao da
    zlib (mesmo polinomio, mesmas inversoes). zlib.crc32(data, crc) tambem
    compoe corretamente para acumulacao em chunks (usado na Fase 4)."""
    return zlib.crc32(data, crc) & 0xFFFFFFFF


def build_frame(ftype: int, payload: bytes = b"") -> bytes:
    """Monta um frame: SOF | type | len(u16 LE) | payload | crc32(LE)."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload {len(payload)} > {MAX_PAYLOAD}")
    body = bytes([ftype, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    crc = esp_crc32_le(body)
    return bytes([SOF]) + body + crc.to_bytes(4, "little")


def read_frame(ser) -> tuple[int, bytes]:
    """Le um frame da serial. Retorna (type, payload) ou levanta em erro."""
    # 1. ressincroniza no SOF
    while True:
        b = ser.read(1)
        if not b:
            raise TimeoutError("timeout esperando SOF — aparelho esta em recovery?")
        if b[0] == SOF:
            break
    # 2. header: type + payload_len
    hdr = ser.read(3)
    if len(hdr) != 3:
        raise TimeoutError("timeout lendo o header do frame")
    ftype = hdr[0]
    plen = hdr[1] | (hdr[2] << 8)
    if plen > MAX_PAYLOAD:
        raise ValueError(f"payload_len invalido: {plen}")
    # 3. payload + crc
    rest = ser.read(plen + 4)
    if len(rest) != plen + 4:
        raise TimeoutError("timeout lendo payload/crc do frame")
    payload, rx_crc = rest[:plen], int.from_bytes(rest[plen:], "little")
    calc = esp_crc32_le(hdr + payload)
    if calc != rx_crc:
        raise ValueError(f"CRC do frame nao bate: rx=0x{rx_crc:08X} calc=0x{calc:08X}")
    return ftype, payload


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

    with ser:
        ser.reset_input_buffer()
        frame = build_frame(CMD_PING)
        print(f"-> PING  ({len(frame)} bytes: {frame.hex(' ')})")
        ser.write(frame)
        try:
            ftype, payload = read_frame(ser)
        except (TimeoutError, ValueError) as e:
            print(f"FAIL: {e}")
            return 1

    if ftype == RESP_PONG:
        ver = payload[0] if payload else -1
        print(f"<- PONG  (protocolo v{ver})")
        if ver == PROTO_VERSION:
            print("OK — framing, CRC e round-trip USB CDC validados.")
            return 0
        print(f"FAIL: versao do firmware (v{ver}) != script (v{PROTO_VERSION})")
        return 1
    if ftype == RESP_NACK:
        code = int.from_bytes(payload[:4], "little", signed=True) if len(payload) >= 4 else 0
        print(f"<- NACK  (esp_err_t = {code})")
        return 1
    print(f"FAIL: frame inesperado type=0x{ftype:02X} payload={payload.hex(' ')}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
