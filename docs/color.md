# Camada 3 — Conversão: onde a física acaba

Esta camada tem três passos de naturezas diferentes, e confundi-los é fácil:

| Passo | Natureza | Confiabilidade |
|-------|----------|----------------|
| `λ = c / f` | **Física** | Exata no vácuo |
| `λ → XYZ` | **Colorimetria** — média empírica de observadores humanos | Medida, com variação individual |
| `XYZ → sRGB` | **Convenção de engenharia** — define um monitor ideal | Arbitrária mas padronizada |

Só o primeiro é física. `c = 299 792 458 m/s`, exata por definição do SI.

## Funções de correspondência de cor CIE 1931

Usamos a **tabela oficial** do observador padrão de 2°, 380–780 nm em passos de
5 nm, com interpolação linear (`color/cie_tables.hpp`).

### Por que não um ajuste analítico — um erro real deste projeto

A primeira versão usava o ajuste multi-lobo de gaussianas de Wyman, Sloan &
Shirley (JCGT 2013), justificado por ter **"erro abaixo de ~1% do pico"**.

Esse raciocínio estava errado, e vale entender por quê.

O erro de 1% é **absoluto**. Mas cor é determinada pelas **razões** entre
`x̄`, `ȳ` e `z̄` — e acima de ~700 nm as três funções valem menos de `1e-3`.
Um erro absoluto minúsculo sobre valores minúsculos é um erro **relativo**
gigante.

Na prática, as caudas gaussianas de `x̄` e `ȳ` decaem em ritmos diferentes e se
**cruzam** perto de 712 nm:

| λ | `x̄/ȳ` real | `x̄/ȳ` do ajuste |
|---|-------------|------------------|
| 650 nm | 2,65 | 2,58 ✓ |
| 700 nm | 2,77 | 1,32 |
| 720 nm | 2,77 | 0,74 |
| 750 nm | 2,77 | **0,23** |

A razão real é ≈ 2,77 e praticamente **constante** em todo o vermelho profundo —
é ela que mantém o vermelho vermelho. Com o ajuste, 736 nm era renderizado como
**verde**.

A normalização de luminância agrava em vez de esconder: ela multiplica o
triestímulo por `1/Y`, que nas bordas do visível vale milhares.

E os testes da época **passavam**, porque verificavam os *picos* das funções e
cores no *centro* do visível. A suíte `tests/test_cie.cpp` existe para cobrir o
que faltava: razões, bordas, e a progressão de matiz ao longo de **todo** o
visível.

> **Lição que vale além deste arquivo:** "erro pequeno" só significa alguma
> coisa em relação à grandeza que se vai usar. Aqui o que importa é a razão, não
> o valor — então a tolerância tinha de ser relativa desde o início.

O ajuste continua no código (`cieXAnalyticFit` etc.), **não** como alternativa
utilizável, mas como contra-exemplo executável, para que ninguém repita a troca
"por ser mais barato".

### Verificação

A tabela embutida é conferida contra três âncoras independentes e amplamente
publicadas:

- `ȳ(555) = 1,00000` — pico da eficiência luminosa fotópica
- `x̄(600) = 1,06220`
- `z̄(445) = 1,78260`

## XYZ → sRGB

Primárias sRGB com ponto branco D65:

```
R =  3,2404542·X − 1,5371385·Y − 0,4985314·Z
G = −0,9692660·X + 1,8760108·Y + 0,0415560·Z
B =  0,0556434·X − 0,2040259·Y + 1,0572252·Z
```

Teste-âncora: o ponto branco D65 tem de virar exatamente `(1, 1, 1)`.

Função de transferência (a "gamma", que na verdade é uma curva composta):

```
c ≤ 0,0031308  →  12,92·c
c >  0,0031308  →  1,055·c^(1/2,4) − 0,055
```

## Gamut: a limitação que não tem solução

O sRGB é um **triângulo** no diagrama de cromaticidade. Toda a curva espectral
(o *locus* monocromático) fica **fora** dele, exceto onde toca as três
primárias.

Medido pelo projeto: **mais de 90%** dos comprimentos de onda entre 400 e 750 nm
produzem pelo menos um componente sRGB negativo — uma quantidade de luz
negativa, que nenhum monitor pode emitir.

