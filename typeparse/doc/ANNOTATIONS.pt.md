# Anotações em Tcl — documentação

*19 de outubro de 2026 · versão v13 · Florent MERLET, com Claude (Anthropic)*

*Outras línguas: [Français](ANNOTATIONS.fr.md) (referência) · [English](ANNOTATIONS.en.md) · [Deutsch](ANNOTATIONS.de.md) · [Nederlands](ANNOTATIONS.nl.md) · [Español](ANNOTATIONS.es.md) · [Italiano](ANNOTATIONS.it.md)*

## Prefácio

**Autores**: Florent MERLET, com Claude (Anthropic).

Este documento descreve as **anotações** do Tcl 9.0 propostas pelo TIP 764: um metadado associado a uma palavra (`{x}palavra`) que não muda o valor, mas permite tipar variáveis, nomear campos e descrever formatos binários. Substitui o manual escrito ao longo do desenvolvimento e descreve o protótipo na **versão v13**.

Três partes, para três leitores:

- **Parte I — Conceitos**: o que é uma anotação, a diferença entre uma nota e um tipo, e as suas regras. Para ler primeiro.
- **Parte II — Referência**: os comandos `note` e `type`, a linguagem de tipos, os caminhos, a construção de valores, os tipos binários e os erros. Para quem escreve Tcl.
- **Parte III — Implementação**: o objeto anotado, o analisador, o bytecode, os ficheiros e funções envolvidos. Para quem revê o TIP ou o código.

Seguem-se a compatibilidade e o desempenho, as escolhas de conceção, os limites e os anexos (instalação, glossário, correspondências com C, Rust e LLVM).

Os exemplos são Tcl executável no protótipo; os comentários `;#` dão o resultado. As mensagens de erro são citadas tal como o Tcl as produz. Os identificadores dos exemplos (`nom`, `fiche`, `couleur`, `grandeur`…) mantêm-se como no original francês, para corresponderem aos scripts de exemplo.

## Parte I — Conceitos

### 1. As anotações

#### 1.1 Origem da palavra

*Anotar* vem do latim *ad notare*, «pôr uma nota ao lado». A palavra *protocolo* diz o mesmo de outra forma: o *prōtokollon* grego era a primeira folha colada no início de um rolo de papiro, que descrevia o seu conteúdo sem fazer parte dele. Uma anotação Tcl é exatamente isso: uma folha colada no início de uma palavra.

```tcl
{mm}12          ;# o valor continua a ser 12; «mm» está colado à frente
{list.int}L     ;# o nome continua a ser L; «list.int» diz o que vai conter
```

#### 1.2 Um metadado

Uma anotação é um **metadado**:

- **não aparece no texto** do valor: `{mm}12` vale `12` para `puts`, `expr`, `string length`;
- **traz uma informação** que o valor sozinho não tem: uma unidade, um tipo, o nome de um campo, uma opção;
- vive **fora do plano dos valores em bruto**: nenhum comando Tcl existente a vê, e nenhum script existente muda de sentido.

#### 1.3 Uma analogia: a parte imaginária

Um número complexo tem uma parte real e uma parte imaginária. Da mesma forma, um valor anotado tem uma parte **em bruto** (o texto que o Tcl sempre manipulou) e uma parte **anotada** (o que o programador tem em mente).

| | número complexo | valor anotado |
|---|---|---|
| parte visível | a parte real | o valor em bruto (`12`) |
| parte acrescentada | a parte imaginária | a anotação (`mm`, `int`, `nom`) |
| o que se lê habitualmente | a parte real | a string |

A analogia para no cálculo. Num complexo, a parte imaginária participa na aritmética. Aqui, **um cálculo perde a anotação**: `expr {$x + 1}` devolve um número nu. Só a cópia, `lindex`, `string cat` e a passagem como argumento a mantêm. É de propósito: o plano dos valores em bruto continua a ser Tcl comum, e é isso que garante a compatibilidade.

#### 1.4 Guiar a execução

A anotação torna explícita a «parte imaginária» do programador, aquilo que ele sabe dos seus dados e que o Tcl ignorava:

- um **tipo** faz verificar cada escrita numa variável (`set {int}n abc` falha);
- um **nome de campo** escolhe para onde vai um valor (`set {pt}a {age}12 {nom}Jean`);
- uma **representação binária** diz como ler e escrever bytes (`{int(u,16,be)}version`);
- uma **nota** marca um valor para que um procedimento o reconheça (`note has $v mm`).

O script mantém a sua forma: acrescentam-se anotações onde se querem garantias, e em mais lado nenhum.

### 2. Notas, tipos e anotações locais

Uma anotação escreve-se sempre `{texto}palavra`. Conforme o que o texto diz e conforme **quem o lê**, desempenha três papéis.

#### 2.1 A nota: a anotação em bruto

Uma **nota** é uma anotação que nenhum construtor resolveu ainda. Segue o valor: cópia, argumento de procedimento, elemento de lista.

```tcl
set x {mm}5
note get $x          ;# mm
type get $x          ;# (vazio: nenhum tipo resolvido)
```

**Empilhamento.** Anota-se um valor já anotado; as notas empilham-se, da mais antiga para a mais recente.

```tcl
set y {px}$x
note get $y          ;# mm.px      (px é a mais recente)
```

A escrita `{a}{b}$x` (duas anotações coladas) não existe: seria ambígua com `{int}{300}`, uma anotação sobre um literal entre chavetas. Empilha-se anotando um valor que já o está.

#### 2.2 O tipo: a anotação resolvida por um construtor

Um **tipo** é uma anotação **resolvida** na tipologia que o interpretador conhece (tipos de base, `type def`, `type alias`). Só um **construtor de variável** resolve:

- `set {T}v …` (e `set v {T}$x`);
- a ligação dos argumentos de `proc`;
- `lassign`, `foreach`, e em `expr`, `x = v` e `x is T = v`.

Posta no **nome** de uma variável, uma anotação dá à variável um **contrato**: toda a escrita posterior é verificada.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

Fora de um construtor, `{T}$x` não verifica nem converte nada, mesmo que `T` seja um tipo conhecido: `T` é simplesmente **empilhado**, como uma nota. `list {int}abc` não dá erro.

#### 2.3 Duas pilhas e uma regra de desempilhamento

Um valor anotado traz **duas listas**: o **tipo resolvido** (no máximo um) e a **pilha das notas** pendentes. A apresentação separa-as com `@`:

```
 list<int>@mm.hello
 └───────┘ └──────┘
 tipo resolvido   notas, da mais antiga (mm) para a mais recente (hello)
```

Um construtor **desempilha** a partir do topo:

1. pega na nota mais recente e tenta resolvê-la;
2. continua enquanto as notas se resolvem; a primeira que não se resolve interrompe tudo e fica na pilha;
3. os tipos desempilhados têm de ser coerentes: o mais recente tem de caber em cada um dos outros, senão `contradictory annotations px and qq`;
4. o valor é convertido («castado») para o tipo mais recente;
5. o contrato da variável faz o último controlo.

```tcl
type def px int
set z {px}$y         ;# y = @mm.px: px resolve-se, mm não
type get $z          ;# px      ; note get $z: mm
```

Uma nota posta **antes** do seu `type def` é portanto resolvida pelo construtor seguinte: escrever `type def mm double` mais tarde dá sentido aos `{mm}` já postos.

