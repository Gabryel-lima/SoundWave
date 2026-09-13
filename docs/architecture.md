# Arquitetura — as quatro camadas

O erro conceitual mais fácil de cometer neste projeto é tratar
`som → cor` como uma operação única. Ela não é. São quatro operações de
naturezas epistemológicas **diferentes**, e a separação entre elas é a decisão
de arquitetura central do SoundWave.

| # | Camada | Pergunta | Natureza | O que acontece se errar |
|---|--------|----------|----------|-------------------------|
| 1 | **Análise** | O que existe no sinal? | Medição | Números errados |
| 2 | **Transformação** | Como uma frequência sonora se associa a outra? | **Escolha** | Nada errado — só *diferente* |
| 3 | **Conversão** | Como uma frequência EM visível vira cor? | Medição (colorimetria) + convenção (sRGB) | Cor errada |
| 4 | **Renderização** | Como os dados são mostrados? | Apresentação | Leitura enganosa |

A camada 2 é a única que não pode estar "errada", porque não há nada contra o
que verificá-la. É por isso que ela é a mais perigosa: um resultado bonito
parece uma descoberta quando é apenas a consequência da função escolhida.

```
                         ┌─── CAMADA 1: ANÁLISE (medida) ────┐
  WAV / gerador  ──▶  PCM mono f64  ──▶  janela  ──▶  FFT  ──▶  Spectrum
                                                                   │
                         ┌─── CAMADA 2: TRANSFORMAÇÃO (escolha) ───┘
                         ▼
                    FrequencyMapper::map(f_som) ──▶ f_EM
                         │
                         ├──────────────┬──────────────────┐
                         ▼              ▼                  ▼
                    dentro do       fora do          descartado
                    visível         visível           (NaN)
                         │              │                  │
    ┌─── CAMADA 3: CONVERSÃO ───┘       │                  │
    ▼                                   ▼                  ▼
  λ = c/f  ─▶ CIE 1931 ─▶ XYZ      pseudocor            preto
             (medida)      │       (rótulo inventado)
                           ▼
                  sRGB + gamut (convenção)
                           │
    ┌─── CAMADA 4: RENDERIZAÇÃO ────────┘
    ▼
  espectrograma · linha do tempo · régua de mapeamento · CSV · manifesto
```

## Onde cada camada vive

| Camada | Cabeçalhos | Depende de |
|--------|-----------|------------|
| 1 | `audio/`, `dsp/`, `core/spectrum.hpp` | nada |
| 2 | `mapping/` | `core/constants.hpp` |
| 3 | `color/` | camada 2 apenas por um `double` |
| 4 | `render/`, `viz/` | camadas 1–3 |

As camadas conversam por **tipos de dados simples**, não por objetos
compartilhados. A camada 2 recebe um `double` e devolve um `double`; a camada 3
recebe um `double` e devolve um `ColorResult`. Isso é o que torna possível rodar
o mesmo `Spectrum` por quatro mapeadores diferentes sem recalcular nada — o
experimento central que o plano propõe.

## A procedência viaja junto com o dado

`ColorResult` carrega `isFalseColour` e `wasOutOfGamut` não por conveniência,
mas porque **a interface não pode esquecer de verificar**. Uma cor produzida
pelas funções de correspondência CIE e uma pseudocor de infravermelho são coisas
categoricamente diferentes, e se o tipo não as distinguisse, a camada 4 acabaria
desenhando as duas do mesmo jeito — apagando justamente a distinção que o
projeto existe para preservar.

## O que NÃO está separado, e por quê

`Interpreter` (em `render/`) junta as camadas 2 e 3 e decide como colapsar
milhares de bins em uma representação. Poderia ser uma quinta camada. Não é,
porque a decisão "dominante vs. ponderado vs. espectral" é uma escolha de
apresentação, não uma transformação de domínio — e inflar a contagem de camadas
não deixaria isso mais claro.
