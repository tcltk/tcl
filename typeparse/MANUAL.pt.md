# Manual dos tipos anotados — capítulos

*(Tradução para português de `MANUEL.md`, que continua a ser a referência.
Este ficheiro reúne os capítulos do manual à medida que são escritos; as
notas de patch ficam em `noyau/LISEZMOI.md`. Os identificadores dos exemplos
mantêm-se como no original francês, para corresponderem aos scripts de
exemplo e às mensagens de erro.)*

## Dict e produto: duas maneiras de descrever uma estrutura

Para quem vem do C: uma **lista** Tcl é um array, um **dict** é uma
estrutura de chaves livres, um **tipo produto** é um `struct` declarado. Os
dois últimos parecem-se muito, e passa-se muitas vezes de um para o outro.

```tcl
type def pt     {{string}nom = "?"; {int}age ?= 0}     ;# produto
type def fiche  {dict(nom, age?=0).string}              ;# dict tipado
```

### O que têm em comum

| | produto `pt` | dict `fiche` |
|---|---|---|
| construção por nome | `set {pt}a {nom}Jean {age}12` ou `set {pt}a(nom,age) Jean 12` | `set {fiche}e nom Jean` |
| acesso a um campo | `$a(nom)` | `dict get $e nom`, `$e(nom)` |
| valor por omissão de um campo/chave | `age ?= 0` (ou `=`) | `age?=0` |
| campo desconhecido | `unknown field "zz" in pt` | `key "zz" is not allowed in fiche` |
| valor serializado | lista Tcl | lista Tcl chave/valor |

O valor por omissão escreve-se da mesma forma: `?=` usa-se nos dois casos, e
um valor por omissão pode ser um literal, `[script]` (calculado em cada
construção) ou `$variável`. Num produto, o antigo `=` continua aceite.

### O que os distingue

* **Campos presentes.** Num produto, todos os campos existem sempre: um
  campo omitido recebe o seu valor por omissão, e um campo sem valor por
  omissão tem de ser dado (`field "x" of pt has no value and no default`).
  Num dict, uma chave sem `?` é obrigatória (`key "nom" is missing in fiche`)
  e uma chave marcada com `?` pode estar ausente.
* **Ordem.** Um produto tem campos ordenados; também se constrói de forma
  **posicional** (`set {pt}b Jean 12`, como a inicialização de um struct C).
  Um dict não tem forma posicional.
* **Um só campo.** Um produto aceita o prefixo `{T.campo}`:
  `set {pt}c {pt.nom}Jean` fixa `nom`, os outros campos recebem o seu valor
  por omissão (`Jean 0`). É a mesma escrita que `{Val.i}5` para uma soma.
* **Tipos dos campos.** Cada campo de um produto tem o seu próprio tipo
  (`{string}nom; {int}age`); um `dict(...)` tem um só para todos os valores
  (`.string`).
* **Tamanho.** Um produto tem um número fixo de campos, fixado na definição.

### Nomear os campos: etiquetas ou caminho (acabou o nome/valor nu)

A forma nome/valor nua (`set {pt}a nom Jean age 12`) foi **suprimida**:
confundia-se com a posicional sempre que *n* argumentos valiam para *n*
campos. Um produto constrói-se agora de três maneiras, sem adivinhar nada:

* **posicional**: tantos valores quantos os campos, por ordem
  (`set {pt}b Jean 12`);
* **etiquetas de campo**: `set {pt}a {age}12 {nom}Jean`. A etiqueta é a
  anotação mais recente de cada argumento; é lida no contexto do produto,
  por isso um nome de campo prevalece sobre um tipo global com o mesmo nome
  (`type def age int` não muda nada), tal como o nome de uma variante numa
  soma. Ordem livre; os campos omitidos recebem o seu valor por omissão.
  **Tudo ou nada**: misturar etiquetas e valores posicionais é um erro
  (`can't mix values named by a field annotation …`), e uma etiqueta
  desconhecida também (`unknown field "taille" in pt`). Um valor que traz a
  sua etiqueta numa variável conta (`set v {age}12; set {pt}a {nom}Jean $v`);
* **caminho**: `set {pt}a(age,nom) 12 Jean`, útil quando os nomes são
  calculados ou os valores estão entre chavetas.

O dict, esse, mantém a sua forma chave/valor: um dict não tem forma
posicional.

