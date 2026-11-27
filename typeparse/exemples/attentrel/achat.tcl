# achat.tcl -- le processus d'achat (BPMN) de Convolutions.rtf, avec attentrel
# réécrit sur TIP 764.
#
#   Service demandeur : demander un achat ............ réceptionner la livraison
#   Service achat     : approuver (ou rejeter), passer commande ....... clore
#   Comptabilité      :                  enregistrer la facture, payer
#
#   tclsh achat.tcl
source [file join [file dirname [info script]] attentrel.tcl]

# la date des convolutions ; fixe ici pour que la sortie soit reproductible
proc maintenant {} { return "07/11/2026" }

proc montre {script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERREUR $r" }
    puts [format "%-58s -> %s" [string trim [regsub -all {\s+} $script " "]] $r]
}

# ======================================================================
# Attributs : des types, rien à écrire
# ======================================================================
type def EtatAchat {"créé", "demandé", "validé", "refusé", "commandé",
                    "livré", "facturé", "payé", "clos"}
transitions EtatAchat {
    créé     demandé
    demandé  {validé refusé}
    validé   commandé
    commandé livré
    livré    facturé
    facturé  payé
    payé     clos
}

# ======================================================================
# Entités : une identité (TclOO) et une fiche typée
# ======================================================================
entite Service  {{string}nom; {string}mel ?= ""}
entite Achat    {{double}montant; {string}description; {EtatAchat}état ?= créé}
entite Commande {{Achat}achat; {string}fournisseur; {string}date}
entite Facture  {{Commande}commande; {double}montant; {string}numéro}
entite Budget   {{string}service; {double}reste}

Service create ::HSE          {/nom}"Service HSE"   {/mel}hse@societe.org
Service create ::Achats       {/nom}"Service achat" {/mel}achat@societe.org
Service create ::Comptabilité {/nom}Comptabilité    {/mel}compta@societe.org
Achat   create ::poubelle     {/montant}100 {/description}"nouvelle poubelle"
Budget  create ::budgetHSE    {/service}HSE {/reste}1000

# ======================================================================
# Convolutions : actants (arguments), circonstants (options), et ce que la
# convolution fait à chaque actant : une précondition \conduit une
# postcondition, dans son type
# ======================================================================
convolution Demander {
    {Service}demandeur;
    {(Achat(état) eq "créé" \conduit Achat(état) = "demandé")}objet;
    {Service}receveur;
    {string}-date ?= [maintenant];
    {string}-but ?= ""; {string}-moyen ?= ""
} {}

# une convolution en vise une autre : son actant est la convolution réifiée ;
# l'achat en est tiré par un défaut calculé (on peut aussi le donner)
convolution Approuver {
    {Service}approbateur; {Demander}demande;
    {(Achat(état) eq "demandé" && Achat(montant) <= 5000
      \conduit Achat(état) = "validé")}achat ?= [set demande(objet)];
    {string}-date ?= [maintenant]
} {}

convolution Rejeter {
    {Service}approbateur; {Demander}demande; {string}motif;
    {(Achat(état) eq "demandé" \conduit Achat(état) = "refusé")}achat ?= [set demande(objet)];
    {string}-date ?= [maintenant]
} {}

convolution Passer_commande {
    {Service}acheteur; {Approuver}approbation; {string}fournisseur;
    {(Achat(état) eq "validé" \conduit Achat(état) = "commandé")}achat ?= [set approbation(achat)];
    {(\produit Commande)}commande;
    {string}-date ?= [maintenant]
} {
    set commande [produit Commande {/achat}$achat {/fournisseur}$fournisseur {/date}$date]
}

convolution Réceptionner {
    {Service}receveur; {(\lit Commande)}commande;
    {(Achat(état) eq "commandé" \conduit Achat(état) = "livré")}achat ?= [$commande get achat]
} {}

convolution Enregistrer_facture {
    {Service}comptable; {(\lit Commande)}commande; {string}numéro; {double}montant;
    {(Achat(état) eq "livré" \conduit Achat(état) = "facturé")}achat ?= [$commande get achat];
    {(\produit Facture)}facture
} {
    set facture [produit Facture {/commande}$commande {/montant}$montant {/numéro}${numéro}]
}

convolution Payer {
    {Service}payeur; {(\lit Facture)}facture;
    {(Achat(état) eq "facturé" && Facture(montant) <= Achat(montant) * 1.2
      \conduit Achat(état) = "payé")}achat ?= [[$facture get commande] get achat]
} {}

convolution Clore {
    {Service}acheteur; {(\déduit Commande)}commande;
    {(Budget(reste) >= 0
      \conduit Budget(reste) = \avant Budget(reste) - Achat(montant))}budget;
    {(Achat(état) eq "payé" \conduit Achat(état) = "clos")}achat ?= [$commande get achat]
} {}

# ======================================================================
# Le processus
# ======================================================================
puts "--- demander, approuver, commander"
set d [Demander -but "remplacer la poubelle cassée" ::HSE ::poubelle ::Achats]
montre {type get $d}
montre {set d(objet)}
montre {poubelle get état}
set ok [Approuver ::Achats $d]
set pc [Passer_commande ::Achats $ok "Fournitures Martin"]
montre {poubelle get état}
montre {Commande1 fiche}

