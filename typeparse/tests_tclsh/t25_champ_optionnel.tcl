type def p2 {{string}nom ?= "?"; {int}age ?= 3; {string}ville = "Lyon"}
set {p2}a {p2.nom}Jean
puts $a
set {p2}b
puts $b
type def p3 {{string}nom ?= [string toupper abc]; {int}n ?= $::nn}
set nn 7
set {p3}c; puts $c
puts [catch {type def p4 {{int}x ?= abc}} m]; puts $m
type def d {dict(x, z?=0).int}
set {d}e x 1; puts $e
