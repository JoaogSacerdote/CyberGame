"""
recovery_protocol.py — camada de transporte do protocolo de recovery (USB CDC).

Espelha components/recovery/include/recovery_proto.h. Importado por
test_ping.py e asset_uploader.py — fonte unica da logica de framing/CRC no PC.

Frame: SOF(0xA5) | type | payload_len(u16 LE) | payload | crc32(LE)
       crc32 = esp_rom_crc32_le(0, type..payload) = zlib.crc32 padrao.
"""
from __future__ import annotations

import struct
import zlib

import serial  # pyserial

# ---- Constantes do protocolo (espelho de recovery_proto.h) ----
PROTO_VERSION = 1
SOF           = 0xA5
MAX_PAYLOAD   = 2048

# Comandos: PC -> ESP
CMD_PING          = 0x01
CMD_LIST          = 0x02
CMD_PUT_BEGIN     = 0x03
CMD_PUT_DATA      = 0x04
CMD_PUT_END       = 0x05
CMD_PUT_ABORT     = 0x06
CMD_GET           = 0x07
CMD_ERASE_CAT     = 0x08
CMD_FACTORY_RESET = 0x09
CMD_SELFTEST      = 0x0A

# Respostas: ESP -> PC
RESP_ACK   = 0x80
RESP_NACK  = 0x81
RESP_PONG  = 0x82
RESP_INFO  = 0x83
RESP_DATA  = 0x84

# asset_type_t (espelho de asset_store.h)
ASSET_TYPE_SPRITE = 0
ASSET_TYPE_FONT   = 1
ASSET_TYPE_SOUND  = 2
ASSET_TYPE_NAMES  = {ASSET_TYPE_SPRITE: "sprite",
                     ASSET_TYPE_FONT:   "font",
                     ASSET_TYPE_SOUND:  "sound"}

NAME_MAX = 40

# Layouts de payload (packed, little-endian — recovery_proto.h)
_PUT_BEGIN_FMT = "<BHII40s"   # type, id, size, crc, name -> 51 bytes
_GET_REQ_FMT   = "<BH"        # type, id                  ->  3 bytes
_INFO_FMT      = "<BHII40s"   # type, id, size, crc, name -> 51 bytes


def esp_crc32_le(data: bytes, crc: int = 0) -> int:
    """Equivalente ao esp_rom_crc32_le(crc, data, len) do ESP-IDF — confirmado
    em HW como a CRC-32 padrao da zlib. Compoe para acumulacao em chunks."""
    return zlib.crc32(data, crc) & 0xFFFFFFFF