### Escolher

* Um conjunto de campos **conhecido e estável**, com tipos variados:
  produto.
* Chaves **livres ou facultativas**, do mesmo tipo: dict.
* Um formato com várias formas possíveis (PNG, GIF…): uma **soma** de
  produtos (ver `exemples/png_somme.tcl`).

## bitarray: um inteiro cujos bits se leem

`bitarray(n, ordem)` é um inteiro de n bits (0 ≤ v < 2^n). A ordem diz de
onde parte o índice 0:

| | `lsb0` (por omissão) | `msb0` |
|---|---|---|
| bit de índice 0 | o da DIREITA (`1 << 0`) | o da ESQUERDA |
| uso | registos, máscaras, C | esquemas de protocolos (RFC) |
| índice de um bit se a largura mudar | inalterado | deslocado |

Tomemos o byte `0xB2`, escrito `10110010`:

    set {bitarray(8)}b 0xB2
    set {bitarray(8,msb0)}c 0xB2
    lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1   (lsb0)
    lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0   (msb0)

**Leitura.** `$b(i)` dá o bit (0 ou 1). `$b(i:j)` dá os bits i a j como UM
inteiro, do tipo `bitarray(j-i+1, ordem)`: `$b(1:4)` vale 9 em lsb0;
`$c(0:3)` vale 11 em msb0 (os quatro bits da esquerda, `1011`).

**Escrita.** `set b(0) 1`, `set b(4:7) 0`. Em `expr`, uma fatia escreve-se
entre aspas, porque o `:` pertence ao analisador de `expr`:
`expr {"b(1:2)" = 3}`. Um valor que não cabe nos bits visados é recusado.

**Duas ordens, dois tipos.** `bitarray(8,lsb0)` e `bitarray(8,msb0)` não se
misturam: atribuir um ao outro é um `type mismatch`. O `bitarray(8)` por
omissão é `bitarray(8,lsb0)`.

**Funções.** `popcount(x)`: número de bits a 1; `bitlen(x)`: largura de um
bitarray, caso contrário número de bits do inteiro.

**Dentro de outros tipos.** Um campo de produto (`{bitarray(8)}flags`), um
elemento de lista (`list.bitarray(4)`) ou uma variável de procedimento são
bitarrays como os outros: `set r(flags.7) 1`, `set L(2.3)`.

**Parece um dict?** Não: nem chaves nem campos, um só inteiro. Para
descrever os campos de um cabeçalho binário, combina-se um produto de
bitarrays (ver o capítulo anterior).

Exemplos completos: `exemples/bitarray_exemples.tcl` (byte «packed» de um
GIF e flags TCP em `msb0`, permissões Unix em `lsb0`).

## type deref: tirar um instantâneo das ligações

Uma ligação `var` contém o NOME de uma variável e segue o seu valor em cada
leitura. `type deref` substitui cada ligação pelo valor que a variável tem
NESSE INSTANTE: o resultado deixa de mudar depois.

    set A 1;  set B 2;  set C 3
    set {list.var.int}refs A B C
    set v [type deref $refs]            ;# 1 2 3   (list.int)
    set B 20
    puts $refs(1)                       ;# 20  : a ligação segue B
    puts $v                             ;# 1 2 3 : o instantâneo fica fixo

* O resultado tem o tipo «sem ligações»: `list.var.int` passa a `list.int`,
  e manipula-se como qualquer lista de inteiros.
* Uma variável em falta interrompe tudo com um erro que nomeia a posição:
  `type deref: at 2: can't read "::C": no such variable`. Não há resultado
  parcial e o valor original não é tocado.
* Só se segue um nível: se uma variável visada contiver ela própria
  ligações, estas são conservadas; um segundo `type deref` segue-as.
* Funciona em listas, dicts, produtos e SOMAS, aninhados ou não, e em tipos
  RECURSIVOS (árvores como `tree(var)`). A variante de uma soma é
  conservada.
* Os grafos tratam-se por níveis: se os vizinhos de um nó forem ligações
  (`{list.var.node}next`), `type deref` dá o nó com os vizinhos substituídos
  pelos seus valores, que ainda contêm ligações; um segundo `type deref`
  desce um nível. Um ciclo nunca faz, portanto, entrar em ciclo infinito.
