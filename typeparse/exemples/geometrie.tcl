# geometrie.tcl --
#
# FR : Un petit morceau de tcllib math::geometry, avec des types. Là où
#      math::geometry range tout à plat ({x1 y1 x2 y2}), chaque objet a ici
#      son type : un segment n'est pas une droite, même s'ils ont la même
#      forme. Tous les calculs sont des corps expr (TIP 759) : {( ... )}.
#      Chaque ligne affichée donne le résultat attendu, calculable de tête.
#
# EN : A small piece of tcllib math::geometry, with types. Where
#      math::geometry keeps everything flat ({x1 y1 x2 y2}), each object here
#      has its own type: a segment is not a line, even with the same shape.
#      All computations are expr bodies (TIP 759): {( ... )}. Each printed
#      line shows the expected result, which can be checked by hand.
#
# Rappels / reminders :
#   - un point est UN élément (règle B) : ses champs par chemin, $P(x), ou
#     tous à la fois, (x, y) = $P ;
#   - les champs sont des double, mais 4 reste la chaîne "4" : les divisions
#     passent par double() pour ne pas devenir entières.

type def point     {{double}x; {double}y}
type def vecteur   {{double}dx; {double}dy}
type def segment   list(2).point           ;# deux extrémités
type def droite    list(2).point           ;# deux points de la droite
type def polyligne list.point
type def polygone  list.point              ;# fermé implicitement
type def triangle  list(3).point
type def carre     list(4).point
type def rectangle {{point}min; {point}max}  ;# aligné sur les axes (tcllib)
type def cercle    {{point}centre; {double}rayon}
type def bezier    list(4).point           ;# Bézier cubique
type def inter     {"aucune", {point}en}   ;# résultat d'une intersection

# FR : un triangle, un carré sont des polygones : conversions explicites.
# EN : a triangle, a square are polygons: explicit conversions.
type convert triangle polygone {{x} {return $x}}
type convert carre    polygone {{x} {return $x}}

namespace eval geo {}

# ---------------------------------------------------------------------------
# Vecteurs / vectors

proc geo::vec {a b} {(
    A is point = $a;  B is point = $b;
    V is vecteur = ($B(x) - $A(x), $B(y) - $A(y));
    $V
)}
proc geo::scal {u v} {(                      # produit scalaire / dot product
    U is vecteur = $u;  V is vecteur = $v;
    $U(dx)*$V(dx) + $U(dy)*$V(dy)
)}
proc geo::vectoriel {u v} {(                 # produit vectoriel (scalaire en 2D)
    U is vecteur = $u;  V is vecteur = $v;
    $U(dx)*$V(dy) - $U(dy)*$V(dx)
)}
proc geo::norme {u} {( sqrt([geo::scal $u $u]) )}
proc geo::rot90 {u} {(
    U is vecteur = $u;
    R is vecteur = (-$U(dy), $U(dx));
    $R
)}
proc geo::distance {a b} {( [geo::norme [geo::vec $a $b]] )}

# ---------------------------------------------------------------------------
# Droites et segments / lines and segments

# FR : projeté orthogonal de P sur la droite D.
proc geo::projete {p d} {(
    P is point = $p;  D is droite = $d;
    A is point = $D(0);
    u = [geo::vec $A $D(1)];  w = [geo::vec $A $P];
    t = [geo::scal $w $u] / double([geo::scal $u $u]);
    H is point = ($A(x) + $t*$u(dx), $A(y) + $t*$u(dy));
    $H
)}

# FR : distance de P au segment S : le projeté, ramené sur le segment.
proc geo::distance_segment {p s} {(
    P is point = $p;  S is segment = $s;
    A is point = $S(0);
    u = [geo::vec $A $S(1)];  w = [geo::vec $A $P];
    t = max(0.0, min(1.0, [geo::scal $w $u] / double([geo::scal $u $u])));
    H is point = ($A(x) + $t*$u(dx), $A(y) + $t*$u(dy));
    [geo::distance $P $H]
)}

# FR : intersection de deux segments : {en}point, ou "aucune".
proc geo::intersection {s1 s2} {(
    S is segment = $s1;  T is segment = $s2;
    A is point = $S(0);
    r = [geo::vec $A $S(1)];  s = [geo::vec $T(0) $T(1)];  q = [geo::vec $A $T(0)];
    d = double([geo::vectoriel $r $s]);
    t = $d == 0 ? -1 : [geo::vectoriel $q $s] / $d;
    u = $d == 0 ? -1 : [geo::vectoriel $q $r] / $d;
    R is inter = 0 <= $t && $t <= 1 && 0 <= $u && $u <= 1
                 ? ($A(x) + $t*$r(dx), $A(y) + $t*$r(dy))
                 : "aucune";
    $R
)}

