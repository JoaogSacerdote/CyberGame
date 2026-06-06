---
tags: [cybersec, skills, indice, meta]
projeto: CyberSec
status: validado
data: 2026-05-08
---

# Index de Skills — CyberSec / Project Blacksmith

Registro central de todas as automacoes (/comandos) personalizadas criadas para o projeto. **Atualizacao automatica:** este arquivo e atualizado sempre que uma nova skill e registrada (regra de ouro definida em `/ajuda`).

> [!info] Como buscar
> Use `/ajuda <descricao>` para Claude localizar a skill correta com base em linguagem natural.
> Use `/ajuda listar tudo` para um resumo de todas as skills.

---

## /ajuda

**Sintaxe:** `/ajuda <descricao em linguagem natural>`

**Faz:** Meta-skill de descoberta. Le este `index-de-skills.md`, casa a descricao do usuario com as skills registradas, e responde com o comando que resolve o problema (sintaxe + breve explicacao). Tambem atualiza este index sempre que uma nova skill e criada.

**Quando usar:**
- Quando voce esqueceu o nome exato de um comando
- Quando quer descobrir se ja existe automacao para um problema antes de pedir uma nova
- Para listar todas as skills (`/ajuda listar tudo`)
- Quando quer entender os parametros aceitos por uma skill especifica

**Exemplos:**
- `/ajuda como atualizo manuais novos da pasta CONSULTA?` → indica `/sync-consulta`
- `/ajuda listar tudo` → resumo de todas as entradas deste arquivo

**Local:** `~/.claude/skills/ajuda/SKILL.md`

---

## /sync-consulta

**Sintaxe:** `/sync-consulta` (sem parametros)

**Faz:** Le `CyberGameCore/CONSULTA/`, detecta arquivos novos/modificados via mtime, aplica **regra de sobreposicao de memoria** (info nova substitui antiga em memorias e notas — nao convive com a antiga). Atualiza snapshot em `CyberGameCore/.sync-state/last-sync.json` e gera log em `CyberGameCore/00_Inbox/log-sincronizacao-consulta.md`.

**Quando usar:**
- Apos adicionar manuais novos em `CONSULTA/`
- Apos atualizar uma versao do `Artigo.pdf`
- Apos receber correcoes de pinagem ou briefing pedagogico
- Antes de comecar uma sessao longa de implementacao (garantia de contexto fresco)
- Quando suspeitar que regras ou definicoes ficaram desatualizadas em relacao aos manuais

**Privacidade:** nada lido por esta skill pode ir para Git. `CyberGameCore/` ja esta no `.gitignore`.

**Local:** `~/.claude/skills/sync-consulta/SKILL.md`

---

> [!todo] Espaco reservado para futuras skills
> Novas entradas devem ser adicionadas acima desta linha, em ordem de criacao mais recente para a mais antiga (`/ajuda` e a primeira do projeto e fica no topo).
