# t72 : type def {-compact}T : les valeurs du type sont toujours en cases, les
#   nombres normalisés à la construction ; type is {compact?} ; conversions sur
#   une copie (la forme compacte reste) ; refus motivés.
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-34s %s %s" $l $rc $m] }
type def point {{double}x; {double}y}
type def {-compact}polygone list.point
t "type is {compact?}polygone"   {type is {compact?}polygone}
t "type is {compact?}list.point" {type is {compact?}list.point}
t "type is {compact?}point"      {type is {compact?}point}
set {polygone}P {0 0} {4 0} {4 3}
t "construit, normalisé"         {set P}
t "type is {compact?}\$P"        {type is {compact?}$P}
t "string length"                {list [string length $P] [type is {compact?}$P]}
t "string reverse"               {list [string reverse $P] [string reverse $P] [type is {compact?}$P]}
t "string range"                 {list [string range $P 0 8] [type is {compact?}$P]}
t "dict size (refus)"            {list [catch {dict size $P}] [type is {compact?}$P]}
t "set P(1.x) 7"                 {set P(1.x) 7; list $P [type is {compact?}$P]}
t "lappend {1 2}"                {lappend P {1 2}; list $P [type is {compact?}$P]}
t "lset 0 {5 5}"                 {lset P 0 {5 5}; list $P [type is {compact?}$P]}
t "lappend invalide"             {lappend P {a b}}
t "contenu puis conversion"      {set c [tcl::unsupported::content $P]; string length $c; type is {compact?}$P}
t "list.point ordinaire"         {set {list.point}L {0 0}; list $L [type is {compact?}$L]}
t "cast vers polygone"           {set Q [type cast polygone {{1 1} {2 2}}]; list $Q [type is {compact?}$Q]}
t "valeur ordinaire"             {type is {compact?}{1 2}}
type def {-compact}vf list.double(32)
t "double(32) arrondi"           {set {vf}F 0.1 0.5; set F}
type def {-compact}octets list.int(u,8)
t "int(u,8)"                     {set {octets}B 1 2 0x10; list $B [catch {lappend B 256} m] $m}
t "refus : list.string"          {type def {-compact}noms list.string}
t "le nom refusé reste libre"    {type def noms list.string; type get [set {noms}n a b]}
t "un produit (v15t)"            {type def {-compact}pp {{double}x; {int(s,32)}k}; type is {compact?}pp}
t "liste de ce produit"          {set {list.pp}Q {1 2} {3e0 4}; list $Q [type is {compact?}$Q]}
t "champ liste d'un produit"     {type def poly {{int(u,32)}n; {list($n).pp}pts}; set {poly}R 1 {{0x10 5}}
                                  list $R [type is {compact?}$R(pts)] [type is {compact?}$R]}
t "refus : produit avec chaîne"  {type def {-compact}pq {{double}x; {string}s}}
t "refus : option inconnue"      {type def {-zz}u list.double}
t "refus : alias"                {type alias {-compact}w list.double}
t "refus : famille"              {type def {-compact}{v({int}n)} {list($n).double}}
t "question inconnue"            {type is {zz?}point}
t "sans question"                {type is point}
t "export : llvm"                {type {llvm!}export point}
t "export : sans cible"          {type export point}