* O tipo do resultado é um tipo próprio, chamado `deref(<tipo original>)`
  (`deref(tree(var))`, `deref(mix)`), calculado uma vez e depois
  reutilizado: dois instantâneos do mesmo tipo têm o mesmo tipo.

    type def mix {{int}a, {var.int}b, {string}s}
    set {mix}s1 {b}A
    type get [type deref $s1]           ;# deref(mix).b

    set {tree(var)}T A {{B {}} {C {}}}  ;# árvore cujos valores são ligações
    set d [type deref $T]               ;# 1 {{2 {}} {3 {}}}
    puts $d(children.0.value)           ;# 2

Ver também `MANUAL-deref.md` (em inglês): o tipo de ligação `var` e
`type deref`, com exemplos em `exemples/deref_manual.tcl`.

## Soma e `union` do C: não confundir

Para quem vem do C: uma **soma** (`{{A}x, {B}y}`) parece-se com uma `union`,
mas não é a mesma coisa.

| | `union` do C | soma |
|---|---|---|
| o que contém | os **mesmos bytes**, lidos de várias formas | **uma só** variante, que traz o seu nome |
| quem sabe qual é válida | ninguém: o programador tem de saber | o próprio valor (`type get` dá `S.x`) |
| ler a variante errada | permitido, leem-se bytes quaisquer | impossível: acede-se à variante que se tem |
| tamanho num ficheiro | o da maior vista | o da variante escolhida |
| escolha da variante | nenhuma, não existe | por um campo já lido (`S($tag)`) ou pelo seu nome (`S(x)`) |

Consequências:
* **Não há `union` neste modelo**, de propósito: mantêm-se as salvaguardas.
  Uma soma não pode ser lida «de través».
* Quando o formato não diz qual variante é válida (a `Tcl_ObjInternalRep` do
  Tcl, cujo discriminante é o endereço `typePtr`, exterior à union), a
  variante é escolhida **na chamada**: `Tcl_ObjAs(twoPtrValue)` (ver
  `exemples/tcl_obj.tcl`). É quem chama que sabe, como em C, mas o tipo
  continua verificado.
* Uma soma em que várias variantes têm a **mesma forma** não é ambígua desde
  que a variante seja escolhida pelo nome (`S(y)`): já não há nada a
  adivinhar.

### Existir, ser nulo, não existir

Não confundir três situações para um valor apontado por uma ligação (tipo
`*(...)`, a seguir):
* **presente**: a ligação designa um valor;
* **presente mas nulo ou vazio**: o valor existe, vale `0`, `{}` ou `""`;
* **inexistente**: a ligação é nula (offset 0, por exemplo), não há nada a
  ler.

Um valor por omissão de campo (`= {}`) não distingue os dois últimos. Uma
soma com uma variante sem valor (`{{T}some, {}none}`) distingue-os. Ainda
não está implementado; ver a nota de conceção.

## Tipos binários: ler e escrever bytes (`type binary`, `chan binary`)