**Primeiro o contexto.** Um construtor resolve no seu contexto antes dos tipos globais: numa soma, os nomes das variantes (`{i}5` em `set {S}v {i}5`); num produto, os nomes dos campos (`{age}12`). Um tipo global com o mesmo nome não muda nada.

#### 2.4 As anotações locais

Uma anotação cujo texto contém um carácter que os nomes de tipo recusam (`-`, `?`, `:`…) **nunca** pode ser um tipo. É uma **anotação local**: só vive entre quem chama e o comando chamado, e não segue o valor.

```tcl
type binary {sizeof?}$T          ;# uma pergunta feita a type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# recusado: local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# recusado: local annotation {-v} cannot declare a variable
```

Duas famílias naturais, sem risco de colisão com um nome de tipo: as opções em `-nome` e as perguntas em `nome?`. Um nome de tipo só contém letras ASCII, algarismos, `_` e bytes não ASCII, e não começa por um algarismo.

#### 2.5 A anotação vazia `{}`

`{}` é «o tipo vazio». O seu efeito depende do sítio:

| onde | efeito |
|---|---|
| sobre um valor, num construtor (`set v {}$x`) | retira o tipo resolvido, mantém as notas |
| sobre um nome (`set {}v $x`) | levanta o contrato da variável; o valor é guardado com as suas notas, sem resolver nada |
| sobre um valor, fora de um construtor | o leitor vê o conteúdo nu (tipo e notas apagados) |

```tcl
set {int}n 5
set {}n abc          ;# o contrato é levantado: n vale abc
```

O valor original nunca é modificado: `{}` produz uma nova vista, o que torna inofensiva a partilha de objetos.

## Parte II — Referência

### 3. O comando `note`

```
note get VALOR               ;# as notas, da mais antiga para a mais recente, unidas por «.»
note has VALOR NOME          ;# 1 se NOME for uma das notas, senão 0
note delete VALOR ?NOME?     ;# o valor sem a nota NOME, ou sem nenhuma nota
```

Os valores Tcl são imutáveis: `note delete` **devolve** um novo valor, não modifica uma variável.

```tcl
set y {px}{mm}5              ;# extra characters after close-brace: não há empilhamento colado
set x {mm}5 ; set y {px}$x
note get $y                  ;# mm.px
note has $y mm               ;# 1
note get [note delete $y px] ;# mm
```

Uso típico: um procedimento que reage à marca de um valor.

```tcl
proc longueur {v} {
    if {[note has $v mm]} {return "$v millimètres"}
    return $v
}
longueur {mm}12              ;# 12 millimètres
```

### 4. O comando `type`

```
type def NOME DEFINIÇÃO        ;# define um tipo nominal (ou uma família parametrizada)
type alias NOME DEFINIÇÃO      ;# dá um nome estrutural a uma definição
type get VALOR                 ;# o tipo resolvido de um valor (vazio se não houver)
type cast TIPO VALOR           ;# o resolvedor de um construtor, sem variável
type convert DE PARA LAMBDA    ;# declara uma conversão explícita
type deref VALOR               ;# instantâneo: cada ligação var substituída pelo seu valor
type binary TIPO ...           ;# representação binária, codificação, descodificação (§ 9)
```

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12
type get $a                    ;# pt
type alias entier int
set {entier}e 3 ; type get $e  ;# int  (um alias não é um tipo novo)
type cast pt {Léa 7}          ;# Léa 7, do tipo pt
```

Em `expr`, o operador **`is`** testa o tipo de uma variável, e uma atribuição pode declarar:

```tcl
expr {a is pt}                 ;# 1
expr {z is pt = ("Zoé", 30)}   ;# declara z do tipo pt e atribui-lhe o valor
expr {new is cmd}              ;# 1 se new for um comando; new(…) passa a ser uma função de expr
```

`is cmd` não é um tipo: testa a existência de um comando e expõe-no como função de `expr`.

### 5. O sistema de tipos

#### 5.1 Tipos de base

| tipo | valores |
|---|---|
| `int` | inteiro (sem limite, como em Tcl) |
| `double` | vírgula flutuante |
| `string` | qualquer string |
| `list` | lista Tcl comum (elementos sem tipo) |
| `dict` | dicionário Tcl comum |
| `var` | ligação a uma variável, pelo seu nome (§ 5.3) |
| `any` | elemento de uma lista heterogénea (`list.any`): cada elemento mantém o seu tipo |

Os **refinamentos** restringem um tipo de base e trazem uma representação binária: `int(u|s, n ?, be|le?)`, `double(32|64 ?, be|le?)`, `string(ENC, N|z)`, `bitarray(n ?, lsb0|msb0?)` (§ 9).

Um `type def` cria um tipo **nominal**: `type def port int(u,16)` é distinto de `int(u,16)`. Um `type alias` apenas dá um nome a uma definição.

#### 5.2 A linguagem de composição

Um tipo escreve-se como uma expressão, analisada segundo o modelo de `expr` (`tclTypeParse.c` retoma a estrutura de `tclCompExpr.c`).

**Três tipos de operandos:**

| operando | onde | exemplo |
|---|---|---|
| tipo | palavra nua, à esquerda de uma `}` ou em posição de operando | `int`, `list`, `pt` |
| nome | palavra colada à direita de uma `}` | `{int}age`: o campo `age` |
| valor | entre aspas, entre chavetas, ou depois de `=` | `"MZ"`, `= 0` |

**Operadores**, do mais forte para o mais fraco:

| operador | papel | exemplo |
|---|---|---|
| `.` | aninhamento (associativo à direita) | `list.dict.int` |
| `( )` | argumentos, colados ao nome | `list(3).int`, `dict(x, y?)`, `pt(double)` |
| `=` ou `?=` | valor por omissão de um campo | `{int}age ?= 0` |
| `;` | produto | `{double}x; {double}y` |
| `,` | soma | `{int}i, {string}s` |
| `{ }nome` | agrupamento, com um nome colado à chaveta de fecho | `{{int}x; {int}y}A` |

`;` e `,` têm a mesma precedência: para misturar um produto e uma soma, agrupa-se com chavetas (`{int}a; {{int}b, {string}c}u`).

**Caracteres.** Um nome de tipo só contém letras ASCII, algarismos, `_` e bytes não ASCII, e não começa por um algarismo. A gramática usa ainda `{ } , ; . = " # ( ) * $`. Qualquer outro sinal ASCII (`! % & ' + - / : < > ? @ [ \ ] ^ | ~`) é recusado: `invalid character "!"`, código `TCL PARSE TYPE BADCHAR`, exceto depois de `=` (um valor por omissão) e entre os parênteses de um construtor (`*(file://x, …)`). É esta recusa que torna seguras as anotações locais (`{-v}`, `{is?}`).

Uma definição é **declarativa**: não é feita nenhuma substituição na leitura, exceto num valor por omissão, avaliado em cada construção.

#### 5.3 Campos e contentores

**Enumeração** — uma soma de literais.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# um só literal: uma constante (assinatura de ficheiro)
```

**Soma** — uma só variante de cada vez, que traz o seu nome.

```tcl
type def S {{int}i, {string}s}
set {S}v {i}5
type get $v                    ;# S.i
set {S.s}w abc                 ;# a variante escolhida pelo nome do tipo
```

**Produto** — um `struct`: campos com nome, ordenados, cada um do seu tipo.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# posicional
set {pt}b {age}12 {nom}Jean    ;# etiquetas de campo (§ 8)
$a(nom)                        ;# Jean
```

