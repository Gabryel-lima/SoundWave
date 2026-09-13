# SoundWave

**Laboratório computacional em C++20 para estudar como informação do espectro
sonoro pode ser transformada em representações no espectro eletromagnético.**

```
f_som  →  f_EM  →  λ  →  XYZ  →  sRGB
```

---

> ## O SoundWave não converte som em luz.
>
> Som é uma onda de **pressão** em um meio material. Luz é um campo
> **eletromagnético** que se propaga no vácuo. Não compartilham mecanismo,
> escala nem referência comum. **Não há nada em 440 Hz que aponte para um
> comprimento de onda.**
>
> `M(f_som) = f_EM` é uma função **inventada por você**. Trocá-la troca
> inteiramente o resultado, e nada no som mudou.
>
> Isso não torna o projeto inútil — torna-o um estudo de *representações*, e não
> de física. O que é legítimo, desde que dito. Todo o resto deste repositório
> existe para manter essa distinção visível:
>
> **fenômeno físico → dado medido → transformação matemática → representação visual**

---

## Começo rápido

```bash
cmake -S . -B build && cmake --build build -j
./build/soundwave_tests          # 185 testes, sem dependência nenhuma

./build/soundwave info           # a assimetria audível/visível
./build/soundwave map 440        # a cadeia completa para uma frequência
./build/soundwave analyze "gen:sweep:0:3" --out out/
./build/soundwave compare "gen:chord:220:3" --out out/compare
```

Nenhum arquivo de áudio é necessário: `gen:tipo:freq:duração` sintetiza sinais
determinísticos de teste.

```bash
./build/soundwave analyze minha-musica.wav --out out/
./build/soundwave analyze minha-musica.wav --config configs/octave.yaml --out out/
```

## O resultado central

```
$ ./build/soundwave map 440

mapeamento    f_EM          lambda      banda      sRGB          fora do gamut
------------------------------------------------------------------------------
linear        407.08 THz    736.5 nm    visible    (255,0,86)    sim
logarithmic   529.57 THz    566.1 nm    visible    (218,255,0)   sim
octave        399.72 THz    750.0 nm    visible    (255,0,85)    sim

A mesma nota uma oitava acima (880 Hz):
  logarithmic   (218,255,0) -> (0,255,146)   [cor diferente: equivalência perdida]
  octave        (255,0,85)  -> (255,0,85)    [mesma cor: equivalência preservada]
```

Três cores completamente diferentes para o **mesmo** som. Essa é a tese do
projeto, demonstrada em uma linha de terminal.

## As hipóteses nulas: e se você não inventar nada?

O projeto implementa os dois únicos mapeamentos que **não** exigem inventar uma
função — e ambos respondem a mesma coisa.

| Mapeamento | Base física | 440 Hz vira | Fidelidade |
|---|---|---|---|
| `identity` | `E = h·f` — preserva energia por quantum | 440 Hz, λ = 681 km, **rádio ELF** | **`Exact`** |
| `scale` | `λ = v/f`, forma fixada pela física | λ = 34,1 nm, **ultravioleta** | 1 parâmetro livre |

Em `scale`, a **forma** da função é física; só a constante `K` é escolhida. Toda a
arbitrariedade cabe em um número — epistemicamente melhor que `log`, onde a curva
inteira é invenção.

E o preço é mensurável: escala pura é uma isometria em espaço logarítmico, então a
janela visível captura exatamente sua própria largura. Ancorando 20 Hz em 750 nm,
**só 20–37,5 Hz cai no visível: 0,907 de 9,966 oitavas, 9,1% da banda audível.**
440 Hz vai para o ultravioleta, 20 kHz para os raios X.

> **Esse é o preço real da honestidade física. Esticar e espremer não é uma
> trapaça opcional — é a única forma de o audível caber no visível.**

## Os meios não resolvem — e isso é demonstrável

Trocar de meio multiplica todos os λ de uma faixa por um fator constante, e um
fator constante não altera a razão máx/mín. **O meio cancela.**

| Meio | v_som | λ@20 Hz | λ@20 kHz | **razão** |
|---|---|---|---|---|
| ar | 343 | 17,16 m | 1,72 cm | **1000,0** |
| água do mar | 1521 | 76,07 m | 7,61 cm | **1000,0** |
| gelo | 3840 | 192,00 m | 19,20 cm | **1000,0** |

A compressão de ~11× é **invariante sob qualquer mudança de meio**. Só a dispersão
a altera, e em ~1%. Ver [`docs/physics.md`](docs/physics.md).

### Onde o meio importa de verdade: os filtros antagônicos

Distância de meia potência em água do mar:

| SOM | | LUZ | |
|---|---|---|---|
| 20 Hz | **90.607 km** | 450 nm | **15,3 m** |
| 20 kHz | 1,2 km | 750 nm | **13,2 cm** |

> **A água é passa-baixa para som e passa-alta para luz.** O mesmo meio preserva
> os GRAVES do som e os AZUIS da luz — mas todo mapeamento monotônico leva grave
> em VERMELHO. O meio destrói exatamente o que o mapeamento preservou.
>
> "O mesmo ambiente para os dois" não produz equivalência. Produz **conflito**.

```bash
./build/soundwave medium sea-water
```

## O primeiro critério externo — e o resultado nulo que ele revelou

Todos os mapeamentos são escolhas, e até aqui nenhum tinha argumento a favor além
de "parece razoável". O projeto agora mede um critério externo:

```
J(M) = média sobre log f de  T_som(f) · T_luz(M(f))
```

*Se o som percorre um meio real e a luz correspondente percorre o mesmo meio, que
fração sobrevive aos dois trajetos?*

| Mapeamento | J | eficiência | no visível |
|---|---|---|---|
| `logarithmic` | 0,43715 | 52,6% | 100% |
| `octave` | 0,43726 | 52,6% | 100% |
| `linear` | 0,12019 | 14,5% | 100% |
| `scale` | 0,03975 | 4,8% | 9,2% |
| `identity` | 0,00000 | 0,0% | 0% |

O critério penaliza o `linear` em **3,6×** — ele empilha a banda no vermelho, que
é onde a água absorve.

### Inverter a orientação alinha os filtros? **Não.**

Os quatro candidatos empatam dentro de **0,04%**. E a razão é mais forte que "o
ganho é pequeno". Medindo onde cada filtro **discrimina**:

| Filtro | Janela de transição | Centro |
|---|---|---|
| **luz** | 27 cm – 1,7 m | 68 cm |
| **som** | 2,5 km – 15,9 km | 6,3 km |

**Separação: 4,0 décadas. Elas não se sobrepõem.**

> **Não existe distância em que os dois filtros discriminem simultaneamente.**
> Onde a luz distingue cores, o som é uniformemente transparente. Onde o som
> distingue graves de agudos, a luz já é zero em todo o visível.
>
> Alinhar os dois não é pouco útil — é **indefinido**. Os domínios não são só
> antagônicos em direção: são **disjuntos em escala**.

```bash
./build/soundwave align sea-water
```

## Até onde é confiável: classificação de fidelidade

Todo resultado vem com um orçamento que classifica **cada etapa**:

| Classe | Tem barra de erro? | Exemplo |
|---|---|---|
| `Exact` | sim, ~10⁻¹⁶ | `c`, `λ = c/f`, `E = h·f` |
| `Measured` | **sim** | frequência dominante (±0,12% a 440 Hz) |
| `ModelFit` | **sim** | Sellmeier, ISO 9613-1, Mackenzie |
| `Convention` | às vezes | sRGB, observador CIE 1931 |
| `Arbitrary` | **não** | o mapeamento f_som → f_EM |

> **Regra central: se qualquer etapa for `Arbitrary`, a incerteza combinada é
> INDEFINIDA.** Escrever `566,1 ± 0,3 nm` para um λ que só existe porque alguém
> escolheu uma função seria uma mentira com casas decimais.

```bash
./build/soundwave fidelity --config configs/scale-physical.yaml
```

Ver [`docs/fidelity.md`](docs/fidelity.md).

## A assimetria que governa tudo

|  | Faixa | Fator | Oitavas |
|--|-------|-------|---------|
| Audível | 20 Hz – 20 kHz | 1000× | **9,97** |
| Visível | 400 – 750 THz | 1,875× | **0,91** |

Compressão: **10,99×**. O visível cobre **menos de uma oitava**.

### Consequência medida

Uma oitava musical vira **+6,5%** em `f_EM` — quase imperceptível. A equivalência
de oitava, o fato perceptual mais sólido da percepção musical, **não sobrevive**
ao mapeamento logarítmico.

Dispersão de cor entre as nove oitavas de A (55 Hz – 14 kHz), medida por
`example_mapping_comparison`:

| Mapeamento | Dispersão | Preserva oitava | Monotônico | Invertível |
|------------|-----------|-----------------|------------|------------|
| logarítmico | 384,7 | ❌ | ✅ | ✅ |
| oitava | **0,0** | ✅ | ❌ | ❌ |
| linear | 303,2 | ❌ | ✅ | ✅ |

