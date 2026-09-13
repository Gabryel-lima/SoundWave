# Física dos meios — o que é possível e o que não é

Este documento responde a uma pergunta específica:

> *"Em vez de espremer ou esticar, dá para buscar equivalência inserindo os dois
> espectros em meios reais? Som é mecânico e depende do meio; luz também."*

A intuição aponta para algo correto — procurar **estrutura física compartilhada**
em vez de uma função inventada. Mas a proposta na forma direta **não funciona**,
e a razão é estrutural, não técnica. Este documento demonstra por quê, e mostra a
versão da ideia que funciona.

---

## 1. O erro central: a frequência não muda

> **Quando uma onda cruza para outro meio, a frequência NÃO muda.
> O que muda é a velocidade e, por consequência, o comprimento de onda.**

A razão é a **continuidade de fase na interface**: a fronteira é forçada a oscilar
na frequência da onda incidente, e é ela a fonte da onda transmitida. Se a
frequência mudasse, os dois lados deixariam de casar em fase.

Teste perceptual direto: um objeto vermelho **continua vermelho** visto debaixo
d'água, embora λ tenha encolhido de 650 nm para ~487 nm.

> **Cor segue frequência, não comprimento de onda no meio.**

Verificado em `physics.vacuo_nao_propaga_som` e na tabela de
`soundwave medium`.

---

## 2. A invariância: o meio cancela

Dentro de um meio, `v` é aproximadamente constante, então `λ = v/f`. A razão entre
os extremos de uma faixa é:

```
λ_max / λ_min = (v/f_min) / (v/f_max) = f_max / f_min      ← v cancela
```

**O meio some da conta.** Trocar de meio multiplica todos os comprimentos de onda
por um fator constante, e um fator constante não altera a razão máx/mín.

| Meio | v_som (m/s) | λ@20 Hz | λ@20 kHz | **razão** |
|---|---|---|---|---|
| ar | 343 | 17,16 m | 1,72 cm | **1000,0** |
| água doce | 1482 | 74,09 m | 7,41 cm | **1000,0** |
| água do mar | 1521 | 76,07 m | 7,61 cm | **1000,0** |
| gelo | 3840 | 192,00 m | 19,20 cm | **1000,0** |
| sílica fundida | 5968 | 298,40 m | 29,84 cm | **1000,0** |

| Meio | n(550 nm) | λ@400 nm | λ@750 nm | **razão** |
|---|---|---|---|---|
| vácuo | 1,0000 | 400,0 nm | 750,0 nm | **1,8750** |
| ar | 1,0003 | 399,9 nm | 749,8 nm | **1,8750** |
| gelo | 1,3100 | 305,3 nm | 572,5 nm | **1,8750** |
| água | 1,3347 | 297,7 nm | 564,1 nm | 1,8948 |
| sílica fundida | 1,4599 | 272,1 nm | 515,7 nm | 1,8955 |

Os comprimentos de onda individuais **mudam muito** — água do mar quadruplica λ
acústico frente ao ar. O que não muda é a razão.

> **A compressão de ~11× entre audível e visível é invariante sob qualquer
> mudança de meio.** Não é artefato do mapeamento nem de ignorar o meio.

Verificado em `physics.razao_acustica_e_exatamente_invariante_ao_meio`.

### A única exceção: dispersão

`n` depende de λ — é por isso que prismas funcionam. Então `λ_meio = λ_vac/n(λ_vac)`
**não** é escala constante, e a razão muda um pouco:

| | razão 400–750 nm |
|---|---|
| vácuo | 1,8750 |
| água | 1,8948 (**+0,96%**) |
| sílica fundida | 1,8955 (+1,09%) |

Real, e cerca de **1%**. Você precisa de um fator de 11.

---

## 3. Não existe meio que resolva

Para som audível ter comprimento de onda visível (550 nm) seria preciso:

| f | v_som necessário |
|---|---|
| 20 Hz | 1,1 × 10⁻⁵ m/s |
| 20 kHz | 1,1 × 10⁻² m/s |

O meio mais lento entre os catalogados está na casa de centenas de m/s. Faltam
**4 a 7 ordens de grandeza**. Não é uma questão de procurar melhor.

O caminho inverso é pior: para luz ter λ acústico (17 m a 1,7 cm) seria preciso
`n ≈ 3 × 10⁻⁸`. Índices menores que 1 existem (raios X, plasmas perto de
ressonância), mas 10⁻⁸ significa velocidade de fase de 10⁸ c. Não é um meio real.

Verificado em `physics.nao_existe_meio_com_som_audivel_em_lambda_visivel`.

---

## 4. As hipóteses nulas: e se você não inventar nada?

O projeto implementa os dois únicos mapeamentos que **não** exigem inventar uma
função. Eles existem para tornar o argumento executável.

