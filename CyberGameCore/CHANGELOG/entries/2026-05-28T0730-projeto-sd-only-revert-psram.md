---
data: 2026-05-28T07:30
tipo: edit + revert (config)
escopo: sdkconfig, decisao de projeto
trigger: usuario decidiu (urgente) consolidar o projeto em SD-only, preservar a NAND, e parar de quebrar a placa com a PSRAM
commits:
  - (pendente)
---

# Projeto consolidado em SD-only + PSRAM revertida (de novo)

## Por que

Decisao do usuario 2026-05-28: usar **apenas o cartao SD**, deixar a NAND de
lado (sem perder o trabalho dela), e ter uma pasta pronta pra copiar no cartao.
E parar de arriscar boot-loop com a PSRAM.

Tentativa de PSRAM @40MHz (entrada [[2026-05-28T0610-reabilita-psram-octal]])
reproduziu o boot loop: a PSRAM eh **detectada** (`Found 8MB PSRAM device`,
vendor AP Memory, gen3, good-die Pass, VCC 3V) mas trava em `cpu_start:
Multicore app` com `RTCWDT_RTC_RST` em loop — a 80MHz E a 40MHz. IGNORE_NOTFOUND
NAO evita (eh hang real). Conclusao: limitacao de hardware da PSRAM nesta placa
(provavel alimentacao/decoupling; o chip responde mas pendura sob carga dos 2
nucleos). Ver [[hw_psram_boot_loop]].

## O que mudou

- `sdkconfig`: PSRAM OFF de novo — `# CONFIG_SPIRAM is not set` +
  `# CONFIG_ESP32S3_SPIRAM_SUPPORT is not set` (reconfigure RC=0, PSRAM off
  confirmado). Firmware bom reconstruido (boota ate o menu, le do SD).

- **Projeto agora eh SD-only de fato:** leitura de assets 100%% via `/sd`
  (`asset_loader`/`dialog_loader`, Fase 2). NAND nao eh inicializada no boot.

- **NAND PRESERVADA (nao apagada):** os componentes `components/storage_hal`,
  `components/asset_store`, `components/recovery` continuam no projeto, intactos.
  As versoes antigas dos loaders (que liam da NAND) estao no git HEAD + nos
  CHANGELOG das Fases 3-6. Reativavel no futuro.

## Estado das imagens (importante)

O cartao le os assets, mas os fundos de sala (300 KB cada) **nao cabem na RAM
interna (~234 KB) sem PSRAM**. Como a PSRAM trava esta placa, as salas sobem
sem fundo (NO_MEM) por ora. Isso NAO eh problema de NAND vs SD — eh memoria.
Caminhos futuros: (a) resolver PSRAM no hardware; (b) render por streaming do
fundo direto do cartao (sem guardar 300 KB) — refactor maior.

## Pasta do cartao

`tools/build_sd_assets.py` gera `sdcard/assets/` (18 arquivos `0_0.bin`..`0_17.bin`,
~2,1 MB). Usuario copia a pasta `assets` pra raiz do cartao (FAT32).

## Links
- [[2026-05-28T0610-reabilita-psram-octal]], [[2026-05-26T1830-disable-psram]]
- [[2026-05-28T0540-fase2-assets-na-sd]], [[project_storage_nand_to_sd]]
- [[hw_psram_boot_loop]], [[build_environment]]