# ---------------------------------------------------------------------------
# Polylignes et polygones / polylines and polygons

proc geo::longueur {l} {(
    L is polyligne = $l;  s = 0.0;
    [for {set i 1} {$i < [llength $L]} {incr i} {
        set s [($s + [geo::distance [lindex $L $i-1] [lindex $L $i]])]
    }];
    $s
)}

proc geo::perimetre {p} {(
    P is polygone = $p;  n = [llength $P];  s = 0.0;
    [for {set i 0} {$i < $n} {incr i} {
        set s [($s + [geo::distance [lindex $P $i] [lindex $P [(($i+1) % $n)]]])]
    }];
    $s
)}

# FR : formule du lacet / EN: shoelace formula
proc geo::aire {p} {(
    P is polygone = $p;  n = [llength $P];  s = 0.0;
    [for {set i 0} {$i < $n} {incr i} {
        set a [lindex $P $i];  set b [lindex $P [(($i+1) % $n)]]
        set s [($s + $a(x)*$b(y) - $b(x)*$a(y))]
    }];
    abs($s) / 2.0
)}

# FR : P est-il dans le polygone ? (lancer de rayon / ray casting)
proc geo::dedans {p q} {(
    P is point = $p;  Q is polygone = $q;  n = [llength $Q];  c = 0;
    [for {set i 0} {$i < $n} {incr i} {
        set a [lindex $Q $i];  set b [lindex $Q [(($i+1) % $n)]]
        if {[(($a(y) > $P(y)) != ($b(y) > $P(y)) && $P(x) < $a(x)
                + ($P(y) - $a(y)) * ($b(x) - $a(x)) / double($b(y) - $a(y)))]} {
            set c [(1 - $c)]
        }
    }];
    $c ? "oui" : "non"
)}

# FR : rectangle englobant / EN: bounding box
proc geo::englobant {p} {(
    P is polygone = $p;
    xs = [lmap q $P {lindex $q(x)}];  ys = [lmap q $P {lindex $q(y)}];
    R is rectangle = (([tcl::mathfunc::min {*}$xs], [tcl::mathfunc::min {*}$ys]),
                      ([tcl::mathfunc::max {*}$xs], [tcl::mathfunc::max {*}$ys]));
    $R
)}

# FR : le carré dont S est la diagonale (d'après le segment2rect de Florent).
proc geo::segment2rect {s} {(
    S is segment = $s;
    (xa, ya) = $S(0);  (xc, yc) = $S(1);
    xm = ($xa + $xc) / 2.0;  ym = ($ya + $yc) / 2.0;     # milieu
    xb = $xm - ($yc - $ym);  yb = $ym + ($xc - $xm);     # M + MC tourné de 90°
    xd = 2*$xm - $xb;        yd = 2*$ym - $yb;
    R is carre = (($xa, $ya), ($xb, $yb), ($xc, $yc), ($xd, $yd));
    $R
)}

# ---------------------------------------------------------------------------
# Triangles et cercles / triangles and circles

proc geo::gravite {t} {(
    T is triangle = $t;
    (ax, ay) = $T(0);  (bx, by) = $T(1);  (cx, cy) = $T(2);
    G is point = (($ax + $bx + $cx) / 3.0, ($ay + $by + $cy) / 3.0);
    $G
)}

proc geo::circonscrit {t} {(
    T is triangle = $t;
    (ax, ay) = $T(0);  (bx, by) = $T(1);  (cx, cy) = $T(2);
    d = 2.0 * ($ax*($by - $cy) + $bx*($cy - $ay) + $cx*($ay - $by));
    a2 = $ax**2 + $ay**2;  b2 = $bx**2 + $by**2;  c2 = $cx**2 + $cy**2;
    O is point = (($a2*($by - $cy) + $b2*($cy - $ay) + $c2*($ay - $by)) / $d,
                  ($a2*($cx - $bx) + $b2*($ax - $cx) + $c2*($bx - $ax)) / $d);
    C is cercle = ($O, [geo::distance $O $T(0)]);
    $C
)}

# FR : intersection d'un cercle et d'une droite : 0, 1 ou 2 points.
proc geo::cercle_droite {c d} {(
    C is cercle = $c;  D is droite = $d;
    H is point = [geo::projete $C(centre) $D];
    h = [geo::distance $C(centre) $H];
    u = [geo::vec $D(0) $D(1)];
    k = sqrt(max(0.0, $C(rayon)**2 - $h**2)) / [geo::norme $u];
    L is list.point = $h > $C(rayon) ? [list]
        : $k == 0 ? [list $H]
        : [list [list [($H(x) - $k*$u(dx))] [($H(y) - $k*$u(dy))]] \
                [list [($H(x) + $k*$u(dx))] [($H(y) + $k*$u(dy))]]];
    $L
)}

