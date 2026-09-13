# Camada 2 — Transformação: a escolha

> Esta é a única camada do pipeline que não é física nem perceptual.
> Ela é uma **escolha**. Todo o resto do projeto existe para manter essa
> escolha visível.

## Por que não existe "a" correspondência

Som é uma onda de **pressão** em um meio material. Luz é um campo
**eletromagnético** que se propaga no vácuo. Elas não compartilham mecanismo,
unidade de escala, nem ponto de referência comum. Não há nada em 440 Hz que
"aponte" para um comprimento de onda.

`M(f_som) = f_EM` é uma função inventada. Trocá-la troca inteiramente o
resultado, e nada no som mudou. O comando `soundwave compare` existe para
tornar isso impossível de ignorar.

Isso **não** torna o projeto inútil. Torna-o um estudo de *representações*, e
não de física — o que é uma coisa legítima, desde que dita.

## A assimetria que governa tudo

|  | Faixa | Fator | Oitavas |
|--|-------|-------|---------|
| Audível | 20 Hz – 20 kHz | 1000× | **9,97** |
| Visível | 400 – 750 THz | 1,875× | **0,91** |

Compressão: **10,99×**.

O visível cobre **menos de uma oitava**. Isso não é um detalhe de calibração: é
a restrição estrutural de que todo o resto decorre.

### Consequência: a oitava desaparece

Sob o mapeamento logarítmico, uma oitava musical (fator 2 em `f_som`) vira:

$$2^{1/10{,}99} = 1{,}065 \quad\Rightarrow\quad \textbf{+6,5\% em } f_{EM}$$

Seis por cento e meio. A equivalência de oitava — o fato perceptual mais sólido
da percepção musical, a razão de A4 e A5 se chamarem ambos "A" — vira um
deslocamento de matiz quase imperceptível.

Medido pelo projeto (`example_mapping_comparison`), a dispersão de cor entre as
nove oitavas de A, de 55 Hz a 14 kHz:

| Mapeamento | Dispersão |
|------------|-----------|
| logarítmico | **384,7** |
| oitava | **0,0** |

**A resposta à pergunta 2 do plano — "o mapeamento logarítmico preserva melhor
as relações musicais percebidas?" — é: não a mais forte delas.** Ele preserva a
ordem e as razões de altura. Não preserva a equivalência de oitava.

## Os mapeadores

### `LogMapper` — padrão

```
x    = (ln f − ln f_min) / (ln f_max − ln f_min)
f_EM = f_EM,min · (f_EM,max / f_EM,min)^x
```

**Preserva:** ordem, e razões iguais de frequência viram razões iguais em `f_EM`
(intervalos musicais iguais ocupam distâncias iguais).
**Não preserva:** equivalência de oitava.
**Invertível:** sim.

É o padrão razoável porque a percepção de altura é logarítmica.

### `OctaveMapper` — classe de altura

```
x    = frac(log₂(f / f_ref))
f_EM = f_EM,min · (f_EM,max / f_EM,min)^x
```

**Preserva:** equivalência de oitava — A4 e A5 recebem exatamente a mesma cor.
É o único mapeador que faz isso.
**Não preserva:** monotonicidade nem injetividade.
**Invertível:** **não**. Infinitas frequências compartilham cada cor, então
"que som gerou esta cor?" deixa de ter resposta única.

Há ainda uma descontinuidade de cor a cada oitava (`x` salta de 1 para 0), que
aparece como emenda visível em varreduras contínuas.

Troca-se **invertibilidade por fidelidade perceptual**. Nenhuma das duas
escolhas é "correta" — elas respondem a perguntas diferentes:

- *"Que frequência produziu esta cor?"* → `LogMapper`
- *"Que nota produziu esta cor?"* → `OctaveMapper`

### `LinearMapper` — contra-exemplo deliberado

```
x    = (f − f_min) / (f_max − f_min)
f_EM = f_EM,min + x · (f_EM,max − f_EM,min)
```

Está no projeto **para ser ruim de um jeito instrutivo**. Em escala linear,
metade do eixo audível (10–20 kHz) é uma única oitava que quase ninguém
distingue, enquanto as nove oitavas restantes se espremem na outra metade.

Medida direta: a escala cromática inteira de A4 a A5 recebe a cor
`(255, 0, 86)` — **idêntica nas treze notas**. 440 a 880 Hz ocupa 2% do eixo
audível linear.

### `CustomMapper` — hipóteses novas sem escrever C++

Interpolação linear por partes em espaço **log-log** sobre pontos de controle
vindos do arquivo de configuração. Monotônico se e somente se os pontos forem —
verificado na construção e reportado em `describe()`.

## Fora do domínio

| Política | Comportamento | Quando usar |
|----------|---------------|-------------|
| `clamp` | prende na borda | padrão; seguro, achata os extremos |
| `extrapolate` | continua a função | deliberadamente sair do visível |
| `discard` | devolve `NaN` | ignorar bins fora da faixa de interesse |

`NaN` não é erro: é o sinal de que o bin existe no sinal mas foi
**deliberadamente** deixado fora do mapeamento. Ele fica preto e com peso zero,
mas não some do `Spectrum`.

## O alvo não precisa ser o visível

`target_min_hz` e `target_max_hz` são parâmetros livres. Apontar para o
infravermelho (`1e13`–`3e13` Hz) é válido, e a camada 3 responderá com
pseudocores corretamente rotuladas. Nada no código assume que o alvo é visível.

## Um critério externo: sobrevivência conjunta

Há agora uma forma de comparar mapeamentos que não depende de gosto:

```
J(M) = média sobre log f de  T_som(f) · T_luz(M(f))
```

Ela penaliza corretamente o `linear` (3,6× pior que o `logarithmic`) e dá zero
para mapeamentos que não põem nada no visível. Ver `soundwave align` e
[`physics.md`](physics.md) §6.

**O que ela NÃO conseguiu:** encontrar um alinhamento. Os quatro candidatos
testados empatam dentro de 0,04%, porque as janelas de transição dos dois filtros
estão separadas por 4 décadas e não se sobrepõem. Resultado nulo, medido e
documentado.

## Como julgar um mapeamento

Não existe métrica única. O projeto expõe três eixos, e `describe()` de cada
mapeador declara os três — esse texto vai direto para o manifesto:

1. **Monotonicidade** — a ordem de altura sobrevive?
2. **Invertibilidade** — dá para voltar da cor para o som?
3. **Equivalência de oitava** — a relação musical mais forte sobrevive?
4. **Sobrevivência conjunta** — quanto resiste à travessia de um meio real?

Nenhum mapeador satisfaz os três. O teorema informal por trás disso: com
compressão de 11×, preservar equivalência de oitava exige periodicidade, e
periodicidade exclui monotonicidade e injetividade.
