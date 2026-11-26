# type repeat T N ?valeur? -> list.T ; type repeat -into L N ?valeur? -> L (v17t).
# Toujours en cases quand l'élément en a une forme ; zéro par défaut.
type def {-compact}octets list.int(u,8)
type def pt {{double}x; {double}y}
type def {-compact}pts list.pt
proc essai {nom script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERR $r" }
    puts "$nom : $r"
}
essai "int(u,8) 5 1"        {set s [type repeat int(u,8) 5 1]; list $s [type get $s]}
essai "-into octets 4"      {set s [type repeat -into octets 4]; list $s [type get $s]}
essai "double 3"            {type repeat double 3}
essai "pt 2 {1 2}"          {set p [type repeat pt 2 {1 2}]; list $p [type get $p]}
essai "-into pts 2 {1 2}"   {set p [type repeat -into pts 2 {1 2}]; list $p [type get $p]}
essai "list.int 3 {1 2}"    {set l [type repeat list.int 3 {1 2}]; list $l [type get $l]}
essai "-into list.int 3 7"  {type repeat -into list.int 3 7}
essai "int 3 7"             {set l [type repeat int 3 7]; list $l [type get $l]}
essai "string 2"            {type repeat string 2}
essai "string 2 a"          {type repeat string 2 a}
essai "-into list(3).int 4" {type repeat -into {list(3).int} 4 1}
essai "-into int 3"         {type repeat -into int 3}
essai "int(u,8) 3 300"      {type repeat int(u,8) 3 300}
essai "arguments"           {type repeat -into octets}
essai "arguments 2"         {type repeat}
essai "inconnu"             {type repeat nope 3}
essai "{octets} signature"  {proc f {{octets}x} {llength $x}; f [type repeat -into octets 10 1]}
essai "écriture"            {set s [type repeat int(u,8) 10 1]; set s(2::2) 0; set s}
essai "lmap après"         {set s [type repeat int(u,8) 20 3]; lrange [lmap v $s {($v*2)}] 0 2}
essai "somme"              {set s [type repeat int(u,8) 100 3]; set c 0; foreach v $s {incr c $v}; set c}
