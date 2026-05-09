# tools/

Utilitarios de PC para conversar com o CyberGame em modo recovery via USB CDC.

## Pre-requisitos

```
pip install pyserial
```

## Como entrar em modo recovery no aparelho

1. Aparelho desligado.
2. Segure **PWR + REC** simultaneamente por 2 segundos.
3. Solte. Aparelho boota e enumera **uma porta serial nova** no PC (alem da porta UART/JTAG existente). Tipicamente aparece como "USB Serial Device" no Device Manager.

## Scripts

### test_ping.py

Sanity check da conexao. Manda `PING\n`, espera `PONG\n`.

```
python test_ping.py COM21
```

Saidas possiveis:

| Saida | Significado |
|---|---|
| `OK — recebido: PONG` | USB CDC funcionando, comunicacao OK |
| `FAIL — esperado 'PONG', recebido: ''` | ESP nao respondeu (timeout 2s) — checar se aparelho esta em recovery |
| `FAIL — esperado 'PONG', recebido: 'UNKNOWN'` | ESP recebeu algo mas nao foi 'PING' — encoding ou ruido na linha |
| `FAIL: nao consegui abrir a porta: ...` | Porta errada / outro programa segurando — checar Device Manager |

## Identificando a porta CDC

No Windows: abre Device Manager → Portas (COM e LPT). Quando o aparelho entra em recovery, aparece uma porta nova. Geralmente "USB Serial Device (COMxx)".

Existem **duas portas** quando o aparelho esta em recovery:
- **Porta JTAG/UART** (para `idf.py monitor`) — aparece sempre quando aparelho ligado
- **Porta CDC** (para esses scripts) — so aparece em modo recovery