> A dispersão menor do linear **não** o torna melhor. Ela é menor porque o
> linear espreme quase tudo no extremo grave: a maioria das oitavas recebe
> praticamente a mesma cor, e só as duas mais agudas se espalham. A métrica
> sozinha engana — é preciso olhar `mapping_ruler.png` junto. Esse é um exemplo
> concreto do motivo pelo qual o projeto expõe três eixos de julgamento em vez
> de um número.

**Resposta à pergunta 2 do plano** — *"o mapeamento logarítmico preserva melhor
as relações musicais percebidas?"* — **não a mais forte delas.** Ele preserva a
ordem e as razões de altura; não preserva a equivalência de oitava. Com
compressão de 11×, preservar equivalência exige periodicidade, e periodicidade
exclui monotonicidade e injetividade. Não dá para ter os três.

## As quatro camadas

| # | Camada | Pergunta | Natureza |
|---|--------|----------|----------|
| 1 | **Análise** | O que existe no sinal? | Medição |
| 2 | **Transformação** | Como uma frequência sonora se associa a outra? | **Escolha** |
| 3 | **Conversão** | Como uma frequência EM visível vira cor? | Colorimetria + convenção |
| 4 | **Renderização** | Como os dados são mostrados? | Apresentação |

A camada 2 é a única que não pode estar "errada", porque não há nada contra o
que verificá-la. É por isso que ela é a mais perigosa.

Ver [`docs/architecture.md`](docs/architecture.md).

## Comandos

| Comando | O que faz |
|---------|-----------|
| `analyze <entrada>` | Pipeline completo → PNGs + CSV + manifesto |
| `compare <entrada>` | Mesmo sinal sob os três mapeamentos, lado a lado |
| `map <hz>` | A cadeia `f_som → f_EM → λ → banda → sRGB`, nos cinco mapeamentos |
| `medium [meio]` | Propriedades reais do meio, a invariância e os filtros antagônicos |
| `fidelity` | Orçamento de fidelidade: natureza e incerteza de cada etapa |
| `align [meio]` | Pontua os mapeamentos por sobrevivência conjunta no meio |
| `gen <tipo> <saída.wav>` | Sinais de teste determinísticos |
| `config [saída.yaml]` | Configuração padrão comentada |
| `info` | Constantes, domínios e a assimetria |
| `live <entrada>` | Tempo real (exige SDL2) |

### Saída de `analyze`

| Arquivo | Conteúdo |
|---------|----------|
| `spectrogram.png` | tempo × frequência; **matiz** = mapeamento, **brilho** = magnitude |
| `timeline.png` | cor representativa de cada quadro |
| `mapping_ruler.png` | a função de mapeamento, com marcas nas oitavas de A |
| `spectrum.png` | espectro de um quadro, barras coloridas |
| `frames.csv` | dado bruto por quadro, incluindo banda e procedência da cor |
| `manifest.yaml` | **todas** as escolhas que produziram o resultado |

No espectrograma, matiz e brilho carregam informações independentes de
propósito: o matiz é a transformação arbitrária em estudo, o brilho é o dado
medido. Misturá-los no mesmo canal visual tornaria impossível dizer qual dos
dois está variando.

## Dependências

**O núcleo e os testes não dependem de nada.** FFT, leitor/escritor WAV,
escritor PNG e harness de teste são próprios. Isso é decisão de arquitetura, não
conveniência: os testes são a única evidência de que a matemática está correta, e
precisam rodar em qualquer máquina sem instalar nada.

| Opcional | Detecção | Ganho | Sem ela |
|----------|----------|-------|---------|
| zlib | automática | PNG ~150× menor | PNG válido, porém grande |
| SDL2 | automática | comando `live` | `live` explica como instalar |
| FFTW3 | automática | backend de FFT alternativo | FFT própria |

```bash
# Debian/Ubuntu
sudo apt-get install zlib1g-dev libsdl2-dev libfftw3-dev
```

## Configuração e reprodutibilidade

Um resultado do SoundWave é uma imagem produzida por uma cadeia de escolhas
arbitrárias. **Sem registrar essas escolhas, a imagem não é um dado — é uma
ilustração.**

Toda execução grava um `manifest.yaml`, que é ele próprio uma configuração
válida:

```bash
soundwave analyze entrada.wav --config antigo/manifest.yaml --out novo/
diff antigo/frames.csv novo/frames.csv
```

O determinismo é verificado **bit a bit** nos testes, não "dentro de uma
tolerância". Ver [`docs/reproducibility.md`](docs/reproducibility.md).

Configurações prontas em [`configs/`](configs/): `default`, `octave`,
`weighted-harmonics`, `full-spectrum`, `linear-counterexample`,
`scale-physical`, `identity-physical`, `medium-filter`, `aligned`.

## Limitações que NÃO podem ser superadas

