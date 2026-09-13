# Fidelidade — até onde o simulador é confiável

> Um simulador só é confiável se disser **em que medida** é confiável.

Este documento descreve o sistema de classificação que acompanha todo resultado
do SoundWave. Ele existe para que ninguém confunda três coisas muito diferentes:

- uma grandeza **exata**,
- uma grandeza **medida com incerteza**,
- uma **escolha** sem valor verdadeiro.

```bash
soundwave fidelity                              # orçamento da configuração padrão
soundwave fidelity --config configs/scale.yaml --hz 440
soundwave medium sea-water                      # orçamento dos modelos de um meio
```

O orçamento também vai no `manifest.yaml` de toda execução de `analyze`.

---

## As cinco classes

| Classe | Significado | Tem barra de erro? | Exemplo |
|---|---|---|---|
| **`Exact`** | Exato por definição ou identidade matemática | sim, ~10⁻¹⁶ | `c`, `λ = c/f`, `E = h·f` |
| **`Measured`** | Medida empírica com incerteza declarada | **sim** | frequência dominante da FFT |
| **`ModelFit`** | Ajuste empírico válido em faixa declarada | **sim** | Sellmeier, ISO 9613-1, Mackenzie |
| **`Convention`** | Padrão acordado; não há valor verdadeiro, mas há acordo | às vezes | primárias sRGB, observador CIE 1931 |
| **`Arbitrary`** | Escolha livre sem base física | **não** | o mapeamento f_som → f_EM |

A ordem acima é de confiabilidade decrescente. O **elo mais fraco** governa a
interpretação de todo o resultado.

---

## A regra central

> **Se qualquer etapa da cadeia for `Arbitrary`, a incerteza combinada é
> reportada como INDEFINIDA.**

Isso não é conservadorismo. É correção.

Considere: `440 Hz → logarítmico → 566,1 nm`. Qual o erro desses 566,1 nm?

A pergunta não tem resposta, porque não existe um "verdadeiro comprimento de onda
de 440 Hz" contra o qual comparar. Escrever `566,1 ± 0,3 nm` seria uma **mentira
com casas decimais**: daria ao resultado a aparência de uma medida que ele não é.

As etapas físicas continuam válidas isoladamente. O que não se pode fazer é
combiná-las em um número único e chamar isso de precisão.

Verificado em `fidelity.uma_etapa_arbitraria_torna_a_incerteza_indefinida`.

### Quando a incerteza É definida

Para cadeias puramente físicas, o orçamento combina as incertezas relativas por
**soma quadrática** (raiz da soma dos quadrados):

```
u_total = √(u₁² + u₂² + ... + uₙ²)
```

Isso pressupõe **independência** entre as fontes de erro — o que vale aqui, já que
as etapas vêm de medidas independentes (acústica, colorimetria, óptica). Onde não
valesse, a soma linear seria o limite conservador correto.

Exemplo: `soundwave medium sea-water` reporta uma incerteza definida, porque ali
não há mapeamento nenhum — só física.

---

## Orçamento por etapa

### Camada 1 — Análise (`Measured`)

| Grandeza | Incerteza | Fonte |
|---|---|---|
| Frequência dominante | `0,05 · Δf / f` | medido pela suíte de testes |

Com `Δf = 10,77 Hz` e `f = 440 Hz`, isso dá **±0,12%**.

> **Ressalva:** vale para um tom isolado e bem resolvido. Com parciais
> sobrepostos ou vazamento o erro cresce. **Abaixo de Δf, duas notas não são
> separáveis de forma alguma** — nenhuma interpolação muda isso.

### Camada 2 — Transformação

| Mapeamento | Classe | Incerteza |
|---|---|---|
| `identity` | **`Exact`** | 0 |
| `scale` | `Arbitrary` (1 parâmetro) | indefinida |
| `linear`, `logarithmic`, `octave`, `custom` | `Arbitrary` (função inteira) | indefinida |

`scale` é epistemicamente superior aos arbitrários comuns: a forma da função é
física, e só a constante `K` é escolhida. **Uma** escolha em vez de uma curva
inteira — ainda que uma só baste para tornar o resultado não-medida.

Verificado em `fidelity.escala_declara_exatamente_uma_etapa_arbitraria`.

### Camada 3 — Conversão

| Grandeza | Classe | Incerteza | Ressalva |
|---|---|---|---|
| `c` | `Exact` | 0 | exata por definição do SI desde 1983 |
| `λ = c/f` | `Exact` | 0 | identidade |
| Limites 400–750 nm | `Convention` | — | a sensibilidade decai continuamente; não há corte objetivo |
| CIE 1931 2° | `Convention` | ~1% | **média de um painel pequeno de 1931**; a variação entre observadores reais chega a vários por cento e *excede* esta incerteza |
| sRGB / D65 | `Convention` | 0 | define um monitor ideal; a tela real difere |
| Mapeamento de gamut | **`Arbitrary`** | — | >90% das cores espectrais não cabem no sRGB |
| Normalização de luminância | **`Arbitrary`** | — | descarta a eficiência luminosa real |

