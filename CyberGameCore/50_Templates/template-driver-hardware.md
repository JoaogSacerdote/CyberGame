---
tags: [cybersec, firmware, hardware]
projeto: CyberSec
status: refatorando
data: AAAA-MM-DD
componente: NOME_DO_COMPONENTE
barramento: SPI | I2C | GPIO | ADC
---

# Driver: {{nome}}

## Resumo

Breve descricao da funcao do periferico no console.

## Pinagem

| Sinal | GPIO | Observacao |
|---|---|---|
| ... | ... | ... |

## Barramento e configuracao

- **Frequencia:**
- **Modo SPI / I2C addr:**
- **Compartilhamento:** (se houver bus partilhado)

## API publica

```c
// components/hardware/{{nome}}.h
esp_err_t {{nome}}_init(void);
```

## Riscos / Pontos de atencao

- ...

## Referencias

- `CONSULTA/Artigo.pdf` Secao 3.3 (Arquitetura de Hardware)
- Datasheet: ...
