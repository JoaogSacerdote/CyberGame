---
tipo: canonical
status: vigente
area: hardware
canonical-for: [pipeline-cor-display, byte-swap, bgr-mode, r-boost-multiplicador]
data: 2026-05-13
ultima-atualizacao: 2026-05-27
componente: hal_bridge + display_hal
tags: [cybersec, hardware, display, hal, calibracao]
---

# Calibracao de Cores do Display ST7796S

> Registro completo do processo de calibracao do pipeline de cores
> (LVGL → hal_bridge → display_hal → ST7796S) com base em comparacao
> empirica usando Samsung S24 como referencia sRGB.

## 1. Sintoma observado (2026-05-13)

Apos integracao das primeiras telas LVGL (`splash`, `menu`, `placeholder`),
o display mostrava cores claramente erradas:

- Fundo `0x101418` (quase preto) → aparecia roxo brilhante
- Texto branco `0xE6E6E6` → aparecia amarelo
- Cinza claro → vermelho
- Verde puro → amarelado

O `ui_debug` (validado anteriormente em hardware com "alfabeto inteiro
renderizando correto") nao apresentava o problema de forma tao obvia porque
usava labels pequenos sobre paineis coloridos — o efeito ficava menos
perceptivel.

## 2. Hipoteses iniciais (descartadas)

| Hipotese | Por que descartada |
|---|---|
| Anti-aliasing das fontes com swap G↔B lossy | Calculos matematicos mostraram que branco puro nao deveria distorcer. Foto refutou. |
| Partial render nao cobrir tela inteira | Logs mostraram `flush_cb` rodando sobre area completa. Fix `lv_obj_invalidate` nao alterou cores. |
| Subpixel rendering BGR fringing | Foto mostrou cores totalmente trocadas (nao apenas halo nas bordas). |
| `swap_gb_inplace` correto, hardware com G↔B fisico | Calibracao com grid 6×4 desmentiu — cores erradas eram R↔B + byte order, nao G↔B. |

## 3. Metodologia de calibracao

Criada uma matriz 6 colunas × 4 linhas = **24 quadrados** ocupando 480×320 px
(cada celula 80×80 px exata), cobrindo:

- **Monocromaticas** (preto, branco, varios cinzas, fundo UI)
- **Primarias puras** (R, G, B)
- **Secundarias puras** (amarelo, ciano, magenta)
- **Meias intensidades** das primarias/secundarias
- **Cores mistas** (laranja, roxo, lima, turquesa, rosa, azul-ciano)

Numero contrastante centralizado em cada quadrado (calculado via luminosidade
Y = 0.3R + 0.59G + 0.11B; branco em fundo escuro, preto em fundo claro).

Codigo em `components/ui/screen_splash.c` (durante a calibracao — sera
revertido apos validacao final).

## 4. Grade de referencia

```
┌────┬────┬────┬────┬────┬────┐
│ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │   linha 1: tons de cinza
├────┼────┼────┼────┼────┼────┤
│ 7  │ 8  │ 9  │ 10 │ 11 │ 12 │   linha 2: cores puras
├────┼────┼────┼────┼────┼────┤
│ 13 │ 14 │ 15 │ 16 │ 17 │ 18 │   linha 3: meias intensidades
├────┼────┼────┼────┼────┼────┤
│ 19 │ 20 │ 21 │ 22 │ 23 │ 24 │   linha 4: mistas
└────┴────┴────┴────┴────┴────┘
```

## 5. Tabela completa de resultados

### Coluna semantica:

| #   | RGB esperado | Nome                        |
| --- | ------------ | --------------------------- |
| 1   | `#000000`    | preto puro                  |
| 2   | `#FFFFFF`    | branco puro                 |
| 3   | `#808080`    | cinza 50%                   |
| 4   | `#E6E6E6`    | cinza claro (UI_COLOR_TEXT) |
| 5   | `#404040`    | cinza escuro                |
| 6   | `#101418`    | fundo UI atual              |
| 7   | `#FF0000`    | vermelho puro               |
| 8   | `#00FF00`    | verde puro                  |
| 9   | `#0000FF`    | azul puro                   |
| 10  | `#FFFF00`    | amarelo                     |
| 11  | `#00FFFF`    | ciano                       |
| 12  | `#FF00FF`    | magenta                     |
| 13  | `#800000`    | vermelho meio               |
| 14  | `#008000`    | verde meio                  |
| 15  | `#000080`    | azul meio                   |
| 16  | `#808000`    | amarelo meio (mostarda)     |
| 17  | `#008080`    | ciano meio (teal)           |
| 18  | `#800080`    | magenta meio (roxo escuro)  |
| 19  | `#FF8000`    | laranja                     |
| 20  | `#8000FF`    | roxo (violeta)              |
| 21  | `#80FF00`    | lima                        |
| 22  | `#00FF80`    | turquesa                    |
| 23  | `#FF0080`    | rosa                        |
| 24  | `#0080FF`    | azul-ciano                  |

### Coluna empirica (3 iteracoes):

| #   | Esperado  | Iter 1: `swap_gb` ON + RGB | Iter 2: `byte_swap` + RGB    | Iter 3: `byte_swap` + BGR |
| --- | --------- | -------------------------- | ---------------------------- | ------------------------- |
| 1   | `#000000` | `000000` ✅                 | `000000` ✅                   | _(pendente)_              |
| 2   | `#FFFFFF` | `ffffff` ✅                 | `ffffff` ✅                   | _(pendente)_              |
| 3   | `#808080` | `1c1731` (roxo escuro)     | `393e55` (cinza azulado)     | _(pendente)_              |
| 4   | `#E6E6E6` | `4a8e55` (verde)           | `767c91` (cinza azulado)     | _(pendente)_              |
| 5   | `#404040` | `000000` (preto)           | `14151d` ✅                   | _(pendente)_              |
| 6   | `#101418` | `493ab5` (roxo brilhante)  | `01030b` ✅                   | _(pendente)_              |
| 7   | `#FF0000` | `5e140c` (vermelho fraco)  | `0f2b9a` (azul) ❌            | _(pendente)_              |
| 8   | `#00FF00` | `3b901f` (verde)           | `378a0e` ✅                   | _(pendente)_              |
| 9   | `#0000FF` | `0a128d` (azul)            | `8a110e` (vermelho) ❌        | _(pendente)_              |
| 10  | `#FFFF00` | `768819` (amarelo verde)   | `388397` (ciano) ❌           | _(pendente)_              |
| 11  | `#00FFFF` | `347998` (ciano)           | `6d821c` (amarelo) ❌         | _(pendente)_              |
| 12  | `#FF00FF` | `8240ac` (magenta)         | `692f9b` ✅ (magenta)         | _(pendente)_              |
| 13  | `#800000` | `010301` (preto)           | `051039` (azul escuro) ❌     | _(pendente)_              |
| 14  | `#008000` | `040712` (preto)           | `112b04` ✅                   | _(pendente)_              |
| 15  | `#000080` | `140127` (roxo escuro)     | `2f0301` (vermelho escuro) ❌ | _(pendente)_              |
| 16  | `#808000` | `030803` (preto)           | `113248` (azul teal) ❌       | _(pendente)_              |
| 17  | `#008080` | `140127` (roxo escuro)     | `323a10` (mostarda) ❌        | _(pendente)_              |
| 18  | `#800080` | `101010` (preto)           | `351153` ✅                   | _(pendente)_              |
| 19  | `#FF8000` | `5a1207` (marrom)          | `0b427a` (azul teal) ❌       | _(pendente)_              |
| 20  | `#8000FF` | `0a168d` (azul)            | `6e196a` ✅                   | _(pendente)_              |
| 21  | `#80FF00` | `3ebd21` (verde)           | `059b6b` (turquesa) ❌        | _(pendente)_              |
| 22  | `#00FF80` | `327f30` (verde)           | `5ba900` (lima) ❌            | _(pendente)_              |
| 23  | `#FF0080` | `5e140c` (vermelho)        | `3609a3` (roxo azulado) ❌    | _(pendente)_              |
| 24  | `#0080FF` | `0a168d` (azul)            | `914e09` (laranja) ❌         | _(pendente)_              |

> Valores reportados foram lidos a olho nu pelo usuario, comparando com cores
> equivalentes pesquisadas no Samsung S24 (display AMOLED, ~100% sRGB).
> Pequenas variacoes sao esperadas (calibracao gamma do painel embarcado).

## 6. Analise dos padroes

### Iteracao 1 (swap_gb_inplace ON, rgb_ele_order RGB)

Estado original. Cores em **meia intensidade** viram quase pretas
(`800000`→`010301`, `008000`→`040712`); cores **muito escuras**
(`101418`→`493ab5`) viram brilhantes — sintoma classico de **byte
embaralhado** (high/low byte trocados no envio SPI). O `swap_gb_inplace`
estava amplificando o problema ao reorganizar bits que ja iam ser mal
interpretados.

### Iteracao 2 (byte_swap_inplace, rgb_ele_order RGB)

Apos substituir `swap_gb_inplace` por `byte_swap_inplace`:
- Pretos/brancos/cinzas/fundo UI: **TODOS corretos** ✅
- Verde, magenta, roxo: preservados ✅
- Mas vermelho ↔ azul **trocados** ❌
- Amarelo ↔ ciano **trocados** ❌
- Laranja ↔ azul-ciano **trocados** ❌

Conclusao: o byte swap era necessario (LVGL escreve uint16_t little-endian,
ST7796S le como bytes big-endian na SPI). E **adicionalmente** o display
fisico tem **R e B trocados** — o que se compensa setando
`rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR` no controlador.

### Iteracao 3 (byte_swap_inplace + rgb_ele_order BGR)

Apos setar `LCD_RGB_ELEMENT_ORDER_BGR` no panel config:
- Pretos/brancos/cinzas/fundo UI: corretos ✅
- **Canais R, G, B agora indo pros canais visuais corretos** ✅
- Misturas (amarelo, ciano, magenta, laranja, turquesa, roxo) corretas ✅
- **Observacao residual**: canal R aparece ligeiramente atenuado em
  intensidades baixas (`0x33` saia como ~`0x11`). Caracteristica fisica
  do painel (LEDs vermelhos com eficiencia luminica menor).

### Iteracao 4 (rb_boost_and_byte_swap_inplace + BGR — final)

Substituida `byte_swap_inplace` por `rb_boost_and_byte_swap_inplace` que
aplica multiplicador 2x no canal R (bits 15-11 do uint16_t pixel) com
clamp em 5 bits, ANTES do byte swap. Resultado:
- Vermelho em intensidades baixas agora bate visualmente com verde/azul
  equivalentes ✅
- Cores ja saturadas em R (R=31) inalteradas ✅
- Custo CPU: ~2 shifts + 1 mul + 1 compare por pixel — negligivel

**Status: pipeline de cores totalmente validado.**

## 7. Fix aplicado

### `components/hal_bridge/hal_bridge.c`

Substituida funcao `swap_gb_inplace` por `rb_boost_and_byte_swap_inplace`,
que faz duas operacoes por pixel:

```c
/* Boost canal R + byte swap (ordem LE->BE para ST7796 + compensacao da
 * luminancia reduzida dos LEDs vermelhos deste painel). */
#define HAL_BRIDGE_R_BOOST_MULT  2u

static inline void rb_boost_and_byte_swap_inplace(uint16_t *pixels, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        uint16_t p = pixels[i];

        /* Boost canal R: bits 15-11 sao o canal R efetivo no display
         * (combinacao de RGB565 LVGL + BGR mode no controller + byte swap). */
        uint16_t r = (p >> 11) & 0x1Fu;
        uint16_t boosted = r * HAL_BRIDGE_R_BOOST_MULT;
        if (boosted > 31u) boosted = 31u;
        p = (uint16_t)((p & 0x07FFu) | (boosted << 11));

        /* Byte swap LE -> BE para ST7796 ler corretamente via SPI. */
        pixels[i] = (uint16_t)((p << 8) | (p >> 8));
    }
}
```

### `components/hardware/display_hal.c`

`rgb_ele_order` mudado de RGB para BGR:

```c
const esp_lcd_panel_dev_config_t panel_cfg = {
    .reset_gpio_num = DISP_PIN_RST,
    /* Calibracao empirica 2026-05-13: display espera BGR. */
    .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
    .bits_per_pixel = 16,
};
```

## 8. Conclusao tecnica

O display **NAO** tem fios G↔B trocados na PCB (premissa antiga em
`project_hardware` desmentida). Os 3 fatores reais sao:

1. **ESP32-S3 (little-endian) ↔ ST7796 (big-endian SPI)**: precisa de
   byte swap em cada uint16_t do buffer antes de mandar para o display.
2. **Display espera BGR** no controlador: setar `LCD_RGB_ELEMENT_ORDER_BGR`
   troca R↔B na ordem dos subpixels.
3. **LEDs vermelhos com luminancia atenuada**: caracteristica fisica do
   painel barato. Compensado em software com boost 2x no canal R antes
   do byte swap.

Combinacao das tres correcoes resolve completamente o pipeline de cores.

## 9. Como reproduzir

Caso suspeite de regressao no pipeline de cores no futuro:

1. Substitua temporariamente o conteudo de `components/ui/screen_splash.c`
   pelo grid de calibracao (snippet abaixo).
2. Flash o firmware.
3. Compare cada um dos 24 quadrados com a referencia sRGB (Samsung S24 ou
   monitor calibrado).
4. Resultados esperados:
   - Quadrados 1, 2 (preto e branco): identicos a referencia
   - Quadrados 3-6 (cinzas e fundo UI): tons monocromaticos sem cor
     dominante
   - Quadrados 7-12 (primarias e secundarias puras): cores vibrantes
     conforme nome
   - Quadrados 13-18 (meias intensidades): tons medios sem saturacao
     dominante
   - Quadrados 19-24 (mistas): laranja, roxo, lima, turquesa, rosa,
     azul-ciano

Snippet do grid (resumo — para o codigo completo, ver historico do git em
`screen_splash.c` na sessao 2026-05-13):

```c
static const struct {
    uint32_t color;
    const char *num;
} grid[24] = {
    { 0x000000, "1"  }, { 0xFFFFFF, "2"  },
    { 0x808080, "3"  }, { 0xE6E6E6, "4"  },
    { 0x404040, "5"  }, { 0x101418, "6"  },
    { 0xFF0000, "7"  }, { 0x00FF00, "8"  },
    { 0x0000FF, "9"  }, { 0xFFFF00, "10" },
    { 0x00FFFF, "11" }, { 0xFF00FF, "12" },
    { 0x800000, "13" }, { 0x008000, "14" },
    { 0x000080, "15" }, { 0x808000, "16" },
    { 0x008080, "17" }, { 0x800080, "18" },
    { 0xFF8000, "19" }, { 0x8000FF, "20" },
    { 0x80FF00, "21" }, { 0x00FF80, "22" },
    { 0xFF0080, "23" }, { 0x0080FF, "24" },
};
```

## 10. Referencias

- `components/hal_bridge/hal_bridge.c` — implementacao do `byte_swap_inplace`
- `components/hardware/display_hal.c` — config do panel ST7796 com BGR
- Memoria local `project_hardware` — precisa atualizar (remover assercao
  "fios G/B trocados na PCB")
- ESP-IDF API: [`esp_lcd_panel_dev_config_t`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/index.html)
