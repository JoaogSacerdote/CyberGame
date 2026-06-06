---
tipo: agent-reference
status: vigente
area: meta
ultima-atualizacao: 2026-05-27
---

# CyberGameCore — vault operacional

> Este vault é **memória operacional do agente Claude Code** para o projeto
> CyberGame. Não é caderno humano. Não é segundo cérebro do usuário.
> Está otimizado para uso por IA, não para leitura manual.

## Para humanos

Se você é o usuário lendo isto, provavelmente quer:
- ver decisões fechadas → [[_AGENT/DECISION_LOG]]
- ver o que está em aberto → [[_AGENT/OPEN_QUESTIONS]]
- ver mudanças recentes no código → `CHANGELOG/INDEX.md`

## Para o agente (Claude Code)

Comece SEMPRE por [[_AGENT/ENTRYPOINT]].

Lá está a hierarquia de fontes, vocabulário, MOCs por área, regras de
manutenção, e o canonical index.

## Estrutura física

```
_AGENT/        — meta (entrypoint, vocab, MOCs, canonical index, regras)
00_Inbox/      — scratch ativo, log de sync, roadmap futuro
10_Arquitetura/ — canonical de FSM, matriz ataques, plano MVP
20_Hardware_HAL/ — canonical de hardware/drivers
50_Templates/  — templates para criar notas novas
90_Historico/  — notas obsoletas (preservadas para arqueologia)
CHANGELOG/     — timeline canônica de mudanças em código
CONSULTA/      — fontes externas (PDFs, raw text, decisões/dúvidas)
```

## Confidencialidade

`CyberGameCore/` está no `.gitignore`. Nada aqui vai para o GitHub.
Inclui artigo científico, manuais, notas internas. Backup é
responsabilidade do usuário (Obsidian Sync, cópia local).