Um campo diz, pelo seu TIPO, como se escreve em bytes. Nada mais a aprender:
é o princípio de `bitarray(16)`, que já traz a sua largura. O `.` só serve
para COMPOR (`list(3).T`); a representação está nos parênteses do tipo.

    type def mz {"MZ"}                           ;# uma constante: soma de um literal
    type def entete {
        {mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve;
        {string(utf-8,z)}nom
    }

| tipo | representação |
|---|---|
| `int(u,n)` / `int(s,n)` | inteiro sem sinal / com sinal de n bits (múltiplo de 8, de 8 a 1024) |
| `double(32)` `double(64)` | vírgula flutuante IEEE |
| `string(raw,N)` | N bytes em bruto |
| `string(ENC,N)` | texto em N bytes, completado com NUL |
| `string(ENC,z)` | texto terminado por NUL (2 bytes em utf-16, ucs-2) |
| `bitarray(n)` | n bits (múltiplo de 8), lidos como inteiro sem sinal |
| `"MZ"` (soma de literais de igual comprimento) | uma constante, verificada pelo tipo |
| `list(N).T`, um produto destes tipos | N elementos; o aninhamento é permitido |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Um texto
que não se codifica sem perda (`é` em ascii) ou que não cabe em N bytes é
recusado.

**Ordem dos bytes.** Último argumento: `be` (big-endian, ordem de rede) ou
`le`: `int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`. Sem ele, vale
o da chamada (`-endian`, little por omissão). Não muda os valores:
`int(u,16,be)` e `int(u,16,le)` são intercambiáveis.

**Alargamento automático.** Um refinamento entra onde se espera um tipo mais
largo:

    set {int(u,8,be)}u 3
    set {int}n $u                    ;# ok: todo o int(u,8) é um int
    set {double}x $u                 ;# ok
    set {int(s,16)}s $u              ;# ok: 0..255 cabe em -32768..32767
    set {int(u,8)}v $n               ;# recusado: estreitamento (cast explícito)
    set {list.int(u,8)}L $liste_int  ;# recusado também para as listas

Um tipo definido por `type def port int(u,16)` continua nominal: `port`
entra num int, mas um simples `int(u,16)` não entra num `port`.

**Alinhamento.** Por omissão os produtos são compactos (packed), como os
cabeçalhos de rede e PE/ELF. `-align natural` (ou `-align N`) acrescenta o
enchimento do C e do LLVM: cada campo começa num múltiplo do seu alinhamento
(o seu tamanho, limitado a 8 ou N), o tamanho de um produto é arredondado ao
seu alinhamento. Enchimento a zero na escrita, ignorado na leitura exceto
com `-strictpad 1`.

    type binary entete                           ;# a representação, vista do núcleo
    type binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    type binary encode entete $valeur ?-endian E? ?-align A?
    chan binary read  $canal entete ?-endian E? ?-align A?   ;# o canal avança
    chan binary write $canal entete $valeur ?-endian E? ?-align A?
    type binary {sizeof?}entete ?-align A?

A leitura e a escrita estão escritas em Tcl (`layout/filebinary.tcl`) por
cima do núcleo: `type binary` descreve, descodifica e codifica bytes em
memória, `chan binary` lê e escreve num canal. O nome `file binary` (o
antigo) continua disponível como alias.

**`type binary`, interrogações e verbos.** As perguntas sobre um tipo
escrevem-se com uma anotação local no argumento de tipo (o `?` final
distingue-a de qualquer nome de tipo); as ações sobre bytes continuam a ser
verbos:

    type binary entete                          ;# a representação (forma nua)
    type binary {what?}$T                       ;# idem
    type binary {sizeof?}$T ?-align A?          ;# tamanho em bytes (erro se variável)
    type binary {is?}$T $octets ?-exact 1?      ;# 1 se $octets se descodifica como T, senão 0
    type binary decode T $octets ...            ;# lê os bytes em memória
    type binary encode T $valeur ...            ;# idem, encode
    type binary deref ...                       ;# idem, deref
    type binary space ...                       ;# idem, space (espaços com nome)

`{is?}` aceita por omissão um prefixo válido (reconhecer um cabeçalho);
`-exact 1` exige que todos os bytes sejam consumidos. Uma opção inválida, um
tipo desconhecido ou sem representação binária continuam a ser erros: só a
falha da descodificação dá 0. Uma anotação desconhecida que termina em `?` é
um erro (`unknown question "oups?"`); qualquer outra anotação é ignorada. Um
tipo com o nome de um verbo (`type def decode ...`) continua descrito por
`type binary decode`, desde que não se siga outro argumento. `decode`,
`encode`, `deref` e `space` exigem que `layout/filebinary.tcl` esteja
carregado (caso contrário: `the binary layer is not loaded`).

**Alias curtos.** `layout/binaliases.tcl` dá nomes breves às representações
habituais (convenção do Rust e do Zig): `u8 u16 u32 u64`, `i8 i16 i32 i64`,
`f32 f64`, com os sufixos `be`/`le` (`u16be`, `i32le`, `f64be`), e
`cstring`, `wstringz`, `latin1z`. São `type alias`: a biblioteca serve também
de cábula sobre os formatos binários. Os `i8…i64` daqui têm sinal; os `iN`
do LLVM não têm sinal, por isso não há correspondência exata (os arrays
`[N x T]` e as estruturas packed correspondem a `list(N).T` e aos produtos).


### Nível 2: tamanhos, quantidades, posições e variantes lidos no ficheiro

Sempre a mesma sintaxe: `$campo` nos argumentos do tipo, como os parâmetros
dos tipos parametrizados, ou um tipo inteiro sem sinal que serve de prefixo.

| escrita | sentido |
|---|---|
| `string(utf-8,int(u,16,be))` | texto precedido do seu tamanho em bytes (string de Pascal) |
| `string(raw,$len)` | N bytes, sendo N o valor do campo `len` |
| `string(utf-8,$len - 4)` | uma expressão Tcl sobre os campos já lidos |
| `list(int(u,8)).T` | lista precedida do seu número de elementos |
| `list($n).T` | n elementos, sendo n um campo |
| `at($off).T` | o valor está na posição `$off` do ficheiro (o cursor não se mexe) |
| `corps($tag)` | a soma `corps`, cuja variante é escolhida pelo campo `tag` |

    type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# nomes de variantes = etiquetas
    type def message {{u8}type; {u16be}len; {corps($type)}body}
    type def exe {{mz}magic = MZ; ...; {u32}lfanew; {at($lfanew).pe}nt}

Regras: um `$campo` tem de designar um campo **já lido** do produto atual ou
de um produto que o englobe (`type binary` verifica-o); um campo de produto
lê-se `$h(off)`. Na escrita, um tamanho ou uma quantidade que não
corresponde ao campo de que depende é um erro, e a etiqueta tem de ser a da
variante. `type binary {sizeof?}` recusa um tipo cujo tamanho depende dos
dados. Exemplo completo (PE, ELF, mensagens): `exemples/niveau2.tcl`.

Exemplos: `exemples/layout_plat.tcl`, `exemples/niveau2.tcl`. Limites: nem
unions nem campos de bits partilhados, nem lista «até ao fim do ficheiro».

#### Variante escolhida: `S(nome)` e parâmetro de tipo

Uma soma `S` também pode ter a sua variante **escolhida na definição**, sem
campo seletor no ficheiro:

```tcl
type def rep_twoPtr {{ptr}ptr1; {ptr}ptr2}
type def U {{rep_long}l, {rep_twoPtr}tp}

type def Q {{U(tp)}s}               ;# a variante tp, escrita por extenso
type def {R({string}v)} {{u8}k; {U($v)}s}   ;# ou dada por um parâmetro
type binary decode {R(tp)} $octets
```

`U(tp)` é um tipo com uma só variante: o seu tamanho é conhecido
(`type binary {sizeof?}`), o valor lido é `U(tp).tp` e não há nada a
adivinhar, mesmo que várias variantes tenham a mesma forma (o que antes
tornava o valor ambíguo). `U($campo)` mantém o seu sentido: variante
escolhida por um campo já lido. Se `$v` for um parâmetro ligado do tipo,
aplica-se o primeiro caso. Exemplo: `exemples/tcl_obj.tcl`
(`Tcl_ObjAs(twoPtrValue)`). Teste: `t36_somme_choisie.tcl`.

#### O tipo ligação `*(origem,repr).T`

Um campo de ficheiro contém muitas vezes um endereço, não o valor. O tipo
ligação lê o endereço (`repr`, um inteiro) e depois o valor `T` que lá se
encontra; o valor do campo é o do destino.

    type def pe {{u8}a; {u8}b}
    type def H {{u8}m; {*(//,u32le).pe}r}

Origens, como num sistema de ficheiros: `//` (ou `file://`) início dos
dados, `./` início da estrutura que o engloba, `../` a sua mãe, `.` o
próprio campo (ver abaixo). Opções: `null` (o endereço 0 significa «sem
destino»), `null=V`. Com `opt(T)` (ver `unit`), um endereço nulo dá `none`,
senão `some`: é a diferença entre «nulo» e «inexistente».

Os alias genéricos (`type alias {ptr32({type}T)} {*(//,u32le).$T}`) dão
nomes curtos: `ptr32(T)`, `ptr64(T)`, `rel32(T)`, `sub32(T)` em
`layout/binaliases.tcl`. Na escrita, os destinos são acrescentados no fim,
alinhados, e os endereços são calculados. Os tipos recursivos, `memory://` e
os espaços com nome são descritos mais abaixo. Detalhes:
`noyau/CONCEPTION-lien.md`.

**O ponto `.`: o endereço é relativo à posição do campo.** `./` marca o
*início da estrutura*; `.` marca *o próprio campo* (o sítio onde o endereço
está escrito). É a origem dos deslocamentos «a partir daqui», frequentes nos
formatos com deslocamentos relativos (`rel32`):

    type def H {{u8}m; {u8}pad; {*(.,u8,null).rec}r}   ;# o campo r está no offset 2
    type binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# mesmo resultado, mas com ./
    type binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Memorando: `//` início dos dados · `./` início da estrutura · `../` início
da estrutura mãe · `.` posição do campo. A notação `.` mantém-se curta de
propósito; um nome mais expressivo só será considerado depois de usado.

#### Até onde seguir as ligações: `-linkdepth`

Uma ligação pode ser seguida de imediato ou deixada como está. A opção de
chamada `-linkdepth` regula o número de ligações seguidas (como o alcance de
uma radiação: um raio gama atravessa tudo, um raio pouco penetrante para na
pele):

    type binary decode H $d                   ;# 1 {7 8}   Inf, por omissão: seguir tudo
    type binary decode H $d -linkdepth 1      ;# 1 {7 8}   um nível de ligações
    set h [type binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      nenhuma ligação seguida
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

Uma ligação não seguida é um inteiro anotado (posição absoluta do destino
nos dados). A cópia, `lindex` e `string cat` mantêm a anotação; um cálculo
perde-a, e o resultado já não pode ser seguido. Segue-se uma ligação a
pedido, um passo de cada vez:

    type binary deref $h(r) $d                ;# 7 8   (-linkdepth 0 por omissão: um só passo)

O que se ganha: ler só o cabeçalho de um ficheiro grande; percorrer uma
lista ligada ou uma árvore (o tipo pode ser recursivo:
`type def N {{u8}x; {*(//,u8,null).opt(N)}next}`); localizar uma ligação
danificada; modificar um campo no sítio, porque `type binary encode`
reescreve tal como está um endereço não seguido em vez de reorganizar o
ficheiro. Um ciclo só é erro com `Inf`.


## Espaços com nome

Uma ligação pode designar um espaço registado pelo seu nome em vez dos dados
da chamada.

```tcl
set sp [type binary space file $octets -name lib]          ;# valor anotado do tipo space
type binary space memory $octets {{4096 4 2}} -name mem    ;# segmentos {endereço offset tamanho}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ é equivalente
type def M {{*(memory://mem/,u32le,null).rec}p}
type binary deref $h0(r)                                    ;# sem DADOS: o espaço basta
type binary space list | get NOME | delete NOME
```

O nome por omissão é calculado (`space1`, …); o comando de criação regista o
espaço; um nome já usado é um erro. `//nome/` exige um espaço sem segmentos,
`memory://nome/` um espaço com segmentos; um endereço fora dos segmentos é
um erro. Não se pode escrever um destino real num espaço com nome: só o
endereço de fronteira (`-linkdepth 0`) é reescrito.

## Escrever uma ligação nula

Um campo ligação que tem um valor por omissão e cujo valor é igual a esse
valor por omissão escreve-se como uma ligação nula (o endereço «nulo» da
ligação, nenhum destino é escrito). A leitura faz o inverso: endereço nulo →
valor por omissão. Sem valor por omissão (e sem `opt`), ler uma ligação nula
continua a ser um erro.

## -dedupe: partilhar destinos idênticos

`type binary encode TIPO VALOR -dedupe 1` (também `write`) só escreve uma vez
os destinos de ligações cujos bytes são idênticos; as ligações em causa
recebem o mesmo endereço. Por omissão (`-dedupe 0`) cada destino é escrito.
Só se partilham destinos sem ligação aninhada (os endereços internos de um
destino dependem da sua posição); a releitura dá o mesmo valor.

## Tamanho dependente de um campo: verificado na construção

Uma lista cujo número de elementos é uma expressão sobre outros campos
(`{list($n).Sample}items`, `list($n - 1).u8`) é verificada logo na
construção do produto: `set {Batch}b(n,items) 3 {{1 1.0 0}}` é recusado
(`field "items" of Batch holds 1 elements, but its size says 3 ($n)`). Os
campos omitidos contam com o seu valor por omissão. A verificação não cobre
as atribuições campo a campo (`set b(n) 3`: um estado intermédio é
legítimo); `type binary encode` volta a verificar na escrita. Os tamanhos em
bytes das strings (`string(raw,$len)`) não são verificados na construção.
