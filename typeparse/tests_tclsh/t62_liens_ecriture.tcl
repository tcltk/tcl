# t62 : liens var : nom canonique à chaque écriture ; lecture d'un lien seul.
set {int}A 1; set {int}B 2; set {int}C 3
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-40s %s %s" $l $rc $m] }
t "construction"           {set {list.var.int}L A B; set L}
t "lset L 1 C"             {lset L 1 C; set L}
t "lappend L C"            {lappend L C; set L}
t "lappend L 5"            {lappend L 5}
t "lappend L {}"           {lappend L {}}
t "set L(0) C"             {set L(0) C; set L}
t "lset L 0 {a b}"         {lset L 0 {a b}}
t "lecture \$L(1)"         {set L(1)}
proc p {} { set {list.var.int}M A; lset M 0 B; lappend M C; return $M }
t "dans une procédure (locaux absents)" {p}
namespace eval ns { variable D 4 }
t "lappend L ns::D"        {lappend L ns::D; set L}
set A 1 ; set B 2
set {list.var.int}L A B
set A 7
puts "L(0)=$L(0)"
set r [lindex $L 0]; puts "r=$r type=[type get $r] set r=[set $r]"
set {var.int}R A; puts "R=$R  set \$R=[set $R]  type=[type get $R]"
lset L 1 C; set C 3; puts "lset: $L  L(1)=$L(1)"
set L(0) 8; puts "écriture par le lien: A=$A"
proc p {} { set x 1; set {list.var.int}M x; set x 5; list $M $M(0) [catch {set [lindex $M 0]} e] $e }
puts [p]
