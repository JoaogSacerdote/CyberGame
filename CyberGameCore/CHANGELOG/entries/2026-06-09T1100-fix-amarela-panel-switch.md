---
date: 2026-06-09T11:00
tipo: fix
escopo: ui/screen_tarefa_amarela
---

# Fix: troca de painel enquanto segura HD na tarefa amarela

## Arquivo editado

`components/ui/screen_tarefa_amarela.c` linha 210

## Antes

```c
if ((new_up || new_down) && !s_holding) {
```

## Depois

```c
if (new_up || new_down) {
```

## Causa

A condição `!s_holding` impedia a troca de painel (BAIA ↔ ESTOQUE) enquanto
o jogador carregava um HD. Resultado: após pegar um HD com A, o joystick
cima/baixo não fazia nada, impossibilitando mover o HD para o outro painel.

O clamping de coluna já estava correto (`if (s_col >= panel_max(s_panel)) s_col = ...`),
bastou remover a guarda.
