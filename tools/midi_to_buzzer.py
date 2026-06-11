#!/usr/bin/env python3
"""
midi_to_buzzer.py — Converte arquivos MIDI em arrays C para buzzer passivo ESP32.

Para cada MIDI, extrai a voz melódica principal (nota mais alta em cada
instante) e gera arrays {freq_hz, duration_ms} prontos para tocar via LEDC.
Silêncio = freq 0.

Dependência:
    pip install mido

Uso:
    python tools/midi_to_buzzer.py

Saída:
    components/hardware/include/buzzer_melodies.h
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

MIDI_FILES: dict[str, Path] = {
    "recepcao":  ROOT / "CyberGameCore/CONSULTA/Aseprite Projeto/ATT_CAIO/RECEPCAO.mid",
    "escritorio": ROOT / "CyberGameCore/CONSULTA/Aseprite Projeto/ATT_CAIO/ESCRITORIO.mid",
    "ataque":    ROOT / "CyberGameCore/CONSULTA/Aseprite Projeto/ATT_CAIO/ATAQUE.mid",
}

OUT = ROOT / "components/hardware/include/buzzer_melodies.h"

# Notas mais curtas que isso (ms) são ignoradas — evita artefatos de MIDI
MIN_NOTE_MS = 30
# Silêncios mais curtos que isso (ms) são fundidos com a nota anterior
FUSE_REST_MS = 25


# ---------------------------------------------------------------------------
# Utilitários MIDI
# ---------------------------------------------------------------------------

def note_to_freq(note: int) -> int:
    """MIDI note 0-127 → Hz (A4=69=440Hz). Retorna 0 para silêncio."""
    if note <= 0:
        return 0
    return round(440.0 * math.pow(2.0, (note - 69) / 12.0))


def build_tempo_map(mid) -> list[tuple[int, int]]:
    """
    Varre todas as tracks e coleta eventos set_tempo em ordem de tick absoluto.
    Retorna lista de (abs_tick, tempo_us) ordenada por tick.
    Sempre começa com (0, 500000) = 120 BPM como default.
    """
    changes: dict[int, int] = {0: 500000}
    for track in mid.tracks:
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            if msg.type == "set_tempo":
                changes[abs_tick] = msg.tempo
    return sorted(changes.items())


def ticks_to_ms(tick_a: int, tick_b: int, tempo_map: list[tuple[int, int]],
                tpb: int) -> float:
    """
    Converte o intervalo [tick_a, tick_b) para milissegundos respeitando
    mudanças de tempo intermediárias.
    """
    if tick_b <= tick_a:
        return 0.0
    ms = 0.0
    for i, (t_tick, t_us) in enumerate(tempo_map):
        next_tick = tempo_map[i + 1][0] if i + 1 < len(tempo_map) else tick_b
        seg_s = max(tick_a, t_tick)
        seg_e = min(tick_b, next_tick)
        if seg_s < seg_e:
            ms += (seg_e - seg_s) * t_us / (tpb * 1000.0)
        if next_tick >= tick_b:
            break
    return ms


# ---------------------------------------------------------------------------
# Extração da melodia
# ---------------------------------------------------------------------------

def extract_melody(midi_path: Path) -> list[tuple[int, int]]:
    """
    Extrai a melodia como lista de (freq_hz, duration_ms).
    Estratégia: em cada intervalo de tempo, a nota mais alta vencedora.
    Canais de percussão (ch 9) são ignorados.
    """
    import mido  # importado aqui para dar erro claro se não instalado

    mid = mido.MidiFile(str(midi_path))
    tpb = mid.ticks_per_beat
    tempo_map = build_tempo_map(mid)

    # ── 1. Coletar todos os segmentos note_on/note_off ──────────────────────
    # Formato: (tick_start, tick_end, note)
    segments: list[tuple[int, int, int]] = []
    pending: dict[tuple[int, int], int] = {}  # (channel, note) -> tick_start

    for track in mid.tracks:
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            if msg.type not in ("note_on", "note_off"):
                continue
            if msg.channel == 9:  # percussão — pular
                continue
            key = (msg.channel, msg.note)
            is_on = (msg.type == "note_on" and msg.velocity > 0)
            if is_on:
                if key not in pending:
                    pending[key] = abs_tick
            else:
                if key in pending:
                    segments.append((pending.pop(key), abs_tick, msg.note))

    # Fechar notas que ficaram abertas
    if segments or pending:
        last_tick = max(
            (s[1] for s in segments),
            *pending.values(),
            default=0
        )
        for (_, note), t_start in pending.items():
            segments.append((t_start, last_tick, note))

    if not segments:
        print("  [AVISO] Nenhuma nota encontrada.")
        return []

    # ── 2. Construir linha do tempo por breakpoints ─────────────────────────
    breakpoints = sorted({t for seg in segments for t in (seg[0], seg[1])})

    melody_raw: list[tuple[int, int]] = []  # (freq, dur_ms_rounded)
    for i in range(len(breakpoints) - 1):
        t0, t1 = breakpoints[i], breakpoints[i + 1]
        mid_tick = (t0 + t1) // 2
        active = [n for s, e, n in segments if s <= mid_tick < e]
        note = max(active) if active else 0
        dur_ms = round(ticks_to_ms(t0, t1, tempo_map, tpb))
        if dur_ms < 1:
            continue
        melody_raw.append((note_to_freq(note), dur_ms))

    # ── 3. Mesclar notas consecutivas iguais ───────────────────────────────
    merged: list[list[int]] = []
    for freq, dur in melody_raw:
        if merged and merged[-1][0] == freq:
            merged[-1][1] += dur
        else:
            merged.append([freq, dur])

    # ── 4. Fundir silêncios muito curtos com nota anterior ──────────────────
    fused: list[list[int]] = []
    for freq, dur in merged:
        if freq == 0 and dur < FUSE_REST_MS and fused:
            fused[-1][1] += dur  # absorve o silêncio curto
        else:
            fused.append([freq, dur])

    # ── 5. Descartar notas muito curtas ────────────────────────────────────
    cleaned = [[f, d] for f, d in fused if d >= MIN_NOTE_MS]

    # ── 6. Mesclar novamente após limpeza ───────────────────────────────────
    final: list[tuple[int, int]] = []
    for freq, dur in cleaned:
        if final and final[-1][0] == freq:
            final[-1] = (final[-1][0], final[-1][1] + dur)
        else:
            final.append((freq, dur))

    return final


# ---------------------------------------------------------------------------
# Geração do header C
# ---------------------------------------------------------------------------

def c_array(name: str, melody: list[tuple[int, int]]) -> str:
    lines = [f"/* {name.upper()} — {len(melody)} notas */"]
    lines.append(f"static const uint16_t melody_{name}[][2] = {{")
    for i, (freq, dur) in enumerate(melody):
        comma = "," if i < len(melody) - 1 else ""
        lines.append(f"    {{{freq:5d}, {dur:5d}}}{comma}")
    lines.append("};")
    lines.append(f"#define MELODY_{name.upper()}_LEN  {len(melody)}u")
    return "\n".join(lines)


def main() -> None:
    try:
        import mido  # noqa: F401
    except ImportError:
        print("Erro: instale mido primeiro:  pip install mido")
        sys.exit(1)

    OUT.parent.mkdir(parents=True, exist_ok=True)

    sections: list[str] = [
        "#pragma once",
        "/* buzzer_melodies.h — gerado por tools/midi_to_buzzer.py — NAO EDITE */",
        "/* Formato de cada entrada: { freq_hz, duration_ms }                  */",
        "/* freq_hz == 0 significa silencio (pausa).                           */",
        "#include <stdint.h>",
        "",
    ]

    for name, path in MIDI_FILES.items():
        if not path.exists():
            print(f"[AVISO] Não encontrado: {path}")
            continue
        print(f"Processando {path.name} ...", end=" ", flush=True)
        melody = extract_melody(path)
        if not melody:
            print("vazia — pulando.")
            continue
        total_s = sum(d for _, d in melody) // 1000
        size_bytes = len(melody) * 4
        print(f"{len(melody)} notas, ~{total_s}s, {size_bytes} bytes em flash")
        sections.append(c_array(name, melody))
        sections.append("")

    OUT.write_text("\n".join(sections), encoding="utf-8")
    print(f"\nGerado: {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
