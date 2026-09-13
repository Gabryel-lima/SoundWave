# Camada 1 — Análise: fundamentos matemáticos

Tudo aqui é **medição**. Erros nesta camada produzem números errados, e números
errados são verificáveis contra sinais sintéticos de resposta conhecida — que é
exatamente o que `tests/test_analyzer.cpp` faz.

## 1. Transformada discreta de Fourier

$$X[k] = \sum_{n=0}^{N-1} x[n] \cdot e^{-i 2\pi k n / N}$$

Implementação: Cooley–Tukey radix-2, iterativa, decimação no tempo
(`src/dsp/fft.cpp`). Exige `N` potência de dois.

**Frequência do bin:** `f_k = k · f_s / N`
**Resolução:** `Δf = f_s / N`

Com `f_s = 44100 Hz` e `N = 4096`, `Δf ≈ 10,77 Hz`. Duas senoides separadas por
menos que isso **não são resolvíveis**, e nenhuma interpolação muda esse fato:
interpolação refina a estimativa de um pico isolado, não separa dois picos.

### O compromisso que não tem saída

| N | Δf a 44,1 kHz | Duração do bloco |
|---|---------------|------------------|
| 1024 | 43,07 Hz | 23 ms |
| 2048 | 21,53 Hz | 46 ms |
| 4096 | 10,77 Hz | 93 ms |
| 8192 | 5,38 Hz | 186 ms |

Melhor resolução em frequência custa pior resolução no tempo. É o princípio da
incerteza aplicado à análise de sinais, não uma limitação da implementação.
Para música, `4096` costuma ser o equilíbrio: resolve um semitom nos médios
(a 440 Hz um semitom vale ~26 Hz) sem borrar demais os transientes.

Nos graves isso **falha**: a 55 Hz, um semitom vale ~3,3 Hz, bem abaixo de
Δf = 10,77. O SoundWave não resolve notas graves individuais com a configuração
padrão, e isso é uma limitação real, não um detalhe.

## 2. Janelamento

A FFT assume que o bloco se repete periodicamente. Se as bordas não casam, a
descontinuidade artificial espalha energia por todos os bins — **vazamento
espectral**.

Usamos a forma **periódica** (`w[n]` em função de `n/N`), não a simétrica
(`n/(N-1)`). A simétrica introduz um viés de meia amostra que desloca a fase
estimada. É um erro comum e silencioso.

| Janela | Ganho coerente | ENBW (bins) | Uso |
|--------|----------------|-------------|-----|
| Retangular | 1,0 | 1,0 | melhor resolução, pior vazamento |
| Hann | 0,5 | 1,50 | **padrão** — bom equilíbrio |
| Hamming | 0,54 | 1,36 | primeiro lobo lateral menor |
| Blackman | 0,42 | 1,73 | vazamento baixo |
| Blackman–Harris | 0,35875 | 2,00 | vazamento mínimo (−92 dB) |

O ganho coerente é calculado **a partir dos coeficientes reais**, não tabelado
(`coherentGain()`), para que uma janela nova entre sem precisar de constantes
novas.

## 3. Magnitude e normalização de amplitude

$$A_k = \frac{2 \cdot |X[k]|}{\sum_n w[n]}$$

O fator 2 junta as frequências `+f` e `−f`, cuja energia se divide igualmente
para um sinal real. **DC e Nyquist não levam o fator 2**, porque não têm par
espelhado — esquecer disso dobra esses dois bins.

Com essa normalização, uma senoide de amplitude `A` produz um pico de magnitude
`A`. Verificado em `analyzer.magnitude_recupera_a_amplitude_da_senoide`.

Para exibição: `D_k = 20·log₁₀(A_k + ε)`, com `ε = 1e-12`.

> **Não confunda** magnitude normalizada com energia física absoluta. Ela depende
> da escala do arquivo, da janela e da normalização — serve para comparação
> relativa, nunca para medida de nível sonoro.

## 4. Interpolação parabólica de pico

O critério do plano era `|f_detectada − f_esperada| ≤ Δf`. Esse critério é
**fraco**: com Δf = 10,77 Hz ele aceita qualquer valor entre 429 e 451 Hz para
um tom de 440 Hz.

Em escala logarítmica, o lobo principal de uma janela Hann é quase exatamente
uma parábola. Ajustando uma parábola aos três bins em torno do máximo:

```
δ = ½ · (α − γ) / (α − 2β + γ)        α, β, γ em dB
f = (k + δ) · Δf
```

Isso reduz o erro para **menos de 5% de um bin** (~0,5 Hz), e é *esse* limite
mais apertado que os testes verificam. Um critério frouxo deixa regressões de
precisão passarem despercebidas.

`δ` fora de `[−0,5; +0,5]` é descartado: um máximo verdadeiro fica a menos de
meio bin do maior bin, e valores fora disso são ruído.

## 5. Remoção de contínua

Um offset de DC aparece como um pico enorme no bin 0, vaza para os vizinhos e
pode **dominar a detecção de frequência dominante**. A média do bloco é
subtraída antes do janelamento.

**Atenção a uma expectativa errada:** isso *não* zera o bin DC. A média é
removida antes da janela, e a janela pondera as amostras desigualmente, então o
bloco janelado volta a ter média levemente não nula. A garantia real é uma
redução de ~3 ordens de grandeza. Um teste que exigisse zero exato estaria
fisicamente errado — e este projeto teve exatamente esse teste antes de ser
corrigido.

## 6. Fase

`phaseRad = arg(X[k])`, em `(−π, π]`.

A fase é confiável no bin de um pico bem resolvido e essencialmente aleatória em
bins dominados por vazamento ou ruído. O SoundWave a registra mas **não a usa**
no mapeamento de cor. Usar fase exigiria justificar por que ela deveria
significar algo visualmente — e não há justificativa boa.

## 7. Determinismo

Os twiddles são pré-computados uma vez na construção e reutilizados. Isso não é
só desempenho: garante que duas execuções produzam resultados **bit a bit
idênticos**, sem o que a promessa de reprodutibilidade não pode ser verificada.
O gerador de ruído usa splitmix64 com semente explícita pelo mesmo motivo —
`std::mt19937` com `std::uniform_real_distribution` não é portável entre
implementações da biblioteca padrão.

## 8. Sinais de validação

| Sinal | O que valida |
|-------|--------------|
| Senoide 440/1000/2000 Hz | posição e amplitude do pico |
| Impulso | espectro plano (índices da FFT corretos) |
| Contínua | pico só em DC |
| Acorde | resolução de múltiplos picos |
| Série harmônica | fundamental dominante, harmônicos presentes |
| Varredura log | evolução temporal, continuidade de fase |
| Ruído branco | energia distribuída |
| Silêncio | magnitude exatamente zero |
