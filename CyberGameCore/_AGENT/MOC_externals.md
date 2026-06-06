---
tipo: moc
status: vigente
area: meta
ultima-atualizacao: 2026-05-27
---

# MOC — Externals (CONSULTA/)

> Mapa de fontes externas. Quando precisar fundamentar decisão técnica
> ou pedagógica, comece aqui.

## Hierarquia de fontes em CONSULTA/

```
CONSULTA/
├── PDFs (literatura)
│   ├── Artigo.pdf                     ⭐ fonte pedagógica primária
│   ├── pdfcoffee...Reusable Firmware.pdf  Beningo (HAL/drivers)
│   ├── Elecia White...Embedded.pdf    arquitetura embedded geral
│   ├── incb_esp32_idf.pdf             16 pp (só sumário extraível)
│   └── MSP4030_..._Specification.pdf  datasheet do módulo display
├── extratos texto
│   ├── Artigo_extracted.txt           texto completo do Artigo (56 KB)
│   ├── msp_out.txt                    extração do datasheet display
│   └── (em .cache/pdf-text/ no projeto raiz: extratos dos livros)
├── decisões / dúvidas do usuário (raw, datadas)
│   ├── RESPOSTAS.txt                  ⭐ decisões fechadas de design (25 KB)
│   ├── A resolver.txt                 ⭐ dúvidas em aberto (13 KB)
│   └── notas_temp.txt                 efêmero — apagar após assimilar
├── notas consolidadas em md
│   ├── consulta_esquema_eletrico.md   esquemas elétricos
│   └── lista-de-imagens-c2-personagem-sala2.md  assets pendentes
├── código de referência (HAL antiga)
│   ├── pmu.{c,h}, display_hal.{c,h}, button_hal.{c,h}, joystick_hal.{c,h}
│   └── (pinout antigo — NÃO portar diretamente)
├── JC4880P443C_I_W/                   docs placa P4 (migração futura)
└── IGNORAR/                           assets brutos — ignorar
```

## Como usar cada categoria

### `Artigo.pdf` (fonte pedagógica primária)
- **Imutável**. Nunca editar.
- Sempre citar seção (`Artigo.pdf §3.2.1`).
- Texto pesquisável em `Artigo_extracted.txt`.
- Cobre: conceitos de ciberseg (Stallings, Kim&Solomon), arquitetura
  proposta de hardware/software (3.3, 3.4), cartas NFC (3.2.1).

### Livros (Beningo / Elecia / INCB)
- Estudados em 2026-05-27 (auditoria de boas práticas).
- Extratos texto em `.cache/pdf-text/` (raiz do projeto, fora do vault).
- Síntese em memória `[[esp_idf_style_guide]]` + nas seções "Padrões
  de design" de [[MOC_firmware]].

### `RESPOSTAS.txt` (decisões de design)
- Resposta do usuário a perguntas estruturadas em blocos.
- Decisões importantes consolidadas em [[DECISION_LOG]] e
  [[matriz-reacao-ataques]] / [[diagramas-do-projeto]].
- Quando consultar diretamente: para nuance que a síntese não capturou.

### `A resolver.txt` (dúvidas em aberto)
- Itens marcados `[ ]` aguardam decisão de equipe.
- Sintetizado em [[OPEN_QUESTIONS]] (versão mais navegável).
- Quando atualizar: prefira atualizar `OPEN_QUESTIONS.md` E marcar item
  como resolvido em `A resolver.txt`.

### `consulta_esquema_eletrico.md`
- Esquemas detalhados. Útil quando bring-up + multímetro.

### `IGNORAR/`
- Assets visuais brutos (sprites, fontes, fx packs).
- Não relevante para agente. Ignorar literalmente.

### `JC4880P443C_I_W/`
- Documentação completa da placa ESP32-P4 candidata para v2.
- Só consultar se [[futuro-migrar-p4-JC4880P443]] for ativado.

### Código legado em CONSULTA/
- Cópia de HALs anteriores com pinout antigo.
- **NÃO portar diretamente** — usar como referência arquitetural.
- Padrões úteis já extraídos em [[auditoria-hal-legada]] (historical).

## URLs externas relevantes

- ESP-IDF style guide (stable): https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/style-guide.html
- ESP-IDF API reference: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/index.html
- LVGL 9.x docs: https://docs.lvgl.io/master/

## Skill /sync-consulta

Quando novos arquivos chegarem em CONSULTA/:
- Comando `/sync-consulta` detecta deltas, aplica sobreposição de memória.
- Log em `00_Inbox/log-sincronizacao-consulta.md`.
- Definição em [[index-de-skills]].