> **Portanto: o que a tela mostra é sempre uma aproximação dessaturada da cor
> monocromática correspondente.** Não é um bug. É o limite físico de um display
> de três primárias, e por isso `ColorResult::wasOutOfGamut` é quase sempre
> `true` — comportamento esperado, registrado no CSV de saída.

### Estratégias

| Estratégia | O que faz | Perde |
|------------|-----------|-------|
| `clip` | ceifa negativos em 0 | matiz, de forma inconsistente |
| `desaturate` | **padrão** — soma branco até zerar o negativo, depois escala | saturação e brilho |
| `desaturate-scale` | igual, mas normaliza o máximo para 1 sempre | luminância relativa |

**O segundo passo de `desaturate` não é opcional.** Dessaturar sozinho deixa o
maior canal muito acima de 1 — em 650 nm, `(7,458; 0; 0,567)`. Um *clamp* por
canal nesse ponto ceifaria R de 7,458 para 1,0 deixando B em 0,567, mudando as
razões entre canais e portanto o matiz: **650 nm saía magenta `(255,0,198)` em
vez de vermelho**. Esse foi um bug real, corrigido, com teste de regressão em
`cie.gamut_preserva_o_matiz_em_vez_de_ceifar_canal_a_canal`.

Escalar os três juntos reduz o brilho e preserva o matiz — a troca certa, já que
aqui o brilho absoluto já é escolha de visualização, não dado.

## Normalização de luminância

A eficiência luminosa do olho cai quase a zero em 400 e 750 nm, então cores
espectrais nas bordas sairiam praticamente pretas. `normalise_luminance: true`
reescala tudo para `Y = 1`.

> É **escolha de visualização, não física**: torna as cores comparáveis entre si
> ao custo de descartar a informação real de brilho. Fica registrada no
> manifesto.

## Pseudocor: fora do visível não existe cor

Não existe "a cor real" do infravermelho. Não existe cor nenhuma. Cor é uma
resposta do sistema visual humano, e o sistema visual humano não responde a
infravermelho nem a raios X.

Qualquer cor mostrada para essas bandas é um **rótulo inventado**, exatamente
como as cores de um mapa topográfico.

Por isso `ColorResult::isFalseColour` existe e viaja até a interface. A paleta
das bandas não visíveis usa tons dessaturados e metálicos, deliberadamente longe
das cores saturadas do locus espectral, para não se confundir com cor medida.

| Banda | λ | Pseudocor |
|-------|---|-----------|
| Rádio | > 1 m | marrom escuro |
| Micro-ondas | 1 mm – 1 m | bronze |
| Infravermelho | 750 nm – 1 mm | terracota |
| **Visível** | **400 – 750 nm** | **cor medida** |
| Ultravioleta | 10 – 400 nm | lilás |
| Raios X | 10 pm – 10 nm | ciano pálido |
| Gama | < 10 pm | quase branco |

### Modos

| Modo | Visível | Fora do visível |
|------|---------|-----------------|
| `visible-only` | cor medida | **preto** — honesto e legível |
| `visible-with-bands` | cor medida | pseudocor (**padrão**) |
| `full-em-spectrum` | pseudocor | pseudocor |

Em `full-em-spectrum` até o visível é marcado `isFalseColour = true`, porque
nesse modo ele recebe um rótulo de banda em vez da sua cor medida.

## Mistura de cores

`weightedMix` opera em **XYZ (luz linear)**, nunca em sRGB codificado. Misturar
valores gamma-codificados — erro que quase todo código de visualização comete —
escurece o resultado, porque a média de duas codificações não é a codificação da
média da luz.

> **Atenção ao significado:** a média de duas cores espectrais **não** é a cor de
> um som com essas duas frequências. É a cor de uma mistura aditiva de luz. Som
> não se combina assim — duas senoides somadas produzem um espectro com **dois
> picos**, não um pico intermediário.
>
> A operação é defensável como decisão de *apresentação* (resumir um espectro em
> uma cor), não como modelo de nada. E uma mistura que inclua qualquer pseudocor
> é, como um todo, pseudocor.
