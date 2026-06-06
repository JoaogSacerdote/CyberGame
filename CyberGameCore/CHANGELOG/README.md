# CHANGELOG — Sistema de Registro de Alterações

Sistema de rastreamento de mudanças no código do CyberGame. Criado em 2026-05-26 após perda de trabalho não commitado por `git reset --hard`. Objetivo: nada do que for editado, revertido ou deletado se perde — sempre dá pra comparar "antes vs depois" e entender por que uma alteração causou conflito.

## Estrutura

```
CHANGELOG/
├── README.md          ← este arquivo
├── INDEX.md           ← índice cronológico (uma linha por entrada)
└── entries/           ← uma entrada por mudança
    └── YYYY-MM-DDTHHMM-slug.md
```

## Regras de uso (para Claude)

1. **Antes** de qualquer edit/revert/delete em código do projeto que seja não-trivial, criar entrada em `entries/`.
2. Toda entrada precisa de **Antes:** com o conteúdo original (snippet ou arquivo inteiro). Sem isso, o registro é inútil.
3. Adicionar linha no `INDEX.md` (mais recente em cima).
4. Reverts e deletes são **obrigatórios** — nunca apagar algo sem registrar primeiro.

## Quando NÃO registrar

- Mudanças triviais (typo, formatação)
- Arquivos gerados automaticamente (build/, .vscode/, sdkconfig.old)
- Mudanças no próprio CHANGELOG

## Template de entrada

```markdown
---
date: YYYY-MM-DD HH:MM
type: edit | revert | delete | add
files:
  - path/to/file.c
session: descrição curta da sessão
---

# Título curto da mudança

## Motivo
Por que isso foi feito. Inclua sintoma original se for fix.

## Antes
\```linguagem
código original
\```

## Depois
\```linguagem
código novo
\```

## Resultado
O que aconteceu depois. Funcionou? Quebrou? Abriu novo problema?

## Links
- Memórias: [[nome-memoria]]
- Entradas relacionadas: [[YYYY-MM-DDTHHMM-slug]]
```

## Tipos

- `edit` — modificação de código existente
- `revert` — desfazer alteração anterior (obrigatório salvar o que foi descartado)
- `delete` — remoção de arquivo/função/bloco
- `add` — criação nova (rara — geralmente edit)