puts "\n--- livrer, facturer, payer, clore"
Réceptionner ::HSE ::Commande1
Enregistrer_facture ::Comptabilité ::Commande1 F-2026-114 104.5
montre {Facture1 fiche}
Payer ::Comptabilité ::Facture1
Clore ::Achats ::Commande1 ::budgetHSE
montre {poubelle get état}
montre {budgetHSE get reste}
montre {info object isa object ::Commande1}

puts "\n--- ce que les types et l'automate refusent"
Achat create ::chaise {/montant}80 {/description}"chaise de bureau"
montre {Achat create ::table {/montant}cent {/description}table}
montre {Demander ::HSE ::chaise}
montre {Demander ::chaise ::chaise ::Achats}
montre {Demander ::HSE ::HSE ::Achats}
montre {Approuver ::Achats $ok}
montre {Approuver ::Achats $d}
set d2 [Demander ::HSE ::chaise ::Achats]
montre {set r [Rejeter ::Achats $d2 "budget épuisé"]; set r(motif)}
montre {Approuver ::Achats $d2}
Achat create ::serveur {/montant}8000 {/description}"serveur de fichiers"
set d4 [Demander ::HSE ::serveur ::Achats]
montre {Approuver ::Achats $d4}
montre {chaise set état payé}
montre {chaise set état volé}
Achat create ::bureau {/montant}300 {/description}bureau
set d3 [Demander ::HSE ::bureau ::Achats]
Passer_commande ::Achats [Approuver ::Achats $d3] "Mobilier Durand"
Réceptionner ::HSE ::Commande2
Enregistrer_facture ::Comptabilité ::Commande2 F-2026-131 450
montre {Payer ::Comptabilité ::Facture2}
montre {bureau get état}

puts "\n--- ce que dit la définition"
puts [phrase Demander]
puts [phrase Passer_commande]
puts [phrase Clore]
Achat create ::lampe {/montant}40 {/description}"lampe de bureau"
montre {possibles ::lampe}
Demander ::HSE ::lampe ::Achats
montre {possibles ::lampe}

puts "\n--- la composition : l'effet d'un processus entier"
montre {effet Demander}
puts "Demander … Clore :"
puts [processus Demander Approuver Passer_commande Réceptionner \
                Enregistrer_facture Payer Clore]
montre {processus Demander Rejeter Passer_commande}
montre {processus Demander Approuver Approuver}
montre {processus Réceptionner}
montre {processus Clore Payer}
montre {processus Passer_commande Réceptionner Enregistrer_facture Payer Clore Clore}

puts "\n--- une déclaration que le corps ne tient pas"
convolution Annoter {{(\lit Achat)}achat; {string}note} {
    $achat set description $note
}
montre {Annoter ::lampe "lampe LED"}
montre {lampe get description}
convolution Commander_vite {{Service}acheteur; {(\produit Commande)}commande} {}
montre {Commander_vite ::Achats}
convolution Trancher {
    {(Achat(état) eq "demandé" \conduit Achat(état) in {"validé" "refusé"})}achat
} {}
montre {Trancher ::lampe}
montre {processus Demander Trancher Passer_commande}
montre {processus Demander Trancher Réceptionner}

puts "\n--- une déclaration refusée à la définition"
montre {convolution Bof1 {{(Achat(état) eq "crée" \conduit Achat(état) = "demandé")}a} {}}
montre {convolution Bof2 {{(Achat(etat) eq "créé" \conduit Achat(état) = "demandé")}a} {}}
montre {convolution Bof3 {{(Achat(état) == "créé" \conduit Achat(état) = "demandé")}a} {}}
montre {convolution Bof4 {{(Achat(état) = "créé" \conduit Achat(état) = "demandé")}a} {}}
montre {convolution Bof5 {{(Achat(état) eq créé \conduit Achat(état) = "demandé")}a} {}}
montre {convolution Bof6 {{(\avant Achat(montant) > 0 \conduit Achat)}a} {}}
montre {convolution Bof7 {{(Achat \traduit Texte)}a} {}}

puts "\n--- les phases d'une convolution : pour, à, par, de, fors"
proc suivre {conv act} { puts "  $conv : [phase $conv]   (en cours : [phase])" }
foreach p {pour à par de fors} { quand $p Demander suivre }
Achat create ::stylo {/montant}3 {/description}"stylos"
set d5 [Demander ::HSE ::stylo ::Achats]
foreach p {pour à par de fors} { quand $p Demander {} }
montre {phase}
montre {quand pendant Demander suivre}

puts "\n--- un échec défait tout, sauf ce qui est achevé (fors)"
proc liste_noire {conv act} {
    if {[dict get $act fournisseur] eq "Ventes Louches"} {
        error "fournisseur sur liste noire"
    }
}
quand de Passer_commande liste_noire
set a5 [Approuver ::Achats $d5]
montre {Passer_commande ::Achats $a5 "Ventes Louches"}
montre {info class instances Commande}
montre {stylo get état}
montre {set pc5 [Passer_commande ::Achats $a5 "Fournitures Martin"]; set pc5(commande)}
montre {stylo get état}

puts "\n--- le journal : chaque convolution est une valeur typée"
puts [journal]
