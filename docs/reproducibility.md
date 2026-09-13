# Reprodutibilidade

Um resultado do SoundWave é uma imagem produzida por uma cadeia de escolhas
arbitrárias. **Sem registrar essas escolhas, a imagem não é um dado — é uma
ilustração.**

Toda execução de `soundwave analyze` grava um `manifest.yaml` ao lado das
imagens. Guarde-os juntos.

## O que o manifesto registra

```yaml
run:
  soundwave_version: 2.0.0
  algorithm_version: 1
  input_path: "exemplo.wav"
  input_fingerprint: fnv1a64:3f2a9c1d5e8b4712
  input_sample_rate: 44100
  input_samples: 132300
  mapping_description: "Logaritmico: x = (ln f - ln 20) / ... NAO preserva
                        equivalencia de oitava: uma oitava desloca f_EM em
                        apenas 6.511%. Borda: clamp."

analysis:   { ... }
mapping:    { ... }
color:      { ... }
interpretation: { ... }
render:     { ... }
```

## Duas versões, de propósito

| Campo | Incrementa quando |
|-------|-------------------|
| `soundwave_version` | qualquer release |
| `algorithm_version` | a **saída numérica muda** para a mesma entrada |

`algorithm_version` sobe com mudanças em: normalização de magnitude,
coeficientes das CMF, fórmula de mapeamento, tratamento de gamut. **Não** sobe
com mudanças de interface, documentação ou desempenho.

Sem essa separação, "mesmo arquivo + mesma configuração → mesmo resultado" seria
impossível de verificar entre versões diferentes do binário.

> A troca do ajuste analítico das CMF pela tabela CIE (documentada em
> `color.md`) é exatamente o tipo de mudança que exige incrementar
> `algorithm_version`.

## Impressão digital da entrada

FNV-1a de 64 bits sobre a representação **binária das amostras decodificadas**,
não sobre os bytes do arquivo.

Isso é deliberado: dois arquivos com metadados diferentes mas o mesmo áudio
produzem o mesmo resultado, logo devem produzir a mesma impressão. E usamos os
*bits* do `double`, não o valor impresso — a impressão perderia precisão e dois
sinais diferentes poderiam colidir por arredondamento.

Não é criptográfica. Serve para detectar "a entrada mudou", não para resistir a
adversário.

## Determinismo verificado

`tests/test_pipeline.cpp` verifica reprodutibilidade **bit a bit**, não "dentro
de uma tolerância":

- mesma entrada + mesma configuração → mesmos `Rgb`, mesma `f_EM`, mesma
  frequência dominante, comparados com `==`
- duas renderizações do mesmo espectrograma → buffers de pixel idênticos

Isso só é possível porque:

- os twiddles da FFT são pré-computados uma vez e reutilizados;
- o gerador de ruído usa splitmix64 com semente explícita (`std::mt19937` com
  `std::uniform_real_distribution` **não** é portável entre implementações da
  biblioteca padrão);
- o desempate na ordenação de picos é explícito, nunca dependente da ordem de
  inserção;
- o desenho de linhas usa Bresenham em inteiros, sem acúmulo de erro de ponto
  flutuante.

## Ida e volta da configuração

Gravar e reler a configuração **não pode alterar nenhum valor, nem no último
bit**. Por isso `configToYaml` imprime com `setprecision(17)` — precisão
suficiente para ida e volta exata em `double`.

Verificado em `config.yaml_faz_ida_e_volta_sem_perder_precisao`, que compara com
`==` e não com tolerância.

## Chaves desconhecidas são avisos, nunca silêncio

Um parâmetro digitado errado que fosse ignorado sem aviso faria o manifesto
registrar uma configuração **diferente da que rodou** — reprodutibilidade
aparente, e portanto pior que nenhuma.

```
$ soundwave analyze entrada.wav --config meu.yaml
aviso: chave desconhecida ignorada: analysis.fft_sizee
```

O mesmo vale para valores inválidos: o padrão é mantido, mas o aviso sai.

## Reproduzindo um resultado antigo

```bash
# o manifesto E uma configuracao valida
soundwave analyze entrada.wav --config resultado-antigo/manifest.yaml --out novo/

# compare
diff resultado-antigo/frames.csv novo/frames.csv
```

Se `algorithm_version` no manifesto for menor que a do binário atual, os
resultados **podem** diferir legitimamente — e é justamente para isso que o
campo existe.

## O que ainda NÃO é reproduzível

Honestamente: a decodificação via FFmpeg (formatos comprimidos). Versões
diferentes do FFmpeg podem decodificar o mesmo MP3 com diferenças de última
casa. Por isso o manifesto guarda a impressão digital das **amostras
decodificadas** — ela detecta a divergência, mesmo sem poder evitá-la.

Para trabalho que exija reprodutibilidade estrita, use WAV.
