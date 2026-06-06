---
data: 2026-05-28T05:40
tipo: edit + add (codigo)
escopo: components/assets, assets/asset_registry.json, tools
trigger: Fase 2 da migracao de armazenamento NAND->microSD: fazer as telas lerem os assets do cartao
commits:
  - (pendente — ainda nao commitado)
---

# Fase 2 — asset_loader e dialog_loader leem do microSD (FATFS)

## Por que

Fase 1 ([[2026-05-28T0230-sd-card-detect-disable-nand]]) desativou a NAND e
montou o microSD. Mas `asset_loader.c`/`dialog_loader.c` ainda liam da NAND
via `asset_store` (desativado) -> telas sem imagem. Fase 2 troca a fonte de
leitura para arquivos no `/sd`. Ver [[project_storage_nand_to_sd]].

Estrategia de menor risco: **mesmo formato de blob** (asset_blob_header_t 32B
+ pixels; dialog_blob 12B + offsets + payload). So muda a CAMADA de leitura:
`asset_store_read(type,id,off,...)` -> `fopen("/sd/assets/<type>_<id>.bin")` +
`fread`. A API publica dos loaders e o cache nao mudam, entao as telas nao
mudam.

## O que mudou

### 1. `asset_registry.json` — fix do dialogo
id=17 apontava pra `recepcionista.txt` (inexistente). O arquivo real e
`assets/sprites/DIALOGO.txt` (8 falas do recepcionista). Trocado.

### 2. `tools/build_sd_assets.py` (novo)
Le o registry, usa `asset_codec.build_blob` (PNG) e `build_dialog_blob` (txt),
grava cada asset como `<out>/assets/<type_int>_<id>.bin`. type_int espelha
asset_type_t (sprite=0). Saida default: `sdcard/` na raiz do repo. O usuario
copia `sdcard/assets/` pra raiz do cartao -> vira `/sd/assets/` no firmware.

### 3. `components/assets/asset_loader.c`
`load_from_nand` -> `load_from_sd`: monta o path `/sd/assets/<type>_<id>.bin`,
`fopen`/`fread` do header, valida (magic/version/pixfmt/data_size + tamanho
total do arquivo), aloca PSRAM (fallback DRAM), le os pixels. Cache load-once
e API inalterados. Removidas as chamadas a `asset_store_read/get_info`.

### 4. `components/assets/dialog_loader.c`
Mesma troca: `fopen` no `/sd/assets/0_17.bin`, le header+offsets+payload,
valida, aloca, monta os ponteiros de linha.

Pre-condicao dos loaders mudou: antes "asset_store_init rodou"; agora "sd_hal
montou /sd". Se o cartao faltar, `fopen` falha -> load retorna erro -> a tela
aborta o build (load_all_assets ja tratava isso). Sem crash.

`asset_store`/`storage_hal` continuam compilando (asset_store.h ainda da o
`asset_type_t`), mas nao sao mais chamados em runtime. Limpeza total da
dependencia fica pra depois.

## Antes (trecho asset_loader.c)
```c
ESP_RETURN_ON_ERROR(asset_store_read(type, id, 0, &hdr, sizeof(hdr)), ...);
...
asset_info_t info;
ESP_RETURN_ON_ERROR(asset_store_get_info(type, id, &info), ...);
```

## Depois (trecho)
```c
snprintf(path, sizeof(path), "/sd/assets/%d_%u.bin", (int)type, id);
FILE *f = fopen(path, "rb");
fread(&hdr, 1, sizeof(hdr), f);
... fseek/ftell pra conferir tamanho total ...
```

## Resultado

**Build VERDE** (exit 0). asset_loader.c + dialog_loader.c compilam limpos.
CyberGame.bin = 0xa2ec0 (~667 KB), 84%% livre.

**Gerador rodou**: 18 assets (2120 KB) gravados em `sdcard/assets/` como
`0_0.bin` .. `0_17.bin`. Precisou `pip install pillow` no python do sistema.

Pendente do usuario:
1. Copiar `sdcard/assets/` -> pasta `/assets` na raiz do cartao microSD.
2. Flash.
3. Validar: salas Recepcao/Empresa devem renderizar com imagem (lendo /sd).
   **Depende do problema de energia da tela (Fase 1) estar resolvido** — sem
   a tela acender, nao da pra ver o resultado.

## Links
- [[project_storage_nand_to_sd]], [[2026-05-28T0230-sd-card-detect-disable-nand]]
- [[build_environment]]
