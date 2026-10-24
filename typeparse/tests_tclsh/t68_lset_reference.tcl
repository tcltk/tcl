# t68 : lset / lpop sur une liste typée : pas de référence en trop (fuite), et
#   lpop garde le type de la variable. Mesure de mémoire sous Linux seulement (/proc) :
#   ailleurs (Windows), elle n'est pas faite. Rien n'est écrit sans fuite, pour que la sortie
#   attendue soit la même partout ; une fuite trouvée écrit une ligne « FUITE » (échec).
proc rss {} {
    if {[catch {open /proc/self/status} f]} {return -1}
    set d [read $f]; close $f; regexp {VmRSS:\s+(\d+)} $d -> k; return $k
}
proc mesure {label body} {
    proc __b {} "set {list.int}L 1 2 3 4; set {list.list.int}M {1 2} {3 4}
                 for {set k 0} {\$k < 100000} {incr k} {$body}"
    __b
    set a [rss]; __b; set b [rss]
    if {$a < 0 || $b - $a < 2000} return
    puts [format "%-28s FUITE %s Ko" $label [expr {$b-$a}]]
}
mesure "lset L 0 9"          {lset L 0 9}
mesure "lset L {1} 8"        {lset L {1} 8}
mesure "eval lset"           {eval {lset L 2 7}}
mesure "lset M 0 1 9"        {lset M 0 1 9}
mesure "eval lset M 1 0 8"   {eval {lset M 1 0 8}}
mesure "lpop + lappend"      {lpop L; lappend L 4}
mesure "lpop M 0 + lset"     {lpop M 0; lset M 0 end+1 2}
set {list.int}L 1 2 3 4
foreach s {{lset L 0 9} {lset L {1} 8} {lpop L} {lpop L 0} {eval {lpop L}}} {
    eval $s; puts [format "%-28s %-8s %s" $s $L [type get $L]]
}
puts [catch {set L(0) x} m]\ $m