def build_frame(ftype: int, payload: bytes = b"") -> bytes:
    """Monta um frame: SOF | type | len(u16 LE) | payload | crc32(LE)."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload {len(payload)} > {MAX_PAYLOAD}")
    body = bytes([ftype, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    return bytes([SOF]) + body + esp_crc32_le(body).to_bytes(4, "little")


class ProtocolError(Exception):
    """Erro de transporte: timeout, CRC, frame inesperado."""


class NackError(ProtocolError):
    """O ESP respondeu RESP_NACK com um esp_err_t."""

    def __init__(self, code: int):
        self.code = code
        super().__init__(f"NACK do ESP: esp_err_t = {code} (0x{code & 0xFFFFFFFF:X})")


class RecoveryClient:
    """Abre a porta CDC uma vez e fala o protocolo de recovery.

    Use como context manager:
        with RecoveryClient("COM20") as cli:
            cli.ping()
    """

    def __init__(self, port: str, timeout: float = 3.0):
        self._ser = serial.Serial(port, 115200, timeout=timeout)
        self._ser.reset_input_buffer()

    def close(self) -> None:
        self._ser.close()

    def __enter__(self) -> "RecoveryClient":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    # ---- frame I/O ----

    def _send(self, ftype: int, payload: bytes = b"") -> None:
        self._ser.write(build_frame(ftype, payload))

    def _recv(self, timeout: float | None = None) -> tuple[int, bytes]:
        """Le um frame. `timeout` sobrepoe o da porta so durante esta chamada
        (para comandos lentos como selftest/factory_reset)."""
        ser = self._ser
        old = ser.timeout
        if timeout is not None:
            ser.timeout = timeout
        try:
            while True:
                b = ser.read(1)
                if not b:
                    raise ProtocolError("timeout esperando SOF — aparelho em recovery?")
                if b[0] == SOF:
                    break
            hdr = ser.read(3)
            if len(hdr) != 3:
                raise ProtocolError("timeout lendo o header do frame")
            plen = hdr[1] | (hdr[2] << 8)
            if plen > MAX_PAYLOAD:
                raise ProtocolError(f"payload_len invalido na resposta: {plen}")
            rest = ser.read(plen + 4)
            if len(rest) != plen + 4:
                raise ProtocolError("timeout lendo payload/crc do frame")
            payload, rx_crc = rest[:plen], int.from_bytes(rest[plen:], "little")
            if esp_crc32_le(hdr + payload) != rx_crc:
                raise ProtocolError("CRC de frame invalido na resposta")
            return hdr[0], payload
        finally:
            if timeout is not None:
                ser.timeout = old

    def _expect_ack(self, timeout: float | None = None) -> None:
        ftype, payload = self._recv(timeout)
        if ftype == RESP_NACK:
            raise NackError(int.from_bytes(payload[:4], "little", signed=True))
        if ftype != RESP_ACK:
            raise ProtocolError(f"esperava ACK, veio type=0x{ftype:02X}")

    # ---- comandos ----

    def ping(self) -> int:
        """Retorna a versao de protocolo reportada pelo firmware."""
        self._send(CMD_PING)
        ftype, payload = self._recv()
        if ftype != RESP_PONG:
            raise ProtocolError(f"esperava PONG, veio type=0x{ftype:02X}")
        return payload[0] if payload else -1

    def list(self) -> list[dict]:
        """Lista os assets no manifest. Cada item: type/id/size/crc/name."""
        self._send(CMD_LIST)
        entries: list[dict] = []
        while True:
            ftype, payload = self._recv()
            if ftype == RESP_INFO:
                t, aid, size, crc, name = struct.unpack(_INFO_FMT, payload)
                entries.append({
                    "type": t, "id": aid, "size": size, "crc": crc,
                    "name": name.rstrip(b"\x00").decode("ascii", "replace"),
                })
            elif ftype == RESP_ACK:
                return entries
            elif ftype == RESP_NACK:
                raise NackError(int.from_bytes(payload[:4], "little", signed=True))
            else:
                raise ProtocolError(f"frame inesperado no LIST: 0x{ftype:02X}")

    def put(self, atype: int, aid: int, name: str, blob: bytes) -> None:
        """Grava um asset: PUT_BEGIN -> PUT_DATA* -> PUT_END.

        O ESP confere, no PUT_END, o CRC acumulado contra o esp_crc32_le(blob)
        anunciado aqui — divergencia vira NackError(ESP_ERR_INVALID_CRC)."""
        name_b = name.encode("ascii", "replace")[:NAME_MAX].ljust(NAME_MAX, b"\x00")
        begin = struct.pack(_PUT_BEGIN_FMT, atype, aid, len(blob),
                            esp_crc32_le(blob), name_b)
        self._send(CMD_PUT_BEGIN, begin)
        try:
            self._expect_ack(timeout=10.0)   # begin_write pode apagar blocos
            for off in range(0, len(blob), MAX_PAYLOAD):
                self._send(CMD_PUT_DATA, blob[off:off + MAX_PAYLOAD])
                self._expect_ack()
            self._send(CMD_PUT_END)
            self._expect_ack(timeout=10.0)   # commit grava o manifest
        except (ProtocolError, OSError):
            # tenta abortar a sessao no ESP para nao deixar handle preso
            try:
                self._send(CMD_PUT_ABORT)
                self._expect_ack()
            except (ProtocolError, OSError):
                pass
            raise

    def get(self, atype: int, aid: int) -> bytes:
        """Baixa um asset inteiro. Levanta NackError se nao existir."""
        self._send(CMD_GET, struct.pack(_GET_REQ_FMT, atype, aid))
        chunks: list[bytes] = []
        while True:
            ftype, payload = self._recv()
            if ftype == RESP_DATA:
                chunks.append(payload)
            elif ftype == RESP_ACK:
                return b"".join(chunks)
            elif ftype == RESP_NACK:
                raise NackError(int.from_bytes(payload[:4], "little", signed=True))
            else:
                raise ProtocolError(f"frame inesperado no GET: 0x{ftype:02X}")

    def erase_category(self, atype: int) -> None:
        self._send(CMD_ERASE_CAT, bytes([atype]))
        self._expect_ack(timeout=15.0)

    def factory_reset(self) -> None:
        self._send(CMD_FACTORY_RESET)
        self._expect_ack(timeout=30.0)

    def selftest(self) -> None:
        """Validacao fisica do NAND (~5-10 s, destrutiva nos blocos de diagnostico)."""
        self._send(CMD_SELFTEST)
        self._expect_ack(timeout=20.0)