> Note que a camada 3 contém etapas `Arbitrary` **mesmo sem nenhum mapeamento**.
> Ou seja: mesmo com `identity`, a saída *colorida* não é uma medida. O que é
> exato em `identity` é a transformação de frequência, não a cor.

### Modelos físicos de meio

| Modelo | Classe | Incerteza | Validade |
|---|---|---|---|
| Mackenzie (1981) | `ModelFit` | 0,005% | 0–30 °C, 25–40 ppt, 0–8000 m |
| Gás ideal (ar) | `ModelFit` | 0,1% | ar seco, ~1 atm |
| Sólidos (gelo, sílica) | `Measured` | 5% | valor único, sem dependência de T |
| ISO 9613-1 | `ModelFit` | 10% | −20 a 50 °C, 50 Hz–10 MHz |
| Francois–Garrison | `ModelFit` | 5% | −2 a 22 °C, 400 Hz–1 MHz |
| Malitson (sílica) | `ModelFit` | 0,001% | 210–6700 nm |
| Daimon (água) | `ModelFit` | 0,027% | 182–1129 nm, 20 °C |
| Segelstein (absorção) | `ModelFit` | **por faixa** | ver abaixo |

---

## Incerteza declarada por faixa

Um número único de incerteza às vezes é uma simplificação **desonesta**. A
absorção óptica da água é o caso:

| Faixa | Incerteza | Ressalva |
|---|---|---|
| 380–500 nm | **até 500%** | Segelstein superestima 3–8× frente a Pope & Fry (1997) e põe o mínimo em ~475 nm em vez de ~420 nm. **Use apenas como ordem de grandeza.** |
| 500–700 nm | 10% | concorda com Pope & Fry |
| >700 nm | 20% | região fortemente absorvente |

A água é tão transparente no azul que a medida é experimentalmente difícil, e
compilações antigas herdam erro.

**Por que ainda assim é utilizável:** a conclusão que o projeto extrai —
*"a água é passa-alta óptico: mata o vermelho, passa o azul"* — é **robusta** ao
erro. Com Segelstein a razão α(700 nm)/α(mínimo) é ~32×; com Pope & Fry seria
~139×. **O erro atenua a conclusão, não a inverte.**

Esse raciocínio — *a conclusão sobrevive ao erro conhecido?* — é o que separa um
modelo utilizável de um número bonito.

Verificado em `fidelity.absorcao_da_agua_declara_incerteza_por_faixa`.

---

## Zero significa "não sabemos", não "não há"

Quando um modelo não está implementado, o SoundWave devolve zero — mas a
reivindicação correspondente é classificada `Arbitrary` e **diz isso**:

```
[arbitrario] absorcao acustica (ice)
    fonte  : NAO MODELADO -- devolve zero
    ressalva: zero aqui significa 'nao sabemos', nao 'nao ha atenuacao'.
              Solidos reais atenuam, e fortemente.
```

Um zero silencioso seria indistinguível de uma medida de transparência perfeita.
Verificado em `physics.meios_nao_modelados_se_declaram_como_tal`.

---

## A lição generalizável

Este projeto já cometeu o erro que o sistema de fidelidade existe para prevenir.

As funções de correspondência de cor usavam um ajuste analítico justificado por
ter **"erro abaixo de ~1% do pico"**. Verdade — em valor **absoluto**. Mas cor
depende das **razões** entre `x̄`, `ȳ` e `z̄`, e acima de 700 nm essas funções
valem ~10⁻⁵. Erro absoluto minúsculo sobre valores minúsculos é erro **relativo**
gigante: 736 nm era renderizado como **verde**.

E os testes da época passavam, porque verificavam picos e o centro do visível.

> **"Erro pequeno" só significa alguma coisa em relação à grandeza que se vai
> usar.** Aqui o que importava era a razão, não o valor — então a tolerância
> tinha de ser relativa desde o início.

Detalhes em [`color.md`](color.md).

---

## O critério também se aplica a este projeto

A cadeia de verificação dos modelos físicos **não é uniforme**: Malitson e
ISO 9613-1 foram conferidos contra valores publicados independentes; Mackenzie e
Francois–Garrison foram conferidos apenas contra uma implementação de terceiros,
o que verifica o *port* mas herdaria qualquer erro de transcrição dela.

Detalhes em [`physics.md`](physics.md) §7. Registrar isso é o mesmo princípio que
o resto do documento defende: a diferença entre "verificado" e "verificado contra
o quê" é exatamente o tipo de coisa que um simulador confiável não esconde.

## Como ler um resultado do SoundWave

1. Leia o **elo mais fraco** no `manifest.yaml`.
2. Se for `Arbitrary` — e será, para qualquer saída colorida — o resultado é uma
   **representação**, não uma medida. Ele é reprodutível, comparável e
   verificável, mas não é uma afirmação sobre o mundo.
3. As grandezas **físicas** intermediárias (`λ`, `f_EM`, atenuação, transmitância)
   **são** medidas, com as incertezas da tabela acima. Use-as.
4. Quando quiser uma cadeia sem etapas arbitrárias, use `soundwave medium` — ele
   não mapeia nada, só reporta física, e devolve incerteza definida.
