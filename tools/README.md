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

Smoke-test do protocolo de recovery (Fase 3). Monta um frame binario `CMD_PING`,
envia, e valida o `RESP_PONG` de volta — confirma enquadramento, CRC32 e o
round-trip USB CDC. O contrato do protocolo esta em
`components/recovery/include/recovery_proto.h`.

```
python test_ping.py COM21
```

Saidas possiveis:

| Saida | Significado |
|---|---|
| `OK — framing, CRC e round-trip USB CDC validados.` | Protocolo funcionando ponta a ponta |
| `FAIL: timeout esperando SOF ...` | ESP nao respondeu (2s) — checar se esta em recovery e se e a porta CDC certa |
| `FAIL: CRC do frame nao bate ...` | Resposta corrompida na linha — ruido ou bug de enquadramento |
| `FAIL: versao do firmware (vX) != script` | Firmware e script com versoes de protocolo diferentes — recompilar/atualizar |
| `<- NACK (esp_err_t = N)` | ESP rejeitou o comando — ver codigo de erro |
| `FAIL: nao consegui abrir a porta: ...` | Porta errada / outro programa segurando — checar Device Manager |

### asset_uploader.py

Gerencia os assets na NAND do aparelho via USB CDC. Subcomandos:

```
python tools/asset_uploader.py <PORTA> ping                          # PING -> PONG
python tools/asset_uploader.py <PORTA> ls                            # lista assets
python tools/asset_uploader.py <PORTA> download sprite 0 piso.bin    # baixa asset cru
python tools/asset_uploader.py <PORTA> erase sprite                  # apaga categoria
python tools/asset_uploader.py <PORTA> reset                         # factory reset
python tools/asset_uploader.py <PORTA> selftest                      # validacao fisica
```

`<tipo>` = `sprite` | `font` | `sound`.

> O subcomando `upload` chega na etapa 4b (junto com o codec de imagem PNG -> blob).

### recovery_protocol.py

Modulo (nao roda direto) com a camada de transporte do protocolo de recovery —
framing, CRC32 e a classe `RecoveryClient`. Espelha
`components/recovery/include/recovery_proto.h`. Importado por `test_ping.py` e
`asset_uploader.py`; fonte unica da logica de protocolo no lado PC.

## Identificando a porta CDC

No Windows: abre Device Manager → Portas (COM e LPT). Quando o aparelho entra em recovery, aparece uma porta nova. Geralmente "USB Serial Device (COMxx)".

Existem **duas portas** quando o aparelho esta em recovery:
- **Porta JTAG/UART** (para `idf.py monitor`) — aparece sempre quando aparelho ligado
- **Porta CDC** (para esses scripts) — so aparece em modo recovery
