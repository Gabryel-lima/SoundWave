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
./build/soundwave_tests          # 139 testes, sem dependência nenhuma

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
| `map <hz>` | A cadeia `f_som → f_EM → λ → banda → sRGB` |
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
`weighted-harmonics`, `full-spectrum`, `linear-counterexample`.

## Limitações declaradas

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
| [`docs/reproducibility.md`](docs/reproducibility.md) | Configuração, manifesto, determinismo |

## Estrutura

```
include/soundwave/        src/
├── core/    espectro, configuração, versão
├── audio/   PCM, WAV, geradores de sinal
├── dsp/     janelas, FFT, analisador
├── mapping/ FrequencyMapper e implementações    ← camada 2
├── color/   CIE 1931, sRGB, bandas EM           ← camada 3
├── render/  imagem, PNG, interpretação, gráficos
└── viz/     tempo real com SDL2 (opcional)

tests/      139 testes, framework próprio
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
8. Como diferentes mapeamentos alteram essa similaridade? → `compare`

## Licença

MIT. Ver [`LICENSE`](LICENSE).
