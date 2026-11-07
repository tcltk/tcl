# Mise au point par les notes (v16y)

- `pas_a_pas.tcl` : une valeur notée `{@step}` est suivie dans `list`, `foreach`, jusqu'au
  paramètre d'une procédure typée ; l'invite `step>` (`h` pour l'aide) permet de continuer,
  d'avancer pas à pas (`s`), d'afficher une variable (`p NOM`), d'exécuter une commande sur place.
- `proto_types.tcl` : des notes (`{pointt}`, `{vitesse}`) posées avant que les types existent ;
  `type debug report` dit où elles ont été vues, propose `point` pour `pointt`, et vérifie les
  valeurs vues quand `vitesse` est défini.
- `console.tcl` : la même chose dans la console Tk (`type debug console`) : la pile, les
  variables du cadre choisi avec leur type et leurs notes, le journal, une ligne de commande ;
  et une note conditionnelle (`type debug when neg {$value < 0}`) qui ne s'arrête que sur une
  valeur négative. `console.png` : la console arrêtée dans `norme`.

Voir `doc/ANNOTATIONS.fr.md`, § 3.1.
