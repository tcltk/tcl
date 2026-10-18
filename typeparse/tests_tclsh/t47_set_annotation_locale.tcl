# set refuse une annotation locale écrite sur la valeur : {-v} ne vit
# qu'entre l'appelant et l'appelé ; une annotation héritée reste tolérée.
proc attempt {name script} {
    puts "$name: [catch {uplevel 1 $script} m] $m"
}
set mot abc
attempt top      {set x {-v}$mot}
attempt question {set y {what?}abc}
attempt typed    {set {int}w {-v}5}
attempt nominal  {set z {mm}$mot}
proc p {} {set mot abc; set x {-v}$mot; return $x}
attempt inproc   {p}
attempt evalstr  {eval {set y {-v}$mot}}
# héritée : l'annotation arrive par un argument, set ne l'a pas écrite
proc q {a} {set b $a; set c [string length $a]; return "$b $c"}
puts "heritee: [q {-v}$mot]"
# une annotation de type valide fonctionne toujours
set {int}n {int}5
puts "valide: $n [type get $n]"
