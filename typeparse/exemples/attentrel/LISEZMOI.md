# attentrel sur TIP 764 : attributs, entités, convolutions

`attentrel` (Florent Merlet, 2025) décrit un domaine à la manière de TypeDB :
des **attributs** (des valeurs), des **entités** (des choses qui ont une
identité), des **relations** entre elles, plus une notion propre : la
**convolution**, un événement qui a des actants, des circonstants et des
conséquences. C'est un verbe au sens de Tesnière : *le service HSE (actant)
demande un achat (actant) au service achat (actant), pour remplacer la poubelle
(circonstant)* ; la demande a pour conséquence que l'achat passe à l'état
« demandé ».

La version d'origine, écrite avant TIP 764, construit tout cela en TclOO :
chaque attribut est un objet, avec ses méthodes `check`, `get`, `set` écrites à
la main (environ 1 700 lignes). Ici, la même idée tient en 400 lignes, plus 1 200 pour l'algèbre des effets, parce
que TIP 764 fournit déjà les morceaux :

| attentrel | ici |
|---|---|
| `Field << Num`, `Enum`, `Struct`, `DICO`… | un type : `double`, `{"créé", "demandé", …}`, un produit, `dict.T` |
| entité | une classe TclOO (l'identité) dont la fiche est un produit typé : chaque écriture est vérifiée |
| actant `1,1` | un argument de la signature : `{Service}demandeur` |
| actant `1,+` | un champ variadique : `{T}args...` |
| circonstant `0,*` | une option : `{string}-but ?= ""` |
| `[demandeur HSE]` à la création | une étiquette : `{/nom}"Service HSE"` |
| conséquences Produit, Conduit, Détruit | une précondition `\conduit` une postcondition, dans le type de l'actant |
| la convolution créée, gardée | la **convolution réifiée** : `type call`, l'appel comme valeur typée du produit de ses actants et circonstants, rendue et rangée dans un journal |

| fichier | |
|---|---|
| `attentrel.tcl` | `entite`, `convolution`, `transitions`, `produit`, `journal`, `effet`, `phrase`, `possibles` |
| `effets.tcl` | l'algèbre des effets : lecture, vérification, exécution, et `processus` (la composition par interprétation abstraite) |
| `achat.tcl` | le processus d'achat BPMN de `Convolutions.rtf` : demander, approuver ou rejeter, commander, réceptionner, facturer, payer, clore (`achat.out`) |

## Le modèle

```tcl
type def EtatAchat {"créé", "demandé", "validé", "refusé", "commandé", …}
transitions EtatAchat {créé demandé  demandé {validé refusé}  validé commandé …}

entite Service {{string}nom; {string}mel ?= ""}
entite Achat   {{double}montant; {string}description; {EtatAchat}état ?= créé}
entite Budget  {{string}service; {double}reste}

convolution Demander {
    {Service}demandeur;                                            ;# actants…
    {(Achat(état) eq "créé" \conduit Achat(état) = "demandé")}objet;  ;# … et ce qui leur arrive
    {Service}receveur;
    {string}-date ?= [maintenant];                                 ;# circonstants
    {string}-but ?= ""; {string}-moyen ?= ""
} {}

convolution Approuver {
    {Service}approbateur; {Demander}demande;
    {(Achat(état) eq "demandé" && Achat(montant) <= 5000
      \conduit Achat(état) = "validé")}achat ?= [set demande(objet)];
    {string}-date ?= [maintenant]
} {}

convolution Clore {
    {Service}acheteur; {(\déduit Commande)}commande;
    {(Budget(reste) >= 0
      \conduit Budget(reste) = \avant Budget(reste) - Achat(montant))}budget;
    {(Achat(état) eq "payé" \conduit Achat(état) = "clos")}achat ?= [$commande get achat]
} {}

set d [Demander -but "remplacer la poubelle cassée" ::HSE ::poubelle ::Achats]
```

### Ce que la convolution fait à chaque actant : une pré- et une postcondition

C'est la typologie de l'analyse d'énoncé (forum ABCLF, 2017) : ce qu'un verbe
fait à l'existence de ses thèmes. Elle s'écrit, dans le type de l'actant, comme
une condition **avant**, `\conduit`, une condition **après** ; `()` est
l'inexistence. C'est la logique de Hoare (`{P} C {Q}`), lue comme un verbe.

| on écrit | forme canonique | la convolution | en programmation |
|---|---|---|---|
| `(\lit Facture)` | `Facture \conduit Facture` | conductive stationnaire | entrée (`in`) |
| `(Achat(état) eq "créé" \conduit Achat(état) = "demandé")` | la même | conductive mobile | entrée-sortie (`in out`) |
| `(Achat(état) eq "demandé" \conduit Achat(état) in {"validé" "refusé"})` | la même | mobile, le corps choisit | |
| `(\produit Commande)` | `() \conduit Commande` | productive | sortie (`out`) |
| `(\déduit Commande)` | `Commande \conduit ()` | déductive | transfert (le *move* de Rust) |
| `(Budget(reste) >= 0 \conduit Budget(reste) = \avant Budget(reste) - Achat(montant))` | la même | réductive | consommation partielle |

La langue des conditions mêle les mots et les signes, comme Tcl : `eq`, `ne`
pour le texte, `==`, `!=`, `<`, `<=`, `>`, `>=` pour les nombres, `in`, `ni`
pour une liste, `&&`, `||`, `!`, et `+ - * /`. Un chemin `Achat(montant)`
désigne l'attribut de l'actant de cette classe (le sien, ou le seul autre
actant de la classe) ; `$achat(montant)`, celui d'un actant nommé ; `$x`, un
argument. Après `\conduit`, `=` affecte, et `\avant X` est la valeur de `X`
avant la convolution : c'est un opérateur du temps, pas un nom (une entité
pourrait s'appeler `old`).

- **À la définition**, tout est vérifié par le système de types : le champ
  (`Achat has no attribute "etat"`), la valeur dans son type
  (`value "crée" matches no variant of EtatAchat`), l'opérateur
  (`Achat(état) is text (EtatAchat): compare with eq, as in Tcl`),
  l'affectation avant `\conduit` (`"=" assigns, after \conduit only`), un mot
  nu (`"créé": write a value as "créé", an attribute as créé(attr)`).
- **Avant le corps**, la précondition est vérifiée, conjoint par conjoint, avec
  les valeurs en cause (`Approuver: achat serveur: Achat(montant) <= 5000 is
  false (Achat(montant) = 8000)`).
- **Après le corps**, le moteur applique les affectations que le corps n'a pas
  faites, puis vérifie toute la postcondition : un actant lu qui a changé, un
  produit qui n'a pas été créé, une alternative que le corps devait trancher.
  La plupart des corps sont vides : la déclaration *est* la conséquence. Le
  contrôle de `Payer` (la facture ne dépasse pas l'achat de plus de 20 %),
  autrefois dans le corps, est maintenant une précondition :
  `Facture(montant) <= Achat(montant) * 1.2`.

Un actant affecté qui n'est pas donné directement (l'achat d'une approbation)
se tire d'un autre par un défaut calculé : `… achat ?= [set demande(objet)]`.

### La composition : nécessaire, requis, contingent, impossible

`processus` suit une chaîne de convolutions sans rien exécuter. Chaque attribut
y est un **domaine** : un ensemble de valeurs pour une énumération, un
intervalle pour un nombre (l'interprétation abstraite). Chaque condition reçoit
un verdict, le carré logique :

- **nécessaire** : vraie dans tout ce que la chaîne permet ;
- **impossible** : fausse partout, la chaîne casse là ;
- **contingent** : vraie dans certaines exécutions seulement, parce qu'elle
  dépend de ce que la chaîne a produit ou changé, ou d'un argument ;
- **requis** : une contingence qui ne porte que sur l'état de départ ; elle
  devient la précondition de la chaîne entière.

Et l'effet de la chaîne s'écrit lui-même comme un effet :

```
processus Demander Approuver Passer_commande Réceptionner Enregistrer_facture Payer Clore
Demander             objet      requis : Achat(état) eq "créé"
Approuver            achat      requis : Achat(montant) <= 5000
Passer_commande      achat      nécessaire
…
Payer                achat      contingent : Facture(montant) <= Achat(montant) * 1.2   (Achat(montant) ∈ ]-∞, 5000], Facture(montant) ∈ ]-∞, +∞[)
Clore                budget     requis : Budget(reste) >= 0
Clore                achat      nécessaire
effet :
  Achat : Achat(état) eq "créé" && Achat(montant) <= 5000 \conduit Achat(état) = "clos"   (traversante)
  Commande : () \conduit ()   (flux)
  Facture : () \conduit Facture(montant) <= 6000   (fait)
  Budget : Budget(reste) >= 0 \conduit Budget(reste) >= -5000   (traversante)
```

Chaque entité est rangée selon son existence aux deux bouts de la chaîne :
la typologie d'un verbe, relue à l'échelle d'un processus.

| sur la chaîne | l'entité est | ici |
|---|---|---|
| `X \conduit X` | traversante | Achat, Budget |
| `() \conduit X` | un fait, qui reste | Facture |
| `X \conduit ()` | consommée | |
| `() \conduit ()` | un flux, né et clos dans la chaîne | Commande |

Le quatrième cas n'existe pas pour un verbe seul (une convolution ne produit et
ne déduit pas la même entité) : le flux est une notion de composition. Un
produit que d'autres convolutions savent déduire est un **flux resté ouvert**,
signalé comme tel (`Commande : () \conduit Commande   (flux ouvert : Clore le
clôt)`) : le processus est probablement inachevé.

Le rapport dit ce que l'on n'avait pas écrit : le budget peut devenir négatif
(jusqu'à −5000, puisque l'achat approuvé peut aller jusqu'à 5000), et, faute
d'une borne basse sur `montant`, il n'a pas de borne haute : un montant négatif
le ferait croître. Une contrainte de type (`montant > 0`, le `@range` de
TypeDB) le fermerait.

Une alternative ouvre des **branches**, gardées séparément ; une étape peut
être impossible dans l'une et non dans l'autre :

```
processus Demander Trancher Passer_commande
Demander             objet      requis : Achat(état) eq "créé"
Trancher             achat      nécessaire
                                branches : Achat(état) = "validé" | Achat(état) = "refusé"
Passer_commande      achat      nécessaire
                                impossible dans la branche Achat(état) = "refusé" : Achat(état) eq "validé", mais Achat(état) = refusé
Passer_commande      commande   nécessaire
effet :
  Achat(état) = "validé" :
    Achat : Achat(état) eq "créé" \conduit Achat(état) = "commandé"
    Commande : () \conduit Commande   (flux ouvert : Clore le clôt)
```

Les limites, connues : les domaines ne sont pas relationnels (un intervalle
par attribut : `Budget(reste) >= Achat(montant)` se perd en route) ; une
entité par classe dans une chaîne (la composition par classe, pas encore par
rôle).

- **`effet NOM`** : les effets d'une convolution.
- **`phrase NOM`** lit la définition : *Demander conduit objet (Achat) : si
  Achat(état) eq "créé", alors Achat(état) = "demandé" ; avec demandeur
  (Service), receveur (Service) ; circonstances : date, but, moyen.*
- **`possibles OBJET`** : les convolutions dont la précondition est vraie pour
  cet objet, maintenant ; une condition sur un autre actant est donnée entre
  parenthèses (une lampe « créée » : `Demander → Achat(état) = "demandé"` ;
  « demandée » : `Approuver`, `Rejeter`).

### Les phases d'une convolution

Une convolution n'est pas instantanée. Elle passe par les cinq phases de
l'analyse du procès, nommées par leur préposition :

| phase | | dans le corps généré |
|---|---|---|
| `pour` | préparation | l'appel est fait, les actants liés |
| `à` | transition inchoative | les rôles et la précondition tiennent : les actants sont saisis |
| `par` | opération | le corps |
| `de` | transition égressive | les affectations, la postcondition |
| `fors` | achèvement | la convolution réifiée, au journal |

`phase ?NOM?` dit où en est une convolution en cours ; `quand PHASE NOM
COMMANDE` accroche une commande à l'entrée d'une phase (`NOM` peut être `*`),
appelée avec le nom et les actants (à `fors`, la convolution réifiée).

**Un échec avant `fors` défait tout** : les fiches des actants reprennent leur
valeur, les entités produites disparaissent, rien n'entre au journal ; seul
l'achèvement est irréversible. Ce peut être une précondition, le corps, une
postcondition, ou un crochet :

```tcl
quand de Passer_commande liste_noire     ;# refuse un fournisseur
Passer_commande ::Achats $a5 "Ventes Louches"  ;# -> fournisseur sur liste noire
stylo get état                           ;# -> validé : rien n'a changé
info class instances Commande            ;# la commande produite a disparu
```

Les noms ne sont pas réutilisés : la commande suivante est `Commande4`, pas
`Commande3`. Ce qui est défait : les fiches des actants entités et les
produits déclarés ; pas un objet hors des actants que le corps aurait changé,
ni une convolution imbriquée déjà achevée.

### Le reste

- **La convolution réifiée.** `Demander` rend une valeur de type `Demander`
  (`type call`) : `$d(objet)` vaut `::poubelle`. Une convolution peut en viser
  une autre ; passer une approbation là où l'on attend une demande est refusé
  par la signature (`unknown way to cast the value from Approuver to Demander`).
- **Le rôle.** Un actant dont le type est une entité doit être un objet de cette
  classe (`Demander: actant "objet" must be Achat, got "::HSE"`).
- **L'automate.** `transitions` donne les changements d'état permis ; la règle
  vit dans la fiche, une écriture directe (`chaise set état payé`) y passe aussi.
- **Le journal.** Chaque convolution achevée y entre, avec ses produits ; une
  convolution qui échoue n'y entre pas.

## Ce que l'exemple montre de TIP 764

Ce qui marche sans rien ajouter : les attributs, les fiches, les signatures
(actants, circonstants, usage, contrôles), les étiquettes qui traversent un
constructeur TclOO (`Service create ::HSE {/nom}"Service HSE"`), les chemins à
travers des produits imbriqués.

**La réification de l'appel** est faite par le noyau depuis la v16z :
`convolution` définit la procédure, nomme son enregistrement
(`type def Demander [dict get [type proc Demander] record]`), et le corps
commence par `set evt [type call Demander]`.

Ce qui est encore émulé, et que le noyau pourrait offrir :

1. **Les types par prédicat.** Le type `Service` n'est ici qu'un `string`
   (le nom de l'objet) ; le contrôle de classe est un préambule généré. Un type
   comme `obj(Service)`, ou des contraintes comme `@regex` et `@range` de
   TypeDB (pour `Tel`, `Mel`, un montant positif), le ferait dans la signature.
2. **Les signatures TclOO.** Un constructeur ou une méthode n'accepte pas
   encore de signature typée ; le constructeur passe donc ses arguments au
   constructeur du produit (`set {Service_fiche}fiche {*}$args`).

Pas encore : les relations (`Employer`), les biconvolutions associatives et
dissociatives (d'autres opérateurs sur des paires), `\reproduit` (la copie) et
`\traduit` (deux entités de part et d'autre) ; la composition par rôle plutôt
que par classe (deux achats dans une même chaîne) ; des domaines relationnels ; l'enchaînement (*induit*,
par une file d'événements) et la cause de chaque convolution au journal ; le
temps et les convolutions virtuelles (promesses) ; plusieurs classes pour un
même rôle (le `plays` de TypeDB).

Une remarque pratique : `$numéro` ne se substitue pas en Tcl (une variable
n'accepte que des lettres ASCII après `$`) ; il faut `${numéro}`. Le
générateur l'écrit ainsi.

## VolɷDuctus

La suite (un métier réel : la mise à disposition de sites télécom, l'accueil
des mails, le rejeu d'un projet, la lecture des classeurs) dépasse le cadre de
la TIP 764 : elle vit dans une archive séparée, VolɷDuctus, qui reprend ce
moteur et demande le `tclsh` de `typeparse`.

