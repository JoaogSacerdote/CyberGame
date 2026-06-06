---
date: 2026-05-26 20:15
type: edit
files:
  - components/assets/asset_loader.c
  - components/assets/dialog_loader.c
  - components/ui_debug/ui_debug.c
  - components/hardware/pmu.c
  - components/hardware/storage_hal.c
  - components/hardware/button_hal.c
  - components/hardware/include/button_hal.h
session: bring-up placa nova
---

# Fixes dos findings da auditoria 2026-05-26

## Motivo
Auditoria automática (`tools/audit-project.ps1`) identificou 3 CRITICAL (allocs em PSRAM com PSRAM desligada) + 4 WARNING (comentários com GPIOs antigos). Corrigir antes que vire bug em runtime ou confunda debug futuro.

## Fix 1: SPIRAM allocs com fallback (CRITICAL)

### asset_loader.c:121
**Antes:**
```c
/* 2. aloca os pixels na PSRAM */
void *pixbuf = heap_caps_malloc(hdr.data_size, MALLOC_CAP_SPIRAM);
if (pixbuf == NULL) {
    ESP_LOGE(TAG, "asset (%d,%u): PSRAM insuficiente para %u bytes",
             type, id, (unsigned)hdr.data_size);
    return ESP_ERR_NO_MEM;
}
```
**Depois:** tenta PSRAM, cai pra heap interna (8-bit) se PSRAM ausente/cheia.
```c
/* 2. aloca os pixels: prefere PSRAM, mas cai pra heap interna se PSRAM
 * desligada (modo bring-up) ou cheia. Sprites grandes (>200 KB) podem
 * falhar na DRAM — esse risco eh inerente ao modo sem PSRAM. */
void *pixbuf = heap_caps_malloc(hdr.data_size, MALLOC_CAP_SPIRAM);
if (pixbuf == NULL) {
    pixbuf = heap_caps_malloc(hdr.data_size, MALLOC_CAP_8BIT);
}
if (pixbuf == NULL) {
    ESP_LOGE(TAG, "asset (%d,%u): sem memoria para %u bytes (PSRAM+DRAM esgotadas)",
             type, id, (unsigned)hdr.data_size);
    return ESP_ERR_NO_MEM;
}
```

### dialog_loader.c:68
Mesmo padrão de fallback aplicado.

### ui_debug.c:271 (estético)
**Antes:** mostra "heap: X/Y KB" onde Y = free_size(SPIRAM). Sem PSRAM, Y=0 sempre.
**Depois:** se SPIRAM total = 0, esconde o segundo número.
```c
const unsigned psram_kb = (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
const unsigned psram_total = (unsigned)(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024);
if (dram_kb != s_last_dram_kb || psram_kb != s_last_psram_kb) {
    if (psram_total == 0) {
        lv_label_set_text_fmt(s_lbl_heap, "heap: %u KB (sem PSRAM)", dram_kb);
    } else {
        lv_label_set_text_fmt(s_lbl_heap, "heap: %u/%u KB", dram_kb, psram_kb);
    }
    s_last_dram_kb  = dram_kb;
    s_last_psram_kb = psram_kb;
}
```

## Fix 2: Stale GPIO references nos comentários (WARNING)

### pmu.c:65
**Antes:** `"GPIO 4 fica flutuando e ruido pode disparar wake-ups espurios."`
**Depois:** `"GPIO 14 (PMU_PIN_PWR) fica flutuando e ruido pode disparar wake-ups espurios."`

### storage_hal.c:230
**Antes:** `"Verificar fios: CS=GPIO10, MISO=GPIO9, MOSI=GPIO7, SCK=GPIO8, /WP e /HOLD em 3.3V"`
**Depois:** `"Verificar fios: CS=GPIO41, MISO=GPIO5, MOSI=GPIO15, SCK=GPIO7, /WP e /HOLD em 3.3V"`

### button_hal.c:89
**Antes:** `"GPIO 3 era REC para o PMU. Se o usuario ainda esta segurando..."`
**Depois:** `"GPIO 13 (BTN_START) compartilha com REC do PMU. Se o usuario..."`

### button_hal.h:16
**Antes:** `"BTN_START GPIO 3 dual-use: PMU le como REC..."`
**Depois:** `"BTN_START GPIO 13 dual-use: PMU le como REC..."`

## Resultado esperado
Próxima execução do `audit-project.ps1` deve mostrar **0 CRITICAL** e **0 WARNING** na categoria STALE_GPIO. SPIRAM_ALLOC vai virar INFO (alocações ainda tentam PSRAM primeiro, mas fallback evita NULL).

## Links
- Entrada relacionada: [[2026-05-26T2000-audit-routine]] (origem dos findings)
- Entrada relacionada: [[2026-05-26T1830-disable-psram]] (causa raiz da PSRAM desligada)
