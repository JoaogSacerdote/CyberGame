---
data: 2026-05-28T08:20
tipo: add (codigo + tooling)
escopo: components/entity, tools, assets/layout
trigger: usuario definiu o modelo de autoria de salas em DUAS fontes-texto (catalogo + posicoes), abandonando JSON generico como formato principal
commits:
  - (pendente)
---

# Pipeline de layout de salas: ENTIDADES (catalogo) + POSICOES_<sala>

## Por que

Para data-drivar o posicionamento de entidades (player, NPCs, moveis) com o
sistema novo (entity_t: pivot base-centro, colisao, Y-sort). Decisao do usuario:
autoria em texto humano, em duas camadas — catalogo GLOBAL reutilizavel +
instancias por sala — combinadas na geracao. **Nada de JSON generico como
autoria.**

## O que mudou (tudo aditivo — telas vivas NAO foram tocadas)

- `assets/layout/ENTIDADES.txt` — catalogo: 1 bloco por TIPO (nome, imagem,
  tipo, colisao_base, flags, sort_offset_y). Sem posicao.
- `assets/layout/POSICOES_<sala>.txt` — instancias: cabecalho SALA/FUNDO +
  blocos POSICAO (entidade=ref do catalogo, nome unico, x/y ou from_image,
  overrides opcionais).
- `tools/gen_room_layout.py` (reescrito) — le os dois, combina, valida contra
  o asset_registry, calcula offsets de colisao (offset_x=-(w//2), offset_y=-h),
  aplica defaults (sort_offset_y=0, from_image=false), avisa em dado faltando,
  e gera `components/entity/room_layout_gen.c`.
- `components/entity/include/room_layout.h` — struct `room_entity_def_t` +
  `room_layout_t` + `room_layouts[]` + `room_layout_find()`. Dados leves (ids
  numericos, sem ponteiros) — nao pesam na flash.
- `components/entity/room_layout_gen.c` (gerado) — seed atual: empresa +
  recepcao com player + NPC. **Compila no firmware (build verde).**
- `entity/CMakeLists.txt`: +room_layout_gen.c.
- Removidos: `assets/room_layout.json` + `assets/room_layout.example.json`
  (JSON de autoria, obsoletos pela decisao acima). Estavam so nesta sessao.

## Regras implementadas (resumo)
Coordenadas 480x320, (0,0) topo-esq; x,y = pivot base-centro (pes); asset deve
existir no registry; type em {player,npc,furniture,prop,trigger}; flags
mapeadas p/ ENTITY_FLAG_*; VISIBLE auto pra quem tem sprite; nomes unicos por
sala (aviso se repetir); POSICAO sobrescreve flags/colisao/sort do catalogo.

## Resultado / pendente

Build VERDE — a tabela de salas compila. **Ainda NAO ha loader em runtime**:
nada le `room_layouts[]` e instancia no entity_pool ainda. Proximo passo
([[task pendente]]): funcao na UI que, ao montar a sala, le o layout, aloca
entities, carrega assets (tolerante a falha sem PSRAM), cria lv_obj e marca
Y-sort. Wire nas telas recepcao/empresa. A fazer com os dados reais do usuario
+ validacao no aparelho.

NOTA: o firmware atual ainda esta com a tentativa PSRAM Octal@40 + memtest off
(aguardando boot-log do usuario). O pipeline de layout e independente disso.

## Links
- [[2026-05-28T0540-fase2-assets-na-sd]], [[project_storage_nand_to_sd]]
- docs/ai-rules/07-entity-system.md (sistema de entidades)
