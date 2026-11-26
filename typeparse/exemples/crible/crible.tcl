# crible.tcl -- le crible d'Ératosthène en Tcl, trois représentations.
#
# Même algorithme que les programmes de autres/ : N octets à 1, on raye
# les multiples de chaque premier (i*i, i*i+i, ...), on compte ce qui reste.
# Chaque étape est chronométrée à part (créer, cribler, compter), meilleur
# de 5. L'étape « cribler » commence par la copie du tableau (la variable
# reçue est partagée avec l'appelant), comme dans les autres langages.
#
#   ./tclsh crible.tcl ?N?
#   TCL_TYPE_NOVECLOOP=1 ./tclsh crible.tcl    (comptage sans vectorisation)

proc best {k script} {
    set b 1e18
    for {set j 0} {$j < $k} {incr j} {
	set t [lindex [time {uplevel 1 $script}] 0]
	if {$t < $b} {set b $t}
    }
    return [expr {$b / 1000.}]
}

fconfigure stdout -encoding utf-8
set N [expr {$argc > 0 ? [lindex $argv 0] : 1000000}]

# 1. Tcl nu : une liste d'objets, un lset par multiple
proc creer_nu {N} { lrepeat $N 1 }
proc cribler_nu {s N} {
    lset s 0 0; lset s 1 0
    for {set i 2} {$i*$i < $N} {incr i} {
	if {[lindex $s $i]} {
	    for {set j [expr {$i*$i}]} {$j < $N} {incr j $i} {lset s $j 0}
	}
    }
    return $s
}

# 2. list.int : une liste typée d'objets, une tranche à pas par premier
proc creer_int {N} { type repeat int $N 1 }

# 3. octets en cases : type repeat range toujours en cases (un octet par
#    élément, sans Tcl_Obj) ; la même tranche, écrite dans les cases
proc creer_octets {N} { type repeat int(u,8) $N 1 }

proc cribler_tranches {s N} {
    set s(0:1) {0 0}
    for {set i 2} {$i*$i < $N} {incr i} {
	if {$s($i)} {set s([expr {$i*$i}]:end:$i) 0}
    }
    return $s
}

# le comptage : une boucle ordinaire, que Tcl vectorise (corps pur)
proc compter {s} {
    set c 0
    foreach v $s {incr c $v}
    return $c
}

puts [format "%-34s %7s %7s %7s %7s" "Tcl (N = $N)" créer cribler compter total]
foreach {nom creer cribler} {
    "Tcl nu, liste + lset"            creer_nu     cribler_nu
    "Tcl, list.int + tranches"        creer_int    cribler_tranches
    "Tcl, octets en cases + tranches" creer_octets cribler_tranches
} {
    set tc [best 5 [list $creer $N]]
    set s0 [$creer $N]
    set tk [best 5 [list $cribler $s0 $N]]
    set s [$cribler $s0 $N]
    set tn [best 5 [list compter $s]]
    puts [format "%-34s %7.1f %7.1f %7.1f %7.1f  -> %d" $nom \
	    $tc $tk $tn [expr {$tc + $tk + $tn}] [compter $s]]
}