### `identity` — f_EM = f_som

O único mapeamento com justificativa física **plena**. Um fônon de frequência `f`
tem energia `E = h·f`, exatamente como um fóton. A identidade é o mapeamento que
**preserva energia por quantum**.

| f_som | f_EM | λ | banda |
|---|---|---|---|
| 20 Hz | 20 Hz | 15.000 km | rádio ELF |
| 440 Hz | 440 Hz | 681 km | rádio ELF |
| 20 kHz | 20 kHz | 15 km | rádio VLF |

Um fóton de 440 Hz carrega 1,8 × 10⁻¹² eV. Um fóton visível carrega 2,25 eV —
**doze ordens de grandeza** acima.

**Fidelidade: `Exact`.** Nada escolhido, nenhuma incerteza. E nada visível.

### `scale` — λ_luz = K · λ_som

A cadeia:

| Passo | Natureza |
|---|---|
| `λ_som = v(meio)/f` | **física**, dado v |
| `λ_alvo = K · λ_som` | **a única escolha livre** |
| `λ_vac = n(λ_vac)·λ_alvo` | **física** (dispersão real, resolvida por iteração) |
| `f_EM = c/λ_vac` | **física** exata |

A **forma** da função é fixada pela física: λ é inversamente proporcional a f, e
ponto. Não há liberdade de curvatura. Toda a arbitrariedade cabe em **um número**.

Isso é estritamente melhor, do ponto de vista epistêmico, que `log` ou `linear`:
lá a forma inteira é invenção; aqui só a escala é.

**E o preço é alto e mensurável.** Ancorando 20 Hz em 750 nm:

| f_som | λ | banda |
|---|---|---|
| 20 Hz | 750 nm | **visível** |
| 30 Hz | 500 nm | **visível** |
| 37,5 Hz | 400 nm | **visível** |
| 440 Hz | 34,1 nm | ultravioleta |
| 20 kHz | 0,75 nm | raios X |

> **Só 20–37,5 Hz cai no visível: 0,907 de 9,966 oitavas — 9,1% da banda audível.**

E 0,907 não é coincidência: escala pura é uma **isometria em espaço logarítmico**,
então a janela visível captura exatamente sua própria largura em oitavas. Trocar o
meio acústico muda `K`, mas **não** muda esses 9,1% — corolário direto da
invariância.

Verificado em `physics.escala_so_leva_nove_porcento_do_audivel_ao_visivel` e
`physics.a_janela_visivel_da_escala_nao_depende_do_meio`.

> **Este é o preço real da honestidade física. Esticar e espremer não é uma
> trapaça opcional — é a única forma de o audível caber no visível.**

---

## 5. Onde a ideia funciona: os filtros antagônicos

O meio não desloca frequências. Mas **atenua** de forma dependente da frequência,
nos dois domínios. Isso é estrutura compartilhada real, medida, e não inventada.

Distância de meia potência (−3 dB) em água do mar a 20 °C — a grandeza
comparável, porque é livre de escala:

| SOM | | LUZ | |
|---|---|---|---|
| 20 Hz | **90.607 km** | 400 nm | 7,0 m |
| 100 Hz | 3.637 km | 450 nm | **15,3 m** |
| 440 Hz | 200,6 km | 500 nm | 14,9 m |
| 2 kHz | 21,2 km | 550 nm | 6,1 m |
| 8 kHz | 5,4 km | 650 nm | 1,1 m |
| 20 kHz | 1,2 km | 750 nm | **13,2 cm** |

Som de 20 Hz percorre mais de **duas voltas na Terra** antes de perder metade da
potência — é o canal SOFAR, que baleias usam. Luz vermelha percorre 13 cm.

> **A água é passa-baixa para som e passa-alta para luz.**

E aqui está o resultado:

> **O mesmo meio preserva os GRAVES do som e os AZUIS da luz.
> Mas todo mapeamento monotônico leva grave em VERMELHO.
> O meio destrói exatamente aquilo que o mapeamento preservou.**

Com o mapeamento logarítmico, 20 Hz (o que melhor sobrevive no som) vai para
750 nm — o que **pior** sobrevive na luz. Os dois filtros são antagônicos.

> **"O mesmo ambiente para os dois" não produz equivalência. Produz conflito.**

Isso é um resultado, não um obstáculo. Verificado em
`physics.os_dois_filtros_sao_antagonicos`.

```bash
soundwave medium sea-water          # a demonstração completa
soundwave medium air --path 1000
```

---

## 6. Limitações que NÃO podem ser superadas

Estas não são pendências de implementação. São restrições estruturais; nenhuma
versão futura do SoundWave vai removê-las.

