# exemples/texte_enrichi — du texte enrichi typé pour un widget text

* `modele.tcl` : les types (styles, segments, blocs, document), des constructeurs courts, le rendu.
* `test.tcl` : le modèle sans Tk (`test.out`), avec un faux widget.
* `fauxtext.tcl` : le faux widget : `insert`, `tag add|remove|configure|bind`, `get`, `dump`
  au format de Tk (une suite plate de textes et de bascules, sans arbre, comme Tk). La démo a été essayée avec Tk 9.0.2 (sous Xvfb) : rendu,
  Ctrl-B/Ctrl-I, relecture.
* `demo.tcl` : la démo dans un vrai widget `text` (à lancer avec le wish de l'arbre TIP 764) :
  le document à gauche, éditable (Ctrl-B, Ctrl-I sur la sélection) ; « Relire » le reconstruit
  par `te::lire` et le rend à droite.

N'emploie que des formes communes à la v16k et à la v16l (constructeurs positionnels, chemins,
variantes nommées par le type `{segment.lien}`) ; `test.tcl` donne la même sortie sur les deux.

## Le modèle

```tcl
type def segment {
    {string}texte,
    {{nomstyle}style; {list.segment}contenu}marque,     ;# récursif
    {{string}cible; {list.segment}contenu}lien
}
type def bloc {
    {list.segment}para,
    {{niveau}niveau; {list.segment}contenu}titre,       ;# niveau = "1", "2", "3"
    {list.list.segment}puces,
    {string}code
}
type def document list.bloc
```

`nomstyle` est la somme des noms de la table des styles, refaite par `te::styles` (avec
`type forget` sur toute la chaîne). Construire :

```tcl
te::document [te::titre 1 "Titre"] \
    [te::para "Un " [te::m gras "texte"] " et un " [te::lien https://tcl.tk "lien"]]
```

Le rendu est un `switch` sur `type get`, et lit les variantes par chemin (`$b(titre.niveau)`,
`$s(marque.contenu)`).

## Écrire sans guillemets : le gabarit

Avec `te::para`, chaque morceau de texte est un argument, et un espace sépare deux arguments :
il faut des guillemets partout. Un **gabarit** est un seul argument, le texte tel qu'on le lit ;
les passages balisés sont entre crochets, une **tête** puis le **contenu** :

```tcl
te::document \
    [te::h 1 {Texte enrichi et types}] \
    [te::p {Voir [lien@cible="https://tcl.tk" -- la page de la TIP], puis
        [alerte -- un texte [gras -- très] important].}] \
    [te::ul {premier point} {second, avec [italique -- un style]}]
```

* `[variante@attribut=valeur,… -- contenu]` : une variante de `segment` (`marque`, `lien`) ; la
  tête a la forme des noms de tags (`te::LireNotes` : `,` sépare, `"…"` cite une valeur).
  `[style -- contenu]` est le raccourci de `[marque@style=style -- contenu]`.
* Le contenu est lu comme le texte autour (pas en mots Tcl) : espaces, ponctuation et `--`
  gardés, crochets imbriqués. `\[`, `\]`, `\\` échappent. Comme en HTML, une suite de blancs
  (fin de ligne comprise) compte pour un espace.
* `te::p`, `te::h niveau`, `te::ul gabarit…` (un gabarit par puce) ; `te::texte gabarit` rend la
  liste des segments. Le document obtenu est **le même** que celui écrit à la main avec
  `te::para`, `te::m`, `te::lien` (testé), et l'aller-retour par le widget aussi.
* Erreurs : balise inconnue (avec la liste des styles), attribut inconnu ou manquant
  (`unknown field "href" in segment.lien`), `[a b]` sans `--`, crochet seul ; un style hors du
  type : `value "surligne" matches no variant of nomstyle`.

**Un document entier : `te::ecrire`.** Le script s'exécute dans un interpréteur sûr
(`interp create -safe`) où les blocs ont des noms courts, `h`, `p`, `ul`, `code`, sans rien masquer
dans l'interpréteur principal :

