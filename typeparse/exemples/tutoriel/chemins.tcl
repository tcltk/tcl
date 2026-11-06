# chemins.tcl -- le coût d'un accès, en ns, boucle vide déduite (le meilleur
# de 5 tours de 200 000 accès)
#
#   tclsh chemins.tcl
#
# Une liste ordinaire lue par lindex/lassign, et les mêmes données typées lues
# par chemin ($p(x), $s(a.x), ...) ou par les commandes de liste.

type def pt   {{double}x; {double}y}
type def seg  {{pt}a; {pt}b}
type def poly list.pt

proc run {what n} {
    set {seg}s {1.0 2.0} {3.0 4.0}
    set {pt}p 1.0 2.0
    set {poly}g {1.0 2.0} {3.0 4.0}
    set l {{1.0 2.0} {3.0 4.0}}
    set q {1.0 2.0}
    set t0 [clock microseconds]
    switch $what {
	vide    {for {set i 0} {$i < $n} {incr i} {}}
	lindex  {for {set i 0} {$i < $n} {incr i} {set v [lindex $l 0 0]}}
	lassign {for {set i 0} {$i < $n} {incr i} {lassign $q x y}}
	px      {for {set i 0} {$i < $n} {incr i} {set v $p(x)}}
	expr    {for {set i 0} {$i < $n} {incr i} {set v [expr {$p(x) * $p(y)}]}}
	lassT   {for {set i 0} {$i < $n} {incr i} {lassign $p x y}}
	sax     {for {set i 0} {$i < $n} {incr i} {set v $s(a.x)}}
	g0x     {for {set i 0} {$i < $n} {incr i} {set v $g(0.x)}}
	sab     {for {set i 0} {$i < $n} {incr i} {set v $s(a,b)}}
	sxy     {for {set i 0} {$i < $n} {incr i} {set v $s(b.(x,y))}}
	g01     {for {set i 0} {$i < $n} {incr i} {set v $g(0:1)}}
    }
    expr {([clock microseconds] - $t0) * 1000.0 / $n}
}

proc best {what} {
    set b 1e9
    for {set r 0} {$r < 5} {incr r} {
	set t [run $what 200000]
	if {$t < $b} {set b $t}
    }
    return $b
}

set base [best vide]
foreach {what label} {
    lindex  "lindex \$l 0 0 (liste ordinaire)"
    lassign "lassign \$q x y (liste ordinaire)"
    px      "\$p(x)"
    expr    "expr {\$p(x) * \$p(y)}"
    lassT   "lassign \$p x y (point typé)"
    sax     "\$s(a.x)"
    g0x     "\$g(0.x)"
    sab     "\$s(a,b)"
    sxy     "\$s(b.(x,y))"
    g01     "\$g(0:1)"
} {
    puts [format "%-36s %6.0f ns" $label [expr {[best $what] - $base}]]
}