Estas não são pendências. São restrições estruturais; nenhuma versão futura vai
removê-las. Lista completa e justificada em [`docs/physics.md`](docs/physics.md) §6.

| # | Limitação | Por quê |
|---|---|---|
| 1 | Frequência não muda com o meio | Continuidade de fase na interface |
| 2 | A compressão de 11× é invariante | `v` e `n` cancelam — é álgebra |
| 3 | Nenhum meio real fecha a lacuna | Faltam 4–7 ordens de grandeza em `v_som` |
| 4 | Escala pura só cobre 9,1% do audível | Isometria em espaço log |
| 5 | Oitava e monotonicidade são incompatíveis | Periodicidade exclui injetividade |
| 6 | >90% das cores espectrais não cabem no sRGB | O locus é externo ao triângulo |
| 7 | Som não se combina como luz | Duas senoides dão dois picos, não um médio |
| 8 | Não existe "a cor de uma frequência sonora" | Domínios sem mecanismo comum |
| 9 | Os filtros do meio são disjuntos em escala | Janelas separadas por 4–5,4 décadas |

## Limitações do estado atual

- **Resolução nos graves.** Com `fft_size: 4096` a 44,1 kHz, `Δf ≈ 10,77 Hz`. A
  55 Hz um semitom vale ~3,3 Hz — o SoundWave **não** resolve notas graves
  individuais na configuração padrão.
- **Gamut.** Mais de 90% dos comprimentos de onda visíveis **não cabem** no
  sRGB. O que a tela mostra é sempre uma aproximação dessaturada.
  `wasOutOfGamut` é quase sempre `true` — esperado, não erro.
- **Pseudocor.** Fora do visível não existe cor. As cores dessas bandas são
  rótulos inventados, sempre marcados com `isFalseColour`.
- **Mistura ponderada.** A média de duas cores espectrais não é a cor de um som
  com essas duas frequências. É decisão de apresentação, não modelo de nada.
- **Normalização de luminância.** Escolha de visualização: descarta o brilho
  real para tornar as cores comparáveis.
- **Formatos comprimidos.** MP3/FLAC exigem FFmpeg e não são estritamente
  reproduzíveis entre versões. Para trabalho rigoroso, use WAV.
- **Fase.** É calculada e registrada, mas **não** usada no mapeamento. Usá-la
  exigiria justificar por que deveria significar algo visualmente.

## Documentação

| Documento | Assunto |
|-----------|---------|
| [`docs/architecture.md`](docs/architecture.md) | As quatro camadas e por que separá-las |
| [`docs/theory.md`](docs/theory.md) | FFT, janelas, resolução, interpolação de pico |
| [`docs/mapping.md`](docs/mapping.md) | A escolha arbitrária e o problema da oitava |
| [`docs/color.md`](docs/color.md) | CIE 1931, gamut, pseudocor |
| [`docs/physics.md`](docs/physics.md) | Meios, invariância, hipóteses nulas, limites intransponíveis |
| [`docs/fidelity.md`](docs/fidelity.md) | Classificação de fidelidade e orçamento de incerteza |
| [`docs/reproducibility.md`](docs/reproducibility.md) | Configuração, manifesto, determinismo |

## Estrutura

```
include/soundwave/        src/
├── core/    espectro, configuração, versão
├── audio/   PCM, WAV, geradores de sinal
├── dsp/     janelas, FFT, analisador
├── mapping/ FrequencyMapper e implementações    ← camada 2
├── physics/ meios reais, fidelidade, filtros    ← base física
├── color/   CIE 1931, sRGB, bandas EM           ← camada 3
├── render/  imagem, PNG, interpretação, gráficos
└── viz/     tempo real com SDL2 (opcional)

tests/      185 testes, framework próprio
examples/   pipeline mínimo, comparação de mapeamentos
configs/    configurações prontas e comentadas
docs/       teoria, mapeamento, cor, reprodutibilidade
```

## Perguntas que o projeto permite investigar

1. Qual função de mapeamento produz a representação mais estável?
2. O logarítmico preserva melhor relações musicais? → **respondida acima: não a
   equivalência de oitava**
3. Como harmônicos alteram a representação? → `configs/weighted-harmonics.yaml`
4. Qual estratégia de combinação é mais informativa? → `dominant` vs `weighted`
   vs `spectral`
5. É possível uma representação consistente entre instrumentos diferentes?
6. Como escalas musicais aparecem no espaço visual? →
   `example_mapping_comparison`
7. Dá para medir similaridade entre representações sonoras?
8. Como diferentes mapeamentos alteram essa similaridade? → `compare`, `align`

## Licença

MIT. Ver [`LICENSE`](LICENSE).