```tcl
set doc [te::ecrire {
    h 1 {Texte enrichi et types}
    p {Voir [lien@cible="https://tcl.tk" -- la page de la TIP].}
    ul {premier point} {second, avec [italique -- un style]}
    code {set x [list 1 2]}
}]
```

Chaque bloc est un alias vers `te::p`, `te::h`, `te::ul`, `te::code`, qui construit le bloc typé
dans l'interpréteur principal et le range : seuls des textes traversent, le document est typé et
vérifié avec les types de l'interpréteur principal (chaque interpréteur a sa table de types). Le
document ne peut rien faire d'autre : ni `open`, ni `exec`, ni `te::…` (testé). C'est aussi ce qui
permet de lire un document venu d'ailleurs sans risque.

L'analogie avec HTML est directe : l'élément est la variante, les attributs ses champs simples,
le contenu son champ `{list.segment}contenu`. Chaque balise est construite par un **chemin calculé à
travers la somme**, `set {segment}s($nom.($champs,contenu)) {*}$valeurs` (v16o) : aucune table des
balises, c'est le type qui refuse une variante inconnue, un attribut inconnu ou manquant, un style
hors de `nomstyle` ; une balise inconnue se reconnaît par `type def segment.$nom`, qui rend la
définition d'une variante.

## Des noms de tags `type@notes`

Un tag Tk est un texte, et le type d'une valeur n'est jamais dans son texte. Le nom d'un tag suit
donc l'affichage des valeurs typées de la TIP 764, `T@notes` : le **nom canonique** de la variante
du nœud, puis ses attributs comme notes, nommés par les champs du type.

| tag | porte |
|---|---|
| `bloc.para`, `bloc.puces`, `bloc.code` | un paragraphe, une liste à puces, du code (tout le bloc) |
| `bloc.titre@niveau=2` | un titre de niveau 2 (`$b(titre.niveau)`) |
| `segment.marque@style=gras` | un passage du style `gras` (`$s(marque.style)`) |
| `segment.lien@cible="https://tcl.tk"` | un lien vers `https://tcl.tk` (`$s(lien.cible)`) |

Un tag porte la **tête** d'un nœud (sa variante et son attribut simple) ; ses enfants sont dans les
plages qu'il couvre. Ces tags ne sont **pas configurés** : ils disent ce qu'est le texte, pas son
apparence (voir plus bas).

### La syntaxe des notes

Un ensemble de chemins, dans la syntaxe des sommes et des chemins de la TIP :

| | |
|---|---|
| `,` | sépare les notes ; l'ordre ne compte pas |
| `.` | une branche : descend d'un nœud à l'autre |
| `=` | une feuille et sa valeur ; sans `=`, la note est un drapeau (`gras`) |
| `(…)` | factorise : `font.(family=helvetica,size=10)` = `font.family=helvetica,font.size=10` |
| `"…"` | une clé ou une valeur hors de `[\w-]`, avec les substitutions backslash de Tcl (`\"`, `\\`, `\n`, `\u00e9`) |

* **La dernière l'emporte**, comme une clé répétée dans un dict : `size=10,size=12` → `size=12`.
  Poser une feuille efface ce qui est dessous ou dessus : `font=x,font.size=10` → `font.size=10`.
* **Forme canonique** (`te::EcrireNotes`) : clés triées, branches factorisées, une branche à un
  seul enfant sans parenthèses (`police.poids=bold`), guillemets seulement s'il le faut. Un même
  sens donne un même nom, donc Tk fusionne bien deux plages voisines de même sens ; relire puis
  réécrire une forme canonique la redonne à l'identique (testé).
* Plus d'encodage d'URL : `segment.lien@cible="https://wiki.tcl-lang.org/page/Été 2026?q=a.b&x=1_2"`
  se relit à l'identique.
* `te::Tag T ?clé valeur…?` écrit un nom (une clé peut être un chemin : `police.taille`) ;
  `te::LireNotes` / `te::EcrireNotes` passent du texte au dict plat `chemin → {} | {valeur}`.
