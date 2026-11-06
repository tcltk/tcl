# Étape 2 : le même programme, typé.
#
# Les données ont un nom et une forme ; les procédures disent ce qu'elles
# prennent et ce qu'elles rendent. Les appels ne changent pas.

namespace eval poly {
    type def point    {{double}x; {double}y}
    type def polygone list.point
    type def boite    {{double}xmin; {double}ymin; {double}xmax; {double}ymax}
}

proc {point}poly::point {{double}x; {double}y} {( $x, $y )}

# L'aire (formule du lacet)
proc {double}poly::aire {{polygone}pg} {(
    a = 0.0;  q = [lindex $pg end];
    foreach p $pg {( a = $a + $q(x) * $p(y) - $q(y) * $p(x);  q = $p )};
    abs($a) / 2
)}

# Le périmètre ; -ouvert : une ligne brisée, sans le dernier côté
proc {double}poly::perimetre {{flag}-ouvert; {polygone}pg} {(
    l = 0.0;
    q = $ouvert ? [lindex $pg 0] : [lindex $pg end];
    foreach p [lrange $pg $ouvert end] {( l = $l + hypot($p(x) - $q(x), $p(y) - $q(y));  q = $p )};
    $l
)}

# Le point P est-il dans le polygone ? (lancer de rayon)
proc {int}poly::contient {{polygone}pg; {point}P} {(
    dedans = 0;  q = [lindex $pg end];
    foreach p $pg {(
        dedans = ($p(y) > $P(y)) != ($q(y) > $P(y))
                 && $P(x) < ($q(x) - $p(x)) * ($P(y) - $p(y)) / ($q(y) - $p(y)) + $p(x)
                 ? !$dedans : $dedans;
        q = $p
    )};
    $dedans
)}

proc {polygone}poly::deplace {{polygone}pg; {double}dx; {double}dy ?= 0.0} {
    lmap p $pg {( $p(x) + $dx, $p(y) + $dy )}
}

# La boîte englobante
proc {boite}poly::boite {{polygone}pg} {(
    (xmin, ymin) = [lindex $pg 0];  xmax = $xmin;  ymax = $ymin;
    foreach p $pg {(
        xmin = min($xmin, $p(x));  xmax = max($xmax, $p(x));
        ymin = min($ymin, $p(y));  ymax = max($ymax, $p(y))
    )};
    ($xmin, $ymin, $xmax, $ymax)
)}