| # | Limitação | Por quê é intransponível |
|---|---|---|
| 1 | **Frequência não muda com o meio** | Continuidade de fase na interface. Consequência de as equações de onda serem lineares com condições de contorno; não há meio que contorne. |
| 2 | **A compressão de 11× é invariante** | `v` e `n` cancelam na razão entre extremos. É álgebra, não aproximação. |
| 3 | **Nenhum meio real fecha a lacuna** | Faltam 4 a 7 ordens de grandeza em `v_som`. |
| 4 | **Escala pura só cobre 9,1% do audível** | Isometria em espaço log: a janela captura sua própria largura (0,907 oitava). |
| 5 | **Equivalência de oitava e monotonicidade são incompatíveis** | Preservar oitava exige periodicidade; periodicidade exclui injetividade. Ver `mapping.md`. |
| 6 | **>90% das cores espectrais não cabem no sRGB** | O locus espectral é externo ao triângulo de três primárias. Limite de qualquer display RGB. |
| 7 | **Som não se combina como luz** | Duas senoides somadas dão dois picos; duas luzes somadas dão uma cor intermediária. A mistura ponderada é apresentação, não modelo. |
| 8 | **Não existe "a cor de uma frequência sonora"** | Som e luz não compartilham mecanismo. Qualquer correspondência é escolhida. |

### Limitações que são apenas do estado atual

Estas **podem** melhorar, e vale distinguir:

| Limitação | Como superar |
|---|---|
| Absorção óptica usa Segelstein (ruim em 380–500 nm) | Trocar por Pope & Fry (1997) |
| Sem espalhamento Rayleigh (por que o céu é azul) | Modelar σ ∝ λ⁻⁴ |
| Absorção acústica em sólidos não modelada | Adicionar dados de gelo e sílica |
| Sem dependência de temperatura em gelo e sílica | Dados dependentes de T |
| Água doce fora da faixa ajustada de Mackenzie | Usar Marczak (1997) |
| Sem dispersão acústica | Relevante só em guias de onda e líquidos com bolhas |

---

## 7. Fontes

| Modelo | Fonte | Validação neste projeto |
|---|---|---|
| Velocidade do som na água | Mackenzie (1981), *JASA* 70(3), 807 | 1539,0866 m/s a (27 °C, 35 ppt, 10 m) |
| Absorção acústica na água | Francois & Garrison (1982), *JASA* 72(3) e 72(6) | 10,7103 dB/km a 50 kHz |
| Absorção atmosférica | ISO 9613-1:1993 | reproduz a tabela da ISO 9613-2 a 10 °C/70% UR dentro de 2% |
| Índice de refração da água | Daimon & Masumura (2007), *Appl. Opt.* 46(18), 3811 | n(589,3 nm) = 1,333349 |
| Índice da sílica fundida | Malitson (1965), *JOSA* 55(10), 1205 | n(587,6 nm) = **1,45846**, casa exatamente |
| Absorção óptica da água | Segelstein (1981), tese, Univ. Missouri-Rolla | ver ressalva em §6 |

Todas as constantes vieram de implementações ou tabelas publicadas. Nenhuma foi
escrita de memória. Ver `tests/test_physics.cpp`.

### A cadeia de verificação não é uniforme — e isso importa

Aplicando o próprio critério de fidelidade a esta documentação: os seis modelos
**não** têm o mesmo grau de verificação.

| Modelo | Cadeia | Força |
|---|---|---|
| Malitson (sílica) | C++ → constante publicada independente (`n_d = 1,45846`) | **forte** — checagem externa |
| ISO 9613-1 | C++ → tabela publicada da ISO 9613-2 | **forte** — checagem externa |
| Daimon (água) | C++ → base refractiveindex.info → artigo | **média** — confere com IAPWS em 0,027% |
| Mackenzie | C++ → implementação publicada (`arlpy`) | **fraca** — só o *port* foi verificado |
| Francois–Garrison | C++ → implementação publicada (`arlpy`) | **fraca** — só o *port* foi verificado |
| Segelstein | C++ → base de dados → tese | **fraca** + ressalva de faixa |

Nos dois casos "fraca", o que foi verificado é que o código C++ reproduz uma
implementação de terceiros dentro de 10⁻³. **Qualquer erro de transcrição
cometido por essa implementação em relação ao artigo original seria herdado por
este projeto sem ser detectado.** Os valores de Mackenzie e Francois–Garrison
reproduzem ordens de grandeza corretas e a fenomenologia esperada (absorção ∝ f²,
canal SOFAR), o que é evidência circunstancial — não é uma checagem contra a
fonte primária.

Fechar essa lacuna exige conferir contra os artigos originais, que não estavam
acessíveis neste ambiente. Fica registrado como pendência, não como resolvido.
