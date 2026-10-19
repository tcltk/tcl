# reseau_couches.tcl --
#
# Un message qui traverse les couches d'un réseau, illustré avec nos
# annotations. Chaque couche est un type dont le champ "payload" n'accepte
# QUE la couche immédiatement supérieure : l'encapsulation est vérifiée par
# le système de types, comme la feuille collée en tête du rouleau
# (prôtokollon) dit ce que contient ce qui suit.

# --- les couches (de la plus haute à la plus basse) ----------------------
type def http {
    {string}request;
    {string}host = "example.org";
    {string}body = ""
}
type def tcp {
    {int}src;
    {int}dst;
    {int}seq = [incr ::seq];            ;# calculé à chaque segment
    {http}payload
}
type def ip {
    {string}src;
    {string}dst;
    {int}ttl = 64;
    {tcp}payload
}
type def eth {
    {string}src;
    {string}dst;
    {int}epoch = [clock seconds];       ;# horodatage de la trame
    {ip}payload
}
set ::seq 1000

# --- émission : on descend les couches, chacune ajoute son en-tête --------
proc emettre {requete} {
    set {http}h request $requete
    set {tcp}t src 49152 dst 80 payload $h
    set {ip}p src 10.0.0.2 dst 93.184.216.34 payload $t
    set {eth}f src aa:bb:cc:00:00:01 dst aa:bb:cc:00:00:02 payload $p
    return $f
}

# --- inspection : on remonte en lisant le type de chaque couche -----------
proc inspecter {trame} {
    set chemin ""
    set retrait ""
    while {1} {
	# Le chemin est construit dans une variable. Une valeur substituée
	# dans un index est UNE feuille : $trame($chemin) chercherait le champ
	# « payload.payload ». On nomme donc la variable avec son chemin : un
	# nom donné comme chaîne (set $ref) est lu comme un chemin littéral.
	set ref trame($chemin)
	set couche [expr {$chemin eq "" ? $trame : [set $ref]}]
	set nom [type get $couche]
	puts "${retrait}\[$nom\]"
	foreach champ {src dst epoch ttl seq request host} {
	    catch {puts "${retrait}  $champ = $couche($champ)"}
	}
	if {$nom eq "http"} break
	append chemin [expr {$chemin eq "" ? "payload" : ".payload"}]
	set retrait "$retrait    "
	unset couche
    }
}

set trame [emettre "GET /index.html"]
puts "=== la trame telle qu'elle circule (texte brut) ==="
puts $trame
puts "\n=== décapsulation couche par couche ==="
inspecter $trame

puts "\n=== accès direct par chemin ==="
puts "requête HTTP : $trame(payload.payload.payload.request)"
puts "port TCP     : $trame(payload.payload.dst)"
puts "TTL IP       : $trame(payload.ttl)"

puts "\n=== le système de types refuse les couches mal empilées ==="
set {tcp}seg src 1 dst 2 payload [set {http}x request "GET /"]
if {[catch {set {eth}mauvaise src a dst b payload $seg} m]} {puts "Ethernet directement sur TCP : $m"}
if {[catch {set {tcp}bad src abc dst 80 payload $x} m]} {puts "port invalide : $m"}