* `te::Tete nom` relit un tag sans `eval` et rend `{sorte attribut}`, `{}` pour un tag étranger
  (`sel`, un tag de l'application), une erreur pour un tag du document mal formé. `te::lire`
  valide ensuite l'attribut contre le type **actuel** (`type cast niveau`, `type cast nomstyle`) :
  un style retiré de la table, un titre de niveau 7 sont refusés, avec la position.
* Un nom peut contenir des espaces et des guillemets : toujours passer les tags à `insert` comme
  une liste (`[list $tag]`, `linsert`), jamais par un littéral (`lindex {a@c="x y"} 0` → `a@c="x`).
  Le nom ne commence jamais par `"`, donc pas d'erreur `extra characters after close-quote` (ni
  `list element in quotes followed by…`).

Limite (v16k) : l'écriture `T@notes` n'est qu'un affichage. Une annotation `{S.a@style=gras}x`
est refusée comme locale (comme `{S.a@gras}x`), et `=`, `"`, `(`, `\` y sont de toute façon
BADCHAR. Il faudrait que la TIP sache relire cette forme (`type parse`, piste 7).

## L'apparence : des tags d'aspect calculés

Tk ne cumule pas les polices : quand deux tags donnent `-font`, celui de plus haute priorité (le
dernier créé) remplace l'autre. Configurer `segment.marque@gras` et `segment.marque@italique`
ne peut donc pas donner « gras dans italique » ; un bloc configuré après les marques (`bloc.puces`)
les masquait même toutes, et un paragraphe sans style prenait la police par défaut du widget
(TkFixedFont).

Chaque morceau de texte reçoit donc **un seul** tag d'aspect, le cumul des champs écrits dans la
table des styles, du bloc vers le passage le plus intérieur (l'intérieur l'emporte sur un même
champ). Son nom est l'ensemble de ces champs, les champs de la police sous une branche :

```
aspect                                          le style par défaut
aspect@police.poids=bold
aspect@police.(pente=italic,poids=bold)         gras dans italique
aspect@couleur="#2b5c8a",marge=16,souligne=1    un lien dans une liste à puces
aspect@couleur="#1d2433",police.(poids=bold,taille=18)   un titre de niveau 1
```

* `te::Aspect w tags` calcule le nom et configure le tag au premier usage ; `te::rendre` le pose à
  chaque `insert`.
* `te::habiller w` refait tous les aspects d'après les tags du document, après une édition. La
  démo l'appelle après Ctrl-B / Ctrl-I, qui posent la marque sur la sélection, ou la retirent si
  toute la sélection l'a déjà.
* `te::lire` ignore les tags d'aspect : le document ne dépend que des tags `bloc.…`/`segment.…`.
* Un lien prend le style `lien` de la table, ou à défaut un bleu souligné.

## Relire le widget : `te::lire`

Le widget ne garde pas d'arbre : un B-arbre de lignes, et dans chaque ligne une suite plate de
textes et de bascules de tags. `te::lire w` refait le document à partir de
`$w dump -text -tag 1.0 end` :

1. les plages des tags `bloc.…` et `segment.…` ; les autres (`sel`, un tag de l'application)
   sont ignorés ;
2. les blocs sont les plages de `para`, `titre`, `puces`, `code` ; le texte hors de tout bloc
   (tapé à la main) devient des paragraphes ;
3. dans chaque bloc, les plages de `marque` et de `lien` sont emboîtées (début croissant, fin
   décroissante, avec une pile).

Deux plages qui se chevauchent (`<gras>a<ital>b</gras>c</ital>`, après une édition) ne forment pas
un arbre : `-chevauchement couper` (défaut) coupe la plage intérieure en deux,
`-chevauchement erreur` refuse avec les positions. Le document relu passe par les mêmes
constructeurs typés que ce qu'on écrit à la main.

L'aller-retour `te::lire` ∘ `te::rendre` est exact pour un document **normal** : pas deux textes
voisins (`"a" "b"` est relu `"ab"`), pas deux passages voisins de même style ni deux liens voisins
vers la même cible (le widget en fait une seule plage), rien de vide.

## Ce que les types apportent ici

* Un style mal orthographié, un titre de niveau 4 sont refusés **à la construction**, à la ligne
  qui les écrit, et pas au rendu.
* Le rendu n'a aucun test de forme : `type get` dit ce qu'est chaque bloc et chaque segment, le
  chemin lit ses parties.
* Changer la table des styles change le type `nomstyle` : les documents construits ensuite sont
  vérifiés contre la nouvelle table ; les anciens gardent leur type et se rendent toujours.

## Ce que l'exemple révèle (pistes)

1. **Messages d'erreur imbriqués trop vagues.** `te::m grass x` donne `value "grass x" is not
   valid for that variant of segment`, sans dire que `grass` n'est pas un `nomstyle` ni quels noms
   sont permis. Il faudrait que l'erreur la plus profonde remonte : `"grass" is not a nomstyle
   (gras, italique, ...)`, avec le chemin `marque.style`.
2. **La déduction de variante est dangereuse avec des sommes qui acceptent tout.** Un texte nu
   passé comme bloc (`te::document ... {pas un bloc}`) était accepté **sans erreur** comme
   `bloc.puces` : `list.list.segment` accepte n'importe quelle liste, car `segment.texte` accepte
   n'importe quelle chaîne. Réglé en v16n : `bloc` et `segment` sont définis `{-strict}`, la
   variante n'est jamais devinée (`value "pas un bloc" names no variant of the strict sum
   bloc`) ; les constructeurs `te::para`, `te::m`… la nomment. `type is {disjoint?}bloc` montre
   que toutes ses variantes se recouvrent.
3. **Construction programmatique par noms.** Dépassé : le chemin d'un produit se calcule
   (`set {style}s([join [dict keys $args] ,]) {*}[dict values $args]`, sans `eval`), et depuis la
   v16o il traverse aussi une somme. Reste : un dict n'entre pas dans un produit
   (`type cast style {poids bold}` : `needs 7 values`).
4. **Types dérivés d'une donnée.** `nomstyle` dépend de la table ; la refaire oblige à oublier et
   redéfinir `segment`, `bloc`, `document` (règle « forget both »). Un type paramétré par l'ensemble
   des noms serait plus naturel, mais les paramètres de type ne sont pas des listes de littéraux.
5. **Arguments de procédure.** Chaque constructeur répète `set {T}x ...` pour typer ce qu'il rend ;
   une procédure ne peut pas déclarer le type de ses arguments ni de son résultat.
6. **Une forme normale manque.** Le widget ne distingue pas `"a" "b"` de `"ab"`, ni deux passages
   gras voisins d'un seul : l'aller-retour n'est exact que pour un document normal. C'est la même
   idée que les expressions canoniques (et les listes canoniques) : une forme unique par valeur,
   ici « textes fusionnés, passages voisins de même style fusionnés, rien de vide ». Un
   `te::normaliser`, ou mieux un type qui n'admet que la forme normale, rendrait la relecture
   exacte par construction.
7. **Une forme texte des valeurs typées.** Deux formes, qui se complètent : `{T}contenu` (avec le
   contenu, de quoi refaire la valeur) et `T@notes` (sans le contenu : le type et ses
   métadonnées), employée ici. La syntaxe des notes de l'exemple (`,` `.` `=` `(…)` `"…"`) donne
   une forme canonique sans encodage ad hoc ; elle mériterait d'être celle de la TIP, avec
   `type canonic $v` et `type parse $texte` (voir `doc/PISTES-annotations-locales.md`). Les messages
   d'erreur sont plus précis qu'avec l'ancien type `balise` :
   `value "surligne" matches no variant of nomstyle`.

8. **Une variante de somme se construit par ses champs** (réglé en v16o). Avant, sa valeur était
   un seul argument : ni étiquettes, ni chemin. Maintenant `set {segment.lien}s {cible:}…
   {contenu:}…`, `set {segment}s(lien.(cible,contenu)) …`, et `type def segment.lien` rend ses
   champs. Le gabarit n'a plus de table.

Côté annotations (débat du 2 novembre) : toutes les annotations de cet exemple sont écrites sur des
mots que reçoit un constructeur (`set {segment}s {segment.marque}$x`). Les pistes B et C ne
changeraient rien ici ; l'exemple ne fait pas pencher la balance.
