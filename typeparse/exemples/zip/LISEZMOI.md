# zipmem — une archive zip lue en mémoire, et sourcée

Un souhait exprimé sur la liste de diffusion : pouvoir écrire `source app.zip`.
Si une archive zip a un type binaire qu'on peut reconnaître, il suffit que
`source` la reconnaisse :

```tcl
source zipmem.tcl
zipmem::install
source app.zip           ;# main.tcl de l'archive ; return y finit le script
```

`main.tcl` voit `info script` = `…/app.zip/main.tcl`, et un `source [file join
[file dirname [info script]] lib/util.tcl]` lit `lib/util.tcl` dans l'archive,
en mémoire. Un `source` de fichier ordinaire passe à la commande d'origine.

## Le format, écrit en types

`zipmem.tcl` décrit le format (APPNOTE.TXT de PKWARE, § 4.3) par les types
binaires de `layout/filebinary.tcl` (§ 9 de `ANNOTATIONS.fr.md`). Les types
sont dans l'espace de noms `::zipmem` (v16u) :

```tcl
namespace eval ::zipmem {
    type def sig_eocd {"PK\x05\x06"}
    type def eocd {
        {sig_eocd}sig;
        {u16le}disk;   {u16le}cddisk;
        {u16le}nhere;  {u16le}n;
        {u32le}cdsize; {u32le}cdoff;
        {u16le}clen;   {string(utf-8,$clen)}comment
    }
    type def entry { … {u16le}nlen; {u16le}xlen; {u16le}clen; …
                     {u32le}local;
                     {string(utf-8,$nlen)}name; {string(raw,$xlen)}extra;
                     {string(utf-8,$clen)}comment }
    type def local { … }
    type def eocd64 { … }  ;  type def loc64 { … }   ;# zip64
}
```

- Une **constante** (`"PK\x05\x06"`) est une somme d'un littéral, vérifiée au
  décodage.
- Une **taille lue dans le fichier** est un champ déjà lu
  (`string(utf-8,$nlen)`).

Le répertoire central se lit en un appel : `type binary decode
"{list($n).::zipmem::entry}e" $tranche`. Chaque entrée est une valeur
`::zipmem::entry`, et ses champs se lisent par chemin (`$x(method)`,
`$x(csize)`).

**Reconnaître une archive** : `zipmem::is $octets` cherche, dans les 64 derniers
Ko, une signature `PK\5\6` qui se décode en `::zipmem::eocd` (`type binary
{is?}::zipmem::eocd`) et dont le commentaire va jusqu'au bout. Pour `source`,
il faut que le fichier commence par `PK\3\4`, ou qu'il ait l'extension `.zip`,
puis que ce type se reconnaisse.

## L'API

```tcl
zipmem::is     OCTETS           ;# 1 si c'est une archive
zipmem::open   OCTETS           ;# l'archive : dict {data base comment entries}
zipmem::names  Z                ;# les membres
zipmem::info   Z MEMBRE         ;# size compressed method crc
zipmem::read   Z MEMBRE         ;# les octets (inflate, CRC et taille vérifiés)
zipmem::text   Z MEMBRE ?ENC?   ;# le texte (utf-8 par défaut)
zipmem::source CHEMIN ?MEMBRE?  ;# sourcer MEMBRE (main.tcl)
zipmem::source -data OCTETS ?MEMBRE?   ;# une archive reçue en mémoire
zipmem::install                 ;# source reconnaît les archives
```

Une archive peut suivre d'autres octets (un exécutable, comme un starkit) : ses
adresses partent de son propre début, que `zipmem::open` calcule (`base`).

## Essais

`tclsh test.tcl` (sortie : `test.out`). Les archives testées :

- `app.zip` : deflate, stored, nom UTF-8, commentaire ;
- `flux.zip` : écrite d'un trait (`zip - -`), avec les enregistrements zip64 ;
- une archive de `zipfs mkzip` ;
- une archive placée derrière 5 000 octets ;
- une archive vide.

Info-ZIP (`zip -r`, avec et sans `-fd`) et `zipfile` de Python ont aussi été
essayés.

Sur une archive de tcllib (5 455 membres, 16 Mo), mesurée sous Linux :

| | zipmem | zipfs (Tcl 9) |
|---|---:|---:|
| ouvrir l'archive (en mémoire) | 14 ms | 26 ms (`zipfs mountdata`) |
| lire un membre (45 Ko) | 0,16 ms | 0,21 ms |

Trois points ont compté pour la vitesse. Avant eux, l'ouverture prenait
1,35 s et la lecture d'un membre 11 ms.

- **Décoder dans une tranche.** Le répertoire, ou les 30 octets d'un en-tête,
  sont décodés dans une tranche (`string range`), pas dans les 16 Mo de
  l'archive.
- **Emballer la liste dans un produit.** La voie C de `type binary decode` ne
  prend qu'un produit ; une liste seule passe par la couche Tcl. C'est 75 fois
  plus rapide.
- **Une adresse plutôt qu'un lien.** L'adresse de l'en-tête local est un entier.
  Un lien `*(//,u32le).local` le dirait mieux, mais un lien lu sans être suivi
  (`-linkdepth 0`) passe aussi par la couche Tcl.

Au passage, un `at(N).list(...)` en tête de `type binary decode` n'était pas
rangé comme une liste : c'est corrigé dans `filebinary.tcl` (`::fb::typed`).

## zipmem et zipfs

Tcl 9 a **zipfs**, un vrai système de fichiers virtuel. `zipfs mount` monte
une archive sur disque, `zipfs mountdata` une archive en mémoire ; ensuite
`source //zipfs:/app/main.tcl`, `glob`, `open`, `file exists` et `load` y
marchent. zipmem n'est pas un système de fichiers : seul `source` voit
l'archive. Voici ce qu'il montre :

- **le format écrit en types**, lisible et vérifié (signatures, tailles,
  zip64) ;
- **`source app.zip` directement**, sans montage, parce que l'archive se
  reconnaît à son type ;
- **une archive reçue en mémoire** (un socket, une base de données) lue sans
  passer par un nom de montage.

## Limites

- Pas de chiffrement, pas de zip64 au niveau des entrées (tailles de plus de
  4 Go), pas d'archive en plusieurs volumes.
- Méthodes stored (0) et deflate (8) seulement.
- Les noms sont lus en UTF-8 : un vieux nom cp437 non ASCII serait mal lu.
- Les archives sourcées restent ouvertes, en mémoire, pour que les `source`
  suivants (un paquet chargé plus tard) les trouvent.
- `glob`, `open` et `file exists` ne voient pas l'archive : c'est le rôle de
  zipfs.