**Lista** — `list` (extensível), `list.T` (elementos do tipo T), `list(N).T` (exatamente N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

Os elementos de uma `list.T` são atómicos: `lindex`, `foreach`, `$L(i)` devolvem elementos tipados, nunca novamente partidos em palavras. A lista nua `{list}` mantém o comportamento do Tcl.

**Dicionário** — `dict` (qualquer chave), `dict.T` (valores do tipo T), `dict(chaves).T`.

```tcl
type def fiche {dict(nom, age?=0).string}   ;# nom obrigatório; age facultativo, 0 por omissão
set {fiche}e nom Jean                       ;# nom Jean age 0
set {fiche}f age 3                          ;# key "nom" is missing in fiche
```

`dict(x, z?)`: z pode faltar; `dict()`: só a chave vazia. As chaves são sempre texto; só os valores são tipados.

**Ligação `var`** — o NOME de uma variável, seguido em cada leitura.

```tcl
set A 1 ; set B 2
set {list.var.int}L A B        ;# ::A ::B
set A 7
$L(0)                          ;# 7
type deref $L                  ;# 7 2   (um instantâneo, do tipo list.int)
```

**Parâmetro de tipo `{type}T`** — num tipo parametrizado, um parâmetro que recebe um tipo (§ 5.5).

**Ligação binária `*(origem,repr).T`** — um endereço em dados binários, seguido até um valor T (§ 9.5).

#### 5.4 Valores por omissão

`?=` (ou `=`, a escrita antiga, ainda aceite num produto) dá um valor por omissão a um campo. Pode ser um literal, um `[script]` avaliado em cada construção, ou uma `$variável` lida em cada construção.

```tcl
type def t {{int}n ?= [expr {6*7}]; {string}s ?= $::tcl_platform(os)}
set {t}v                       ;# 42 Linux
```

Um campo sem valor por omissão tem de ser dado: `field "x" of pt has no value and no default`.

#### 5.5 Tipos parametrizados

O nome de um `type def` pode trazer parâmetros, declarados como campos: `{type}T` (um tipo), `{int}n`, `{double}f`, `{string}s`. Usam-se no corpo com `$`.

```tcl
type def {pt({type}T)} {{$T}x; {$T}y ?= 0}
set {pt(double)}p {x}1.5       ;# 1.5 0
type get $p                    ;# pt(double)
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}   ;# recursivo
```

#### 5.6 Parâmetros fantasma: as unidades físicas

Um parâmetro ausente do corpo é permitido: não muda os valores, mas distingue as instâncias, que são tipos nominais diferentes. Uma família de grandezas dá assim unidades cujo valor continua a ser um `double`:

```tcl
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def mm   {grandeur(L, 0.001)}
type def m    {grandeur(L)}
type def kg   {grandeur(M)}
type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}   ;# regra não aplicável
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}
set {mm}a 1500
set b {m}$a                    ;# 1.5  (conversão explícita)
set {m}c $a                    ;# type mismatch: expected m but got mm (use an explicit cast)
set d {kg}$a                   ;# unknown way to cast the value from mm to kg
```

Uma regra `type convert` pode visar um tipo ou uma **família** (o nome de um `type def` parametrizado); a lambda recebe então os parâmetros das instâncias (`$de(f)`). `return -code continue` quer dizer «não aplicável». As conversões continuam explícitas.

#### 5.7 Alias fornecidos

`layout/binaliases.tcl` define, para os formatos binários (convenções do Rust e do Zig):

| alias | definição |
|---|---|
| `u8 u16 u32 u64`, `i8 i16 i32 i64` | `int(u,n)`, `int(s,n)` |
| sufixos `be`, `le` (`u16be`, `i32le`) | ordem dos bytes fixa |
| `f32 f64` (e `f32be`…) | `double(32)`, `double(64)` |
| `cstring`, `wstringz`, `latin1z` | `string(utf-8,z)`, `string(utf-16le,z)`, `string(latin1,z)` |
| `ptr32(T)`, `ptr64(T)` | `*(//,u32le,null).T`, `*(//,u64le,null).T` |
| `rel32(T)`, `sub32(T)` | `*(.,i32le).T`, `*(./,u32le).T` |
| `unit`, `opt(T)` | `""` ; `{T}some, {unit}none` |

### 6. Variáveis tipadas e vida de um valor anotado

#### 6.1 O contrato de uma variável tipada

Uma variável declarada com `set {T}v …` tem um **contrato**: cada escrita é verificada, venha ela de `set`, `lappend`, `lset`, `incr`, `dict set`, de uma escrita por caminho (`set v(x) …`) ou de `expr {v = …}`.

```tcl
set {list.int}M 1 2
lappend M 3                    ;# 1 2 3
lappend M x                    ;# expected integer but got "x"
```

**Alargamento automático.** Um refinamento entra onde se espera um tipo mais largo: `int(u,8)` em `int`, `int(s,16)` ou `double`; `double(32)` em `double(64)`. O estreitamento é recusado (exige um cast explícito), tal como entre `bitarray(…,lsb0)` e `bitarray(…,msb0)`.

```tcl
set {int(u,8)}o 200
set {int}w $o                  ;# aceite: type get $w -> int
set {int(u,8)}o2 300           ;# value "300" does not fit in int(u,8)
```

**Nominais.** Um tipo nominal apaga-se perante a sua base (`px` entra em `int` e dá `int`, sem rasto). Dois nominais sem ligação de base nunca são equivalentes (`mm` e `kg`): é precisamente esse o seu papel.

**Casts explícitos.** `set v {T}$x` ou `type cast T $x` convertem segundo, por esta ordem: uma regra `type convert` específica; a cadeia das bases; a variante de uma soma; uma regra de família; as conversões integradas entre escalares. Senão: `unknown way to cast the value from A to B`.

**Levantar o contrato.** `set {}v valor` (§ 2.5).

**Testar.** `expr {v is T}`: verdadeiro se a variável for tipada e o seu tipo couber em `T`; para uma variável sem tipo, verdadeiro se o seu valor for válido para `T`.

#### 6.2 O que mantém a anotação, o que a perde

| operação | tipo | notas |
|---|---|---|
| cópia (`set a $x`), argumento, valor de retorno | mantido | mantidas |
| `lindex`, `foreach`, `$L(i)` sobre uma `list.T` | elemento do tipo `T` | — |
| `string cat $x` (um só argumento) | mantido | mantidas |
| cálculo (`expr`, `string toupper`, `dict set` sobre um valor) | **perdido** | **perdidas** |
| cast `{T}$x` num construtor | alterado | mantidas |
| `set {T}a $x`, variável tipada | `T` | apagadas pelo alargamento |

**Adoção.** Uma variável **sem tipo** que recebe um valor tipado **adota** o seu tipo, e portanto o seu contrato:

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e passa a ser uma variável int
set e hello                    ;# expected integer but got "hello"
```

É o comportamento mais surpreendente do modelo. A armadilha clássica é um `catch … m` reutilizado: se um primeiro `catch` guardar em `m` um valor tipado, um segundo `catch` já não consegue guardar lá a sua mensagem de erro. Usar uma variável por uso, `unset` entre dois usos, ou `set {}m …`. (Ponto em aberto, § 17.)

**Procedimentos.** Um argumento que recebe um valor tipado fica tipado e estrito dentro do procedimento; um argumento comum não muda.

```tcl
proc f {x} {set x 1.5}
set {int}n 3
f $n                           ;# expected integer but got "1.5"
f 3                            ;# 1.5
```

**Variáveis de ciclo.** `foreach`, `lmap`, `dict for`, `dict map` (e o filtro de `dict filter`) esquecem o tipo da sua variável antes de cada atribuição; depois do ciclo, a variável mantém o tipo do último elemento recebido.

#### 6.3 `type deref`: um instantâneo das ligações

`type deref V` devolve `V` com cada ligação `var` substituída pelo valor que a variável tem nesse instante. O resultado tem o tipo «sem ligações» (`list.var.int` → `list.int`), chamado `deref(<tipo>)` para um produto ou uma soma. Só se segue um nível por chamada; uma variável em falta é um erro que nomeia a posição (`type deref: at 2: can't read "::C": no such variable`), sem resultado parcial. Funciona em listas, dicts, produtos, somas e tipos recursivos; um ciclo nunca entra em ciclo infinito.

### 7. Aceder às partes de um valor: os caminhos

#### 7.1 A forma entre parênteses

Um valor tipado lê-se e escreve-se com a sintaxe dos arrays, mas o índice é um **caminho**:

```tcl
type def rect {{{int}x; {int}y}A; {{int}x; {int}y}B}
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# construção (§ 8)
$R(A.x)                                    ;# 1
set R(B) {5 6}                             ;# escrita de um campo inteiro
set R((A,B).x) 10 30                       ;# escrita múltipla
${R(A.x, B.y)}                             ;# leitura, espaços permitidos
```

Os espaços são permitidos em `$R(…)`, `${R(…)}` e numa palavra anotada `{T}R(…)`. Numa palavra nua não anotada (`set R(A.x, A.y) …`), o Tcl corta nos espaços como sempre: escrever sem espaços ou entre aspas.

#### 7.2 A linguagem de caminhos

A linguagem é a da página da wiki do Tcl «A better way to do indexation», analisada segundo o modelo de `expr` (`tclTypePath.c`).

**Operandos:**

| operando | sentido |
|---|---|
| `nome` (palavra nua) | campo, chave de dict, nome de variante ou índice de lista (`3`, `end`, `end-1`): decide o tipo do contentor |
| `'chave'` | sempre uma chave (`'end'` é a chave «end», não um índice) |
| `"etiqueta"` | texto literal copiado para o resultado |

**Operadores**, do mais fraco para o mais forte:

| operador | papel | exemplo |
|---|---|---|
| `,` | coleção (um grupo) | `A,B` |
| `*` (unário) | desdobramento: o grupo funde-se no seu pai | `*0:2,4` |
| `.` | aninhamento; distribui-se sobre um grupo | `A.(x,y)` = `A.x,A.y` |
| `:` | intervalo de lista | `0:end-1` |
| `( )` | agrupamento | `(A,B).x` |

**Forma do resultado.** Uma leitura múltipla devolve listas aninhadas segundo os grupos: `$R((A,B).(x,y))` dá `{1 2} {3 4}`; `$L(0:2,4)` dá `{10 20 30} 50`; `$L(*0:2,4)` dá `10 20 30 50`.

**O caminho não substitui nada.** É o Tcl que substitui `$x`, `[cmd]` e `[(expr)]` em `$R(…)` e nas palavras nuas; um valor substituído é **uma folha**, nunca relida como caminho:

```tcl
set k file.txt ; $D($k)        ;# a chave «file.txt» (não D.file.txt)
set n 1        ; $D(f_$n)      ;# colado a texto, o valor prolonga o nome
set idx {0 2}  ; $L(*$idx)     ;# * desdobra um valor lista: 10 30
set p {[exec ls]} ; $L($p)     ;# nada é executado: bad index "[exec ls]"
```

Entre chavetas e em `${…}`, `$` e `[` são erros: `a path does not substitute "$"…`. Um caminho calculado lê-se **nomeando** a variável com ele: `set ref R($chemin) ; set $ref`.

#### 7.3 Contentor a contentor

| contentor | leitura | escrita |
|---|---|---|
| produto | `$a(nom)`, `$R(A.x)`, `$R(A.(x,y))` | `set a(nom) Léa` (revalidado) |
| soma | `$v(i)`: a variante atual | `set v(i) 6` |
| lista | `$L(2)`, `$L(end)`, `$L(1:3)`, `$L(0,2)` | `set L(1) 25`, `set L(0,2) 5 6` |
| dict | `$D(a)`, `$D('end')`, `$D(a,b)` | `set D(a) 3` |
| bitarray | `$b(3)` (um bit), `$b(1:4)` (uma fatia: um inteiro `bitarray(4,…)`) | `set b(0) 1`, `set b(4:7) 0` |
| enumeração | (escalar: sem caminho) | — |

Ler e depois reescrever o mesmo caminho funciona sempre: `set R(c) [set R(c)]`.

### 8. Construir um valor

#### 8.1 As formas de um construtor de produto

Um produto de *n* campos constrói-se de quatro maneiras, sem adivinhar nada:

| forma | exemplo | regra |
|---|---|---|
| posicional | `set {pt}a Jean 12` | exatamente *n* valores, pela ordem dos campos |
| etiquetas de campo | `set {pt}a {age}12 {nom}Jean` | todos os valores etiquetados; ordem livre; os campos omitidos recebem o seu valor por omissão |
| um só campo | `set {pt}a {pt.nom}Jean` | os outros campos recebem o seu valor por omissão |
| caminho + valores posicionais | `set {pt}a(age,nom) 12 Jean` | um valor por folha do caminho (ou por grupo, § 8.3) |

Juntam-se **nenhum argumento** (`set {pt}a`, todos os campos com o valor por omissão) e **um produto do mesmo tipo** (cópia). Tudo o resto é um erro:

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

A forma nome/valor nua (`set {pt}a nom Jean age 12`) **já não existe**: confundia-se com a posicional sempre que *n* argumentos valiam para *n* campos (§ 16).

#### 8.2 As etiquetas de campo

A etiqueta é a anotação **mais recente** de cada argumento, lida no contexto do produto: um nome de campo prevalece sobre um tipo global com o mesmo nome.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# não muda nada
set {paire}p {nom}jean {age}12          ;# jean 12
set {paire}p {nom}zoe                   ;# zoe 0
set v {age}21 ; set {paire}p {nom}max $v   ;# conta uma etiqueta trazida por uma variável
```

**Tudo ou nada:**

```tcl
set {paire}p {nom}jean 12        ;# can't mix values named by a field annotation ({field}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom}a {taille}3    ;# unknown field "taille" in paire
set {paire}p {nom}a {nom}b       ;# duplicate field "nom" in paire
```

Um valor entre chavetas também se etiqueta: `set {seg}s {a}{1 2} {b}{3 4}`.

#### 8.3 Caminho e valores posicionais

`set {T}v(caminho) v1 v2 …` constrói o produto (e os seus produtos aninhados); os campos nomeados recebem os valores, os outros o seu valor por omissão. O **número** de valores escolhe a repartição, nunca o seu conteúdo:

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# um valor por folha
set {rect}R((A,B).(x,y))     1 2 3 4      ;# idem
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# um valor por elemento do grupo da cabeça
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# um valor para todo o caminho
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# com desdobramento: um valor, plano
```

Cada valor tem a forma que a sua leitura devolveria; os três números só coincidem se as leituras coincidirem.

**A combinação caminho + etiquetas não existe**: `set {rect}R(A) {y}2 {x}1` é um erro (`1 targets but 2 values`). Não está prevista enquanto nenhum uso a pedir.

#### 8.4 Somas e listas

Uma soma constrói-se com a sua variante em etiqueta, ou pelo nome da variante no tipo:

```tcl
set {S}v {i}5                  ;# S.i
set {S.s}w abc                 ;# S.s
set {list.S}L {i}1 {s}a        ;# cada elemento mantém a sua variante
```

Sem etiqueta, a variante é deduzida do valor, pela ordem das variantes: `set {S}u 5` dá `S.i`. Quando várias variantes têm a mesma forma (`{string}a, {string}b`), a primeira serve sempre: para escolher, etiquetar (`{b}y`).

Uma lista recebe os seus elementos um a um: `set {list.int}L 1 2 3`, `set {list(2).int}c 3 4`. Um só argumento é **um** elemento.

### 9. Tipos binários

Um tipo diz, por si mesmo, como se escreve em bytes: não há mais sintaxe a aprender. A leitura e a escrita são feitas por `layout/filebinary.tcl` (carregado com `source`), por cima do núcleo, com uma via rápida em C para as disposições fixas (§ 9.6).

#### 9.1 `bitarray`: um inteiro cujos bits se leem

`bitarray(n, ordem)` é um inteiro de n bits (0 ≤ v < 2^n). A ordem diz de onde parte o índice 0: `lsb0` (por omissão, o bit da direita, como `1 << i`: registos, máscaras) ou `msb0` (o bit da esquerda, como nos esquemas dos RFC). São dois tipos distintos.

```tcl
set {bitarray(8)}b 0xB2                ;# 10110010
set {bitarray(8,msb0)}c 0xB2
lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1
lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0
$b(1:4)                                ;# 9   (um inteiro bitarray(4,lsb0))
set b(0) 1 ; set b(4:7) 0
expr {"b(1:2)" = 3}                    ;# em expr, uma fatia entre aspas
expr {popcount($b)} ; expr {bitlen($b)}
```

#### 9.2 Representações

| tipo | representação |
|---|---|
| `int(u,n)` / `int(s,n)` | inteiro sem sinal / com sinal de n bits (múltiplo de 8, de 8 a 1024) |
| `double(32)`, `double(64)` | vírgula flutuante IEEE |
| `string(raw,N)` | N bytes em bruto |
| `string(ENC,N)` | texto em N bytes, completado com NUL |
| `string(ENC,z)` | texto terminado por NUL (2 bytes em utf-16, ucs-2) |
| `bitarray(n)` | n bits (múltiplo de 8), lidos como inteiro sem sinal |
| `"MZ"` (soma de literais de igual comprimento) | uma constante, verificada |
| `list(N).T`, um produto destes tipos | N elementos; o aninhamento é permitido |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Um texto que não se codifica sem perda (`é` em ascii) ou que não cabe em N bytes é recusado na construção.

**Ordem dos bytes.** Último argumento: `be` ou `le` (`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`). Sem ele, vale o da chamada (`-endian`, little por omissão). Não muda os valores: `int(u,16,be)` e `int(u,16,le)` atribuem-se um ao outro.

**Alinhamento.** Por omissão os produtos são compactos (packed). `-align natural` (ou `-align N`) acrescenta o enchimento do C: cada campo começa num múltiplo do seu alinhamento, o tamanho de um produto é arredondado. Enchimento a zero na escrita, ignorado na leitura exceto com `-strictpad 1`.

#### 9.3 Comandos

```
type binary TIPO                                   ;# a representação, vista do núcleo
type binary {what?}TIPO                            ;# idem
type binary {sizeof?}TIPO ?-align A?               ;# tamanho em bytes (erro se depender dos dados)
type binary {is?}TIPO BYTES ?-exact 1?             ;# 1 se BYTES se descodificar como TIPO, senão 0
type binary decode TIPO BYTES ?-offset N? ?-endian E? ?-consumed VAR? ?-align A? ?-linkdepth D?
type binary encode TIPO VALOR ?-endian E? ?-align A? ?-dedupe 1?
type binary deref LIGAÇÃO ?BYTES?                  ;# segue uma ligação não seguida, um passo
type binary space file|memory|list|get|delete …    ;# espaços com nome
chan binary read  CANAL TIPO ?opções?              ;# o canal avança
chan binary write CANAL TIPO VALOR ?opções?
```

As perguntas (`{what?}`, `{sizeof?}`, `{is?}`) são **anotações locais** sobre o argumento de tipo; as ações são verbos. `file binary` continua a ser um alias do nome antigo.

```tcl
type def mz {"MZ"}
type def entete {{mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve; {cstring}nom}
set o [type binary encode entete [type cast entete {MZ 2 abcd hello}]]
type binary decode entete $o           ;# MZ 2 abcd hello
```

#### 9.4 Nível 2: o que o ficheiro diz de si próprio

Um tamanho, uma quantidade, uma posição ou uma variante podem ser lidos no ficheiro: `$campo` nos argumentos do tipo (um campo **já lido** do produto atual ou de um produto que o englobe), ou um tipo inteiro sem sinal que serve de **prefixo**.

| escrita | sentido |
|---|---|
| `string(utf-8,int(u,16,be))` | texto precedido do seu tamanho em bytes (string de Pascal) |
| `string(raw,$len)`, `string(utf-8,$len - 4)` | N bytes, N calculado a partir de campos lidos |
| `list(int(u,8)).T`, `list($n).T` | lista precedida do seu número de elementos, ou de n elementos |
| `at($off).T` | o valor está na posição `$off` (o cursor não se mexe) |
| `corps($tag)` | a soma `corps`, cuja variante é escolhida pelo campo `tag` |
| `U(tp)`, `R($v)` (parâmetro ligado) | uma soma cuja variante é fixada na definição |

```tcl
type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# nomes de variantes = valores da etiqueta
type def message {{u8}type; {u16be}len; {corps($type)}body}
```

Logo na construção é verificada uma lista cujo tamanho depende de um campo: `set {Batch}b(n,items) 3 {{1 1.0 0}}` é recusado (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).

#### 9.5 Ligações: `*(origem, repr, opções).T`

Um campo contém muitas vezes um endereço, não o valor. O tipo ligação lê o endereço (`repr`, um inteiro) e depois o valor `T` que lá se encontra.

| origem | o endereço conta a partir de |
|---|---|
| `//` (ou `file://`) | o início dos dados |
| `./` | o início da estrutura que o engloba |
| `../` | o início da sua mãe |
| `.` | o próprio campo |
| `//nome/`, `memory://nome/` | um espaço com nome (ficheiro, ou segmentos de memória) |

Opções: `null` (o endereço 0 significa «sem destino»), `null=V`. Com `opt(T)`, um endereço nulo dá `none`, senão `some`: é a diferença entre «nulo» e «inexistente». Na escrita, os destinos são acrescentados no fim, alinhados, e os endereços calculados; `-dedupe 1` só escreve uma vez os destinos idênticos. Um campo ligação igual ao seu valor por omissão escreve-se como uma ligação nula.

**`-linkdepth`** regula o número de ligações seguidas (`Inf` por omissão). Uma ligação não seguida é um inteiro anotado (a posição absoluta do destino); `type binary deref` segue-a um passo. Isto permite ler só o cabeçalho de um ficheiro grande, percorrer uma lista ligada e reescrever um campo no sítio.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8: o destino
```

#### 9.6 Codecs C

`type binary encode` e `decode` tentam primeiro uma via em C, que cobre as disposições fixas: inteiros de 1, 2, 4, 8 bytes, vírgulas flutuantes de 4 e 8 bytes, produtos (com `-align`), `list(N).T` e `list($n).T`. Fora deste domínio (strings, somas, ligações, `at`, campos de bits, tamanhos calculados, `-dedupe`), ou em caso de erro, a camada Tcl toma a vez e dá as suas mensagens. `set ::tcl_typebinaryc 0` desativa a via C. Os bytes e os valores produzidos são idênticos nas duas vias (teste `t51_codec_c`). Medições: § 15.

### 10. Erros

As mensagens seguem o uso do Tcl: nomeiam o valor, o tipo ou o campo em causa. Os códigos `errorCode` próprios dos tipos começam por `TCL TYPE` ou `TCL PARSE TYPE`.

| situação | mensagem (exemplo) | `errorCode` |
|---|---|---|
| carácter proibido num tipo | `invalid character "!" in type "{int}a!"` | `TCL PARSE TYPE BADCHAR` |
| nome de tipo desconhecido num nome de variável | `unknown type "zz"` | `TCL TYPE UNKNOWN` |
| anotação local num nome de variável | `local annotation {-v} cannot declare a variable: a variable name takes a type` | `TCL TYPE LOCALANNOT` |
| anotação local guardada por `set` | `local annotation {-v} cannot be stored by "set": it only applies between a caller and the command it calls` | `TCL TYPE LOCALANNOT` |
| valor inválido para o tipo | `expected integer but got "abc"`; `value "300" does not fit in int(u,8)` | `TCL TYPE INT` (conforme o tipo de base) |
| nenhuma variante serve | `value "jaune" matches no variant of couleur` | `TCL TYPE SUM` |
| tipos nominais diferentes | `type mismatch: expected m but got mm (use an explicit cast)` | `TCL TYPE MISMATCH` |
| nenhuma conversão conhecida | `unknown way to cast the value from mm to kg` | — |
| anotações empilhadas incoerentes | `contradictory annotations px and qq` | `TCL TYPE CONTRADICT` |
| campo desconhecido | `unknown field "zz" in pt`; `pt has no field "zz"` (caminho) | `TCL TYPE FIELD`; `TCL TYPE PATH` |
| campo sem valor nem valor por omissão | `field "y" of rect has no value and no default` | `TCL TYPE FIELD` |
| número de valores errado | `wrong # args: pt needs 2 values; to name fields …` | `TCL WRONGARGS` |
| etiquetas e posições misturadas | `can't mix values named by a field annotation ({field}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
| campo duplicado | `duplicate field "nom" in paire` | `TCL TYPE FIELD` |
| tamanho dependente não respeitado | `field "items" of Batch holds 1 elements, but its size says 3 ($n)` | `TCL TYPE FIELD SIZE` |
| chave de dict em falta ou não permitida | `key "nom" is missing in fiche`; `key "zz" is not allowed in fiche` | `TCL TYPE DICT` |
| aridade de lista | `list(2)<int> needs exactly 2 elements but got 1` | `TCL TYPE LIST` |
| substituição num caminho literal | `a path does not substitute "$": …` | `TCL PARSE INDEX …` |
| índice de lista inválido num caminho | `bad index "zz": must be integer?[+-]integer? or end?[+-]integer?` | `TCL VALUE INDEX` |
| ligação a uma variável desaparecida | `can't access "…": the procedure call that owned this variable has returned` | `TCL TYPE STALE` |
| `type deref` sobre uma ligação partida | `type deref: at 2: can't read "::C": no such variable` | `TCL TYPE DEREF` |
| valor anotado criado fora do analisador | `annotated values can only be created by the parser or by typed assignments` | `TCL VALUE ANNOTATED` |
| camada binária não carregada | `the binary layer is not loaded` | — |

## Parte III — Implementação

### 11. Visão geral

A implementação cabe em **sete ficheiros novos** (cerca de 15 000 linhas de C), copiados para `generic/`, e em **pontos de ligação** numa vintena de ficheiros do núcleo. Os ficheiros novos nunca são alterados por patch: são entregues inteiros; só os ficheiros do núcleo recebem patches.

### 12. O objeto anotado

#### Estrutura

Um valor anotado é um `Tcl_Obj` comum cujo tipo interno é `annotated` (`annotatedObjType`, em `tclAnnotObj.c`):

```
 Tcl_Obj  (typePtr = &annotatedObjType)
 ┌───────────────────────────────────────────────────────────────┐
 │ bytes / length   a string: a do conteúdo V (calculada a        │
 │                  pedido por UpdateStringOfAnnotated)           │
 │ internalRep.twoPtrValue                                        │
 │   ptr1 ──► V   o conteúdo: um Tcl_Obj comum, partilhado        │
 │   ptr2 ──► T   a anotação, numa destas formas:                 │
 │                · uma referência de tipo resolvida (typeRef)     │
 │                · o texto em bruto da anotação (ainda não        │
 │                  resolvida: uma nota, ou um tipo a resolver)    │
 │                · ""   a marca da anotação vazia {}              │
 │                · um estado «annotstate» {T, N}: T resolvido     │
 │                  (ou NULL) e N, a lista das notas, da mais      │
 │                  antiga para a mais recente                     │
 └───────────────────────────────────────────────────────────────┘
```

#### Propriedades

- **Transparente**: a string é a de V. Para qualquer comando que não conheça as anotações, o valor é V.
- **Imutável e partilhado**: o estado `{T, N}` é ele próprio um objeto imutável; `Tcl_DuplicateObj` partilha-o. Anotar, desempilhar ou retirar uma nota produz um objeto novo.
- **Sem custo sem notas**: enquanto não houver nota, `ptr2` é só T.
- **Lista abstrata** (Tcl 9, `TCL_OBJTYPE_V2`): `lindex`, `llength`, `lrange`, `foreach`, `in`, `lappend`, `lset`… passam pelos procedimentos de lista do objeto (`AnnotListLength`, `AnnotListIndex`, `AnnotListSlice`, `AnnotListReverse`, `AnnotListGetElements`, `AnnotListSetElement`, `AnnotListReplace`, `AnnotListIn`). Os elementos de uma `list.T` são devolvidos tipados; um escalar anotado é uma lista de um elemento que é ele próprio.

#### Rotinas do `Tcl_ObjType`

| rotina | papel |
|---|---|
| `FreeAnnotated` | liberta V e T (e a célula «si próprio» de um escalar, tabela `SelfCell`) |
| `DupAnnotated` | copia V, partilha T ou o estado `{T, N}` |
| `UpdateStringOfAnnotated` | a string de V |
| `SetAnnotatedFromAny` | **recusa**: um valor anotado nasce do analisador ou de uma atribuição tipada, nunca de uma conversão (`TCL VALUE ANNOTATED`) |

#### Funções principais (`tclAnnotObj.c`)

`TclAnnotateValue` (pôr uma anotação: cast ou nota), `TclAnnotSettle` (o desempilhamento de um construtor, § 2.3), `TclAnnotResolve`, `ClassifyAnnot` (tipo, variante, desconhecido, local, inválido; o contexto antes dos tipos globais), `TclAnnotInfo`, `TclAnnotPending`, `TclAnnotPopTop`, `TclAnnotUnwrap` (o conteúdo nu, para as conversões do núcleo), `TclAnnotDisplay` (`list<int>@mm.hello`), e os comandos `::tcl::annotate`, `::tcl::annotraw`, `::tcl::annotlocal`, `note`.

### 13. No analisador e no compilador

#### O prefixo `{x}palavra`

No Tcl 9 original, uma palavra entre chavetas seguida de outros caracteres é um erro de sintaxe (`extra characters after close-brace`), exceto `{*}`. O prefixo de anotação ocupa portanto uma escrita **que não tinha sentido**: nenhum script válido muda de sentido.

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand`: uma palavra `{texto}resto` produz um token **`TCL_TOKEN_ANNOT`** (512, `tcl.h`) que traz o texto da anotação, seguido dos tokens do resto da palavra. Uma palavra anotada `{T}nome(...)` vai até ao parêntese equilibrado, espaços incluídos, numa linha.
- `tclCompile.c`, `TclCompileTokens`: se o resto da palavra for literal (`TclAnnotSuffixIsLiteral`), a palavra compila em `::tcl::annotraw texto palavra` (uma nota em bruto, sem substituição); senão em `::tcl::annotate texto valor`.
- `tclCompCmdsSZ.c`, `TclCompileSetCmd`: um `set` cujo nome está anotado passa por `TclSetAnnotatedCmd`; uma anotação local escrita sobre o valor (`set x {-v}$mot`) emite uma chamada a `::tcl::annotlocal`, que lança o erro (`TclAnnotLocalWordText`).
- `tclVar.c`, `Tcl_SetObjCmd`: um primeiro argumento anotado delega em `TclSetAnnotatedCmd` (`tclTypedVar.c`), o construtor.

#### O índice `$nome(...)`: `TCL_TOKEN_INDEX`

- `tclParse.c`, `Tcl_ParseVarName`: no `(` que se segue a um nome de variável, chama **`TclParseVarIndex`**, que lê o índice até ao `)` equilibrado: parênteses aninhados, etiquetas `"..."` emparelhadas, substituições `$x`, `[cmd]`, `[(expr)]`, barras invertidas; chavetas recusadas. Se o índice não estiver bem formado segundo esta gramática, há um **recurso** exato à regra do Tcl 9 (`info complete {a "c $dd("}` continua verdadeiro).
- Só quando a sintaxe estendida é usada é que os componentes são envolvidos num token **`TCL_TOKEN_INDEX`** (1024, `tcl.h`): um índice válido em Tcl 9 mantém exatamente os seus tokens. As palavras nuas não são afetadas: `puts a(b c)` continua a ser duas palavras.
- `${nome(...)}`: o nome inteiro é literal; `Tcl_ObjGetVar2` e `INST_LOAD_STK` (via `TclTypedVarGetElementByName`) leem o caminho quando a variável é um escalar tipado.
- `tclTest.c`: `testparser` mostra o token «index».

#### Bytecode

| instrução | onde | papel |
|---|---|---|
| `INST_INDEX_CONCAT` («indexConcat», 6 bytes: número `uint1` + máscara `uint4`) | `tclCompile.c` (`TclCompileIndexTokens`, chamado por `TclCompileVarSubst` e `TclPushVarName`), `tclExecute.c` | concatena as partes de um índice num objeto **pathindex** que guarda os troços substituídos (`TclPathIndexNew`): um valor substituído continua a ser uma folha do caminho |
| `INST_TYPE_FORGET` | `tclCompCmds.c` (`dict for`, `dict map` compilados), `tclExecute.c` | esquece o tipo de uma variável de ciclo antes da atribuição |

Via não compilada: `TclSubstTokens` (`SubstIndexTokens`) produz o mesmo objeto pathindex.

#### Outros pontos de ligação do núcleo

| ficheiro | alteração |
|---|---|
| `tclBasic.c` | `TclTypesInit`: cria `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` toma a via lenta para um valor anotado; `INST_FOREACH_STEP` esquece o tipo; leitura e escrita de elementos tipados (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` e `TclTypedVarAfterStore` à volta de cada escrita: o contrato |
| `tclProc.c` | `TclTypedVarBindArg`: ligação tipada dos argumentos |
| `tclCmdAH.c`, `tclDictObj.c` | variáveis de ciclo (`TclTypedVarForgetByName`); `TclAnnotUnwrap` nas leituras de dict |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | conversões do núcleo sobre o conteúdo (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`): um valor anotado nunca é convertido no sítio |
| `tclListObj.c` | `lappend` pela `replaceProc` de uma lista abstrata; `lindex` com vários índices sem achatar |
| `tclCompExpr.c` | operador `is` (lexema `IS_TYPE`, `IsTypeAhead`), atribuições `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. Os ficheiros acrescentados

| ficheiro | linhas | conteúdo |
|---|---|---|
| `tclTypes.c` | 4 500 | tipologia, `type def/alias/get/cast/convert/deref/binary`, validação, alargamento, casts, codecs C |
| `tclTypeParse.c` | 2 700 | analisador e compilador das expressões de tipos (modelo: `tclCompExpr.c`) |
| `tclTypeGeneric.c` | 1 400 | tipos parametrizados, instâncias, parâmetros fantasma, refinamentos binários |
| `tclAnnotObj.c` | 2 150 | o objeto anotado, o estado `{T, N}`, a resolução, `note` |
| `tclTypedVar.c` | 1 350 | variáveis tipadas, construtores (`TclSetAnnotatedCmd`, `BuildProductEx`, etiquetas de campo) |
| `tclTypePath.c` | 2 900 | linguagem de caminhos, leitura, escrita, construção por caminho, objeto pathindex |
| `tclVarRef.c` | 360 | ligações `var` (nome canónico, acesso, ligações caducadas) |
| `tclTypes.h`, `tclTypesInt.h` | 550 | estruturas (`TclType`, `TclTypeField`, `TclBinLink`…) e interfaces |

Do lado Tcl: `layout/filebinary.tcl` (camada binária: `type binary decode/encode/deref/space`, `chan binary`) e `layout/binaliases.tcl` (alias).

## Compatibilidade, conceção, limites

### 15. Compatibilidade e desempenho

**Nenhum script válido muda de sentido.** As duas sintaxes acrescentadas ocupam escritas que eram erros: `{x}palavra` («extra characters after close-brace») e um índice `$nome(...)` estendido, que recorre exatamente à regra do Tcl 9 assim que não está bem formado. Verificações:

- testes do Tcl `parse`, `parseExpr`, `parseOld`, `subst` (incluindo 257 testes de `testparser`), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`, `append`, `lset`, `dict`, `info`: resultados idênticos aos da base;
- equivalência com um Tcl 9 original em 18 000 casos gerados (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`): nenhum resultado válido alterado;
- AddressSanitizer sem erros; memória estável ao longo de 1 000 000 de construções.

**Os tipos não são inimigos da velocidade.** Medições no exemplo `cstruct` (10 000 medidas, `-align natural`):

| operação | camada Tcl | codec C | Tcl clássico escrito à mão, sem controlo |
|---|---|---|---|
| codificação (`type binary encode`) | 333 ms | 1,3 ms | ≈ 3,3 ms (`binary format`; 330 µs medidos para 1 000) |
| descodificação (`type binary decode`) | 485 ms | 3,0 a 3,7 ms | 7,3 a 10,5 ms (`binary scan`) |

O codec C, que além disso **verifica** cada valor contra o seu tipo, é 2 a 3 vezes mais rápido do que o Tcl clássico escrito à mão, e 130 a 250 vezes mais rápido do que a camada Tcl genérica.

### 16. Escolhas de conceção

| escolha | razão |
|---|---|
| `{x}palavra` ocupa um erro de sintaxe | compatibilidade total, nenhuma regra de citação nova |
| sem empilhamento colado `{a}{b}$x` | ambíguo com `{int}{300}`; empilha-se anotando um valor anotado |
| só um construtor resolve um tipo | `{T}$x` fora de um construtor não custa nada e nunca falha; a verificação faz-se onde o valor é guardado |
| anotações locais reconhecidas pelos seus caracteres | um nome de tipo não pode conter `-` nem `?`: nenhuma colisão possível |
| `type def` nominal, `type alias` estrutural | distinguir unidades (`mm`, `kg`) sem mudar os valores |
| alargamento automático, estreitamento explícito | nenhuma perda de informação silenciosa |
| conversões explícitas entre nominais | `mm` → `m` exige `{m}$a` ou `type cast` |
| forma nome/valor nua suprimida | `set {png}p chunks $l` lia-se de duas maneiras; agora os nomes estão escritos no código-fonte (`{chunks}…` ou `p(chunks)`), nunca adivinhados nos dados |
| etiquetas de campo: tudo ou nada | uma mistura seria uma forma posicional disfarçada |
| o caminho não substitui nada | um dado que contenha `[script]` nunca é executado pelo analisador de caminhos (a dupla substituição desaparece); mesma regra que um array |
| valores substituídos atómicos | `$D($k)` lê a chave `$k`, mesmo que contenha `.` ou `,` |
| caminho + etiquetas não previsto | sem necessidade demonstrada; o uso dirá |

### 17. Limites e pontos em aberto

**A corrigir ou decidir**

- **Adoção por uma variável sem tipo.** `set e [lindex $L 1]` torna `e` tipada e estrita. Daí a armadilha `catch … m` (§ 6.2), e o erro que escapa é a mensagem original, o que o torna difícil de compreender. A decidir: adoção silenciosa, adoção sem contrato, ou mensagem explícita.
- **`;` e `,` misturados sem chavetas** (`{int}a; {int}b, {int}c`) são aceites sem erro e produzem um campo anónimo; a regra da especificação recusa-os.
- **Ler uma variante que não é a atual** de uma soma dá `unknown way to cast the value from S to string` em vez de uma mensagem que nomeie a variante.
- **Variantes da mesma forma**: sem etiqueta, a primeira é escolhida sem aviso.
- `set {paire}s {aut.nom}x` (o campo de **outro** produto) dá `wrong # args`; uma mensagem própria seria mais clara.
- `lset` com uma lista de índices que passa por `TclLsetList` guarda uma referência a mais (fuga, sem falha).

**Ainda por fazer**

- Valor «inexistente» em geral: `opt(T)` cobre as ligações binárias, não há forma geral.
- `union`, campos de bits partilhados, lista «até ao fim do ficheiro» (a recusa de `union` é deliberada).
- Anotação de comentário `{#}` ao nível do analisador, anotação da primeira palavra (`{c}proc`), anotações de declaração (`proc p {{?}{…} {!}arg}`).
- Notas por elemento de uma lista tipada (hoje a nota aplica-se à lista inteira); apagar todas as notas com `{@}$x`.
- Codecs C para somas, ligações e tamanhos calculados; cache do tipo analisado.
- Banco de testes C sobre maqueta (`typeparse/test/`) por atualizar; exemplos `arbre_var.tcl` e `defauts_calcules.tcl` por corrigir.

## Anexos

### A. Instalação e testes

O protótipo parte do Tcl `core-9-0-branch` com o ramo do TIP 759. A partir da raiz da árvore Tcl (MSYS2 / MinGW64 ou Unix):

```
bash typeparse/maj.sh [typeparse.zip]   # extrai, copia as fontes de tipos, aplica os patches do núcleo,
                                        # recompila e compara cada tests_tclsh/tNN_*.tcl com o seu .out
```

- As fontes de tipos são entregues inteiras em `typeparse/` e copiadas para `generic/`; só os patches do núcleo listados em `noyau/ORDRE.txt` são aplicados, sem fuzz (`-F0`), sendo um patch já aplicado detetado e saltado.
- Opções de `maj.sh`: `--dry-run`, `--yes`, `--no-build`, `--no-test`, `--commit "mensagem"`.
- Notas de cada patch: `noyau/LISEZMOI.md`. Exemplos executáveis: `exemples/`.

### B. Glossário

| termo | sentido |
|---|---|
| anotação | texto entre chavetas colado à frente de uma palavra: `{x}palavra` |
| nota | anotação não resolvida, que segue o valor |
| tipo | anotação resolvida na tipologia conhecida |
| anotação local | anotação cujo texto não pode ser um tipo (`{-v}`, `{is?}`); só vive entre quem chama e o comando |
| construtor | o que guarda um valor e resolve as suas anotações: `set {T}v`, ligação de argumentos, `lassign`, `foreach`, atribuição em `expr` |
| contrato | o tipo de uma variável, verificado em cada escrita |
| adoção | uma variável sem tipo toma o tipo do valor recebido |
| cast | conversão explícita para um tipo (`{T}$x` num construtor, `type cast`) |
| nominal | tipo criado por `type def`, distinto da sua definição; apaga-se perante a sua base |
| refinamento | tipo de base restringido, com uma representação binária (`int(u,8)`) |
| alargamento | passagem automática de um refinamento para um tipo mais largo |
| produto | `struct`: campos com nome e ordenados |
| soma | uma variante entre várias, com nome |
| variante | um dos ramos de uma soma |
| etiqueta de campo | anotação que nomeia o campo de um argumento: `{age}12` |
| caminho | índice de um valor tipado: `R(A.(x,y))` |
| pathindex | objeto que traz um índice e as suas partes substituídas |
| ligação | `var.T` (para uma variável) ou `*(…).T` (para um endereço binário) |
| parâmetro fantasma | parâmetro de um tipo que não muda os valores mas distingue as instâncias |

### C. Correspondências

| aqui | C | Rust | LLVM |
|---|---|---|---|
| produto | `struct` | `struct` | `{ T1, T2 }` (packed: `<{ … }>`) |
| soma | (`union` + discriminante à mão) | `enum` com dados | — |
| enumeração (soma de literais) | `enum` | `enum` sem dados | — |
| `list(N).T` | `T[N]` | `[T; N]` | `[N x T]` |
| `int(u,16)` / `u16` | `uint16_t` | `u16` | `i16` (sem sinal) |
| `int(s,32)` / `i32` | `int32_t` | `i32` | `i32` |
| `double(64)` / `f64` | `double` | `f64` | `double` |
| `cstring` | `char *` terminado por NUL | `CStr` | `i8*` |
| `ptr32(T)` | `T *` (32 bits) | `*const T` | `ptr` |
| `opt(T)` | ponteiro que pode ser `NULL` | `Option<T>` | — |
| tipo nominal (`type def mm double`) | `typedef` (sem controlo) | newtype `struct Mm(f64)` | — |
| `-align natural` / packed | alinhamento por omissão / `__attribute__((packed))` | `#[repr(C)]` / `#[repr(packed)]` | alinhamento / `<{ }>` |