# ---------------------------------------------------------------------------
# Courbe de Bézier cubique (polynômes de Bernstein) / cubic Bézier curve

proc geo::bezier_point {b t} {(
    B is bezier = $b;
    (x0, y0) = $B(0);  (x1, y1) = $B(1);  (x2, y2) = $B(2);  (x3, y3) = $B(3);
    s = 1.0 - $t;
    P is point = ($s**3*$x0 + 3*$s**2*$t*$x1 + 3*$s*$t**2*$x2 + $t**3*$x3,
                  $s**3*$y0 + 3*$s**2*$t*$y1 + 3*$s*$t**2*$y2 + $t**3*$y3);
    $P
)}

# ===========================================================================
# Les exemples / the examples

proc montre {n libelle valeur attendu} {
    set t [type get $valeur]
    if {$t ne ""} {set valeur "$valeur   ($t)"}
    puts [format "%-4s %-40s %-34s attendu : %s" $n $libelle $valeur $attendu]
}

set {droite}Ox    {0 0} {10 0}
set {droite}Diag  {0 0} {1 1}
set {segment}S10  {0 0} {10 0}
set {triangle}T   {0 0} {4 0} {0 3}
set {carre}K      {0 0} {4 0} {4 4} {0 4}
set {cercle}C5    {0 0} 5
set {droite}Y3    {0 3} {1 3}

puts "--- vecteurs, droites, segments"
montre 1  "projeté de (3,4) sur Ox"        [geo::projete {3 4} $Ox]          "(3, 0)"
montre 1b "projeté de (2,0) sur y = x"     [geo::projete {2 0} $Diag]        "(1, 1)"
montre 2  "distance (5,5) au segment (0,0)-(10,0)"    [geo::distance_segment {5 5} $S10]  5
montre 2b "distance (-3,4) au même segment"     [geo::distance_segment {-3 4} $S10] "5, vers (0,0)"
set {segment}s1 {0 0} {4 4};  set {segment}s2 {0 4} {4 0}
montre 3  "intersection de diagonales"     [geo::intersection $s1 $s2]      "{en}(2, 2)"
set {segment}s3 {0 0} {4 0};  set {segment}s4 {0 1} {4 1}
montre 3b "intersection de parallèles"     [geo::intersection $s3 $s4]      "aucune"
montre 11 "rotation de (1,0) de 90°"       [geo::rot90 {1 0}]                "(0, 1)"
montre 11b "(1,2) . (3,4)"                 [geo::scal {1 2} {3 4}]           11

puts "\n--- polylignes, polygones"
set {polyligne}PL {0 0} {3 4} {3 10}
montre 4  "longueur de la polyligne"       [geo::longueur $PL]               11
montre 5  "aire du triangle"               [geo::aire $T]                    6
montre 5b "périmètre du triangle"          [geo::perimetre $T]               12
montre 6  "aire du carré de côté 4"        [geo::aire $K]                    16
montre 7  "(2,2) dans le carré ?"          [geo::dedans {2 2} $K]            oui
montre 7b "(5,1) dans le carré ?"          [geo::dedans {5 1} $K]            non
montre 12 "segment2rect (0,0)-(4,4)"       [geo::segment2rect $s1]           "(0,0) (0,4) (4,4) (4,0)"
montre 13 "rectangle englobant du triangle" [geo::englobant $T]              "min (0,0), max (4,3)"

puts "\n--- triangles, cercles, courbes"
montre 8  "cercle r = 5 et droite y = 3"   [geo::cercle_droite $C5 $Y3]      "(-4, 3) et (4, 3)"
montre 9  "cercle circonscrit au triangle" [geo::circonscrit $T]             "centre (2, 1.5), rayon 2.5"
set {triangle}T6 {0 0} {6 0} {0 3}
montre 10 "centre de gravité"              [geo::gravite $T6]                "(2, 1)"
set {bezier}Bz {0 0} {0 4} {4 4} {4 0}
montre 14 "Bézier en t = 0,5"              [geo::bezier_point $Bz 0.5]       "(2, 3)"

# FR : les types protègent : un segment n'est pas une droite.
# EN : types protect: a segment is not a line.
puts "\n--- refus / refusals"
foreach essai {
    {geo::projete {3 4} $S10}
    {geo::aire $s1}
    {geo::gravite $K}
    {geo::projete {3 a} $Ox}
} {
    catch $essai m
    puts [format "     %-28s -> %s" $essai $m]
}
