#!/bin/bash
# maj.sh -- met à jour typeparse/ depuis typeparse.zip, applique les patchs, recompile, teste.
#           updates typeparse/ from typeparse.zip, applies patches, rebuilds, tests.
#
# À lancer depuis la RACINE de l'arbre Tcl, sous MinGW64 :
#   bash typeparse/maj.sh [options] [typeparse.zip]
#
# Options :
#   -n, --dry-run   ne change rien : montre ce qui serait fait
#   -y, --yes       ne pose aucune question
#       --no-build  ne recompile pas
#       --no-test   ne lance pas les tests
#       --commit "message"   après des tests réussis, commite les seuls fichiers touchés
#                            (fossil commit sur la liste nommée) puis propose le push
# Variables : TCLSH (défaut app/bin/tclsh90.exe), TCLWIN (dossier de build, défaut win),
#             JOBS (défaut 4)
#
# Version : typeparse/VERSION (celle de l'archive) ; le tclsh installé la donne par
#           tcl::unsupported::typesversion (v16b et suivantes ; avant : tclsh typeparse/version.tcl).
#           Le script affiche les deux au début, et vérifie après compilation qu'elles concordent.
#
# Étapes : 1. extrait l'archive dans un dossier temporaire et ne remplace dans typeparse/ que
#             les fichiers qui DIFFÈRENT (cmp) ; liste ce qui est nouveau/modifié/absent de l'archive
#          2. sources de types (tclTypes*.{c,h}, tclAnnotObj.c, tclTypedVar.c, tclTypePath.c,
#             tclTypeGeneric.c, tclTypeParse.c, tclVarRef.c) : livrées ENTIÈRES dans typeparse/,
#             copiées dans generic/ quand elles diffèrent ; elles ne sont jamais patchées
#          3. patchs de typeparse/noyau/ORDRE.txt, partie NOYAU seulement (les sections qui visent
#             les sources de types sont ignorées), sans fuzz (-F0) : déjà appliqué (patch -R
#             --dry-run passe) -> ignoré ; applicable -> appliqué ; sinon -> arrêt avec le détail.
#             Sans -F0, un patch qui n'ajoute que des lignes pouvait se réappliquer ailleurs
#             avec du fuzz (taille-dynamique, 18 octobre : CheckDynCounts en double)
#          4. si un fichier C vient de changer (ou --build) : make clean, make, make install-binaries
#          5. tests : chaque typeparse/tests_tclsh/tNN_*.tcl comparé à son .out
#          6. (--commit) fossil commit sur les fichiers touchés, puis question avant fossil push

main() {
    local DRY=0 YES=0 BUILD=auto TEST=1 MSG="" ZIP="" FORCEBUILD=0
    while [ $# -gt 0 ]; do
        case "$1" in
            -n|--dry-run) DRY=1 ;;
            -y|--yes) YES=1 ;;
            --no-build) BUILD=0 ;;
            --build) FORCEBUILD=1 ;;
            --no-test) TEST=0 ;;
            --commit) shift; MSG="$1" ;;
            -h|--help) sed -n '2,35p' "$0"; return 0 ;;
            *) ZIP="$1" ;;
        esac
        shift
    done
    ZIP="${ZIP:-typeparse.zip}"
    local TCLSH="${TCLSH:-app/bin/tclsh90.exe}" BUILDDIR="${TCLWIN:-win}" JOBS="${JOBS:-4}"
    local ROOT; ROOT="$(pwd)"
    case "$TCLSH" in /*|[A-Za-z]:*) TCLABS="$TCLSH" ;; *) TCLABS="$ROOT/$TCLSH" ;; esac
    [ -f "$ZIP" ] || { echo "Archive introuvable : $ZIP (à lancer depuis la racine de l'arbre)"; return 1; }
    [ -d generic ] || { echo "Pas de dossier generic/ ici : lancer depuis la racine de l'arbre Tcl"; return 1; }
    local missing=""
    for c in unzip patch find sed grep tr sort mktemp; do
        command -v "$c" >/dev/null 2>&1 || missing="$missing $c"
    done
    if [ -n "$missing" ]; then
        echo "Outils manquants :$missing"
        echo "  pacman -S unzip patch findutils sed grep coreutils   (selon ce qui manque)"
        return 1
    fi
    # comparaison de fichiers : cmp (diffutils) si présent, sinon md5sum
    if command -v cmp >/dev/null 2>&1; then
        same() { cmp -s "$1" "$2"; }
    elif command -v md5sum >/dev/null 2>&1; then
        same() { [ "$(md5sum < "$1")" = "$(md5sum < "$2")" ]; }
    else
        echo "Ni cmp ni md5sum : pacman -S diffutils"; return 1
    fi

    # version compilée dans un tclsh : « v16b », ou « antérieure à v16b » sans la commande
    tclversion() {
        [ -f "$TCLABS" ] || { echo "(pas de $TCLSH)"; return; }
        echo 'puts [expr {[info commands ::tcl::unsupported::typesversion] eq "" ? "antérieure à v16b (tclsh typeparse/version.tcl pour la deviner)" : [::tcl::unsupported::typesversion]}]' \
            | "$TCLABS" 2>/dev/null | tr -d '\r'
    }

    ask() { [ "$YES" = 1 ] && return 0; read -r -p "$1 [o/N] " a; [ "$a" = o ] || [ "$a" = O ] || [ "$a" = y ]; }
    local TOUCHED; TOUCHED="$(mktemp)"

    echo "== 1. Archive $ZIP"
    local TMP; TMP="$(mktemp -d)"
    unzip -q "$ZIP" -d "$TMP" || return 1
    [ -d "$TMP/typeparse" ] || { echo "L'archive ne contient pas de dossier typeparse/"; return 1; }
    local VARCH="?" VAVANT
    [ -f "$TMP/typeparse/VERSION" ] && VARCH="$(tr -d '\r\n' < "$TMP/typeparse/VERSION")"
    VAVANT="$(tclversion)"
    echo "   version de l'archive   : $VARCH"
    echo "   version de $TCLSH : ${VAVANT:-?}"
    local nnew=0 nmod=0 f rel
    while IFS= read -r -d '' f; do
        rel="${f#$TMP/}"
        case "$rel" in *.so|*.dll|*.o|*.exe) continue ;; esac
        if [ ! -e "$rel" ]; then
            echo "   nouveau   $rel"; nnew=$((nnew+1))
        elif ! same "$f" "$rel"; then
            echo "   modifié   $rel"; nmod=$((nmod+1))
        else
            continue
        fi
        if [ "$DRY" = 0 ]; then mkdir -p "$(dirname "$rel")"; cp "$f" "$rel"; echo "$rel" >> "$TOUCHED"; fi
    done < <(find "$TMP/typeparse" -type f -print0)
    # fichiers présents chez vous mais plus dans l'archive : signalés, jamais supprimés
    while IFS= read -r -d '' f; do
        rel="${f#./}"
        [ -e "$TMP/$rel" ] || case "$rel" in *.so|*.dll|*.o|*.exe|*.orig|*.rej) ;; *) echo "   absent de l'archive (conservé) $rel" ;; esac
    done < <(find typeparse -type f -print0 2>/dev/null)
    echo "   $nnew nouveau(x), $nmod modifié(s)"
    rm -rf "$TMP"

    echo "== 2. Sources de types -> generic/"
    local applied_now=0 p line
    local TYPEFILES="tclAnnotObj.c tclTypes.c tclTypeParse.c tclTypeGeneric.c tclTypedVar.c tclTypePath.c tclVarRef.c tclTypes.h tclTypesInt.h"
    for f in $TYPEFILES; do
        [ -f "typeparse/$f" ] || { echo "   MANQUE   typeparse/$f"; return 1; }
        if [ ! -f "generic/$f" ] || ! same "typeparse/$f" "generic/$f"; then
            echo "   copié           generic/$f"
            if [ "$DRY" = 0 ]; then
                cp "typeparse/$f" "generic/$f"; echo "generic/$f" >> "$TOUCHED"; applied_now=1
            fi
        fi
    done

    # Partie NOYAU d'un patch : les sections dont le fichier n'est pas une source de types.
    # Une section commence à « diff ... » ou à une ligne « --- » suivie de « +++ ».
    corepart() {
        awk -v skip=" $TYPEFILES " '
            function keepfor(l,   a, n) {
                sub(/^\+\+\+ /, "", l); sub(/[ \t].*/, "", l)
                n = split(l, a, "/"); return index(skip, " " a[n] " ") == 0
            }
            { L[NR] = $0; sub(/\r$/, "", L[NR]) }
            END {
                keep = 0
                for (i = 1; i <= NR; i++) {
                    if (L[i] ~ /^diff /) {
                        for (j = i + 1; j <= NR && L[j] !~ /^\+\+\+ /; j++) ;
                        keep = (j <= NR) ? keepfor(L[j]) : 0
                    } else if (L[i] ~ /^--- / && i < NR && L[i+1] ~ /^\+\+\+ / \
                            && (i == 1 || L[i-1] !~ /^diff /)) {
                        keep = keepfor(L[i+1])
                    }
                    if (keep) print L[i]
                }
            }' "$1"
    }

    echo "== 3. Patchs C (partie noyau, sans fuzz)"
    if [ -f typeparse/noyau/ORDRE.txt ]; then
        local CORE; CORE="$(mktemp)"
        while IFS= read -r line; do
            p="${line%%#*}"; p="${p//$'\r'/}"
            p="${p#"${p%%[![:space:]]*}"}"; p="${p%"${p##*[![:space:]]}"}"
            [ -n "$p" ] || continue
            if [ ! -f "typeparse/noyau/$p" ]; then echo "   MANQUE   $p"; return 1; fi
            corepart "typeparse/noyau/$p" > "$CORE"
            if [ ! -s "$CORE" ]; then
                echo "   types seuls     $p (sources copiées à l'étape 2)"
                continue
            fi
            # fichiers cibles : tous doivent exister (sinon patch poserait une question)
            local missing=""
            for f in $(grep -E '^\+\+\+ ' "$CORE" | sed -E 's/^\+\+\+ [ab]\///; s/[[:space:]].*//'); do
                [ -f "$f" ] || missing="$missing $f"
            done
            if [ -n "$missing" ]; then
                echo "   CONFLIT         $p : fichier(s) absent(s) de l'arbre :$missing"
                rm -f "$CORE"; return 1
            fi
            local out
            if patch -p1 -R --dry-run -F0 -s < "$CORE" >/dev/null 2>&1; then
                echo "   déjà appliqué   $p"
            elif out="$(patch -p1 -N --dry-run -F0 < "$CORE" 2>&1)"; then
                echo "   à appliquer     $p"
                if [ "$DRY" = 0 ]; then
                    patch -p1 -N -F0 -s < "$CORE" || { echo "ÉCHEC patch $p"; rm -f "$CORE"; return 1; }
                    grep -E '^\+\+\+ ' "$CORE" | sed -E 's/^\+\+\+ [ab]\///; s/[[:space:]].*//' >> "$TOUCHED"
                    applied_now=1
                fi
            else
                # Déjà appliqué, mais un patch SUIVANT a retouché le contexte de
                # ses lignes : -R ne passe plus à l'identique, et patch reconnaît
                # chaque fichier comme « Reversed (or previously applied) ».
                local nfiles nrev
                nfiles=$(printf '%s\n' "$out" | grep -c '^checking file')
                nrev=$(printf '%s\n' "$out" | grep -c 'Reversed (or previously applied)')
                if [ "$nrev" -gt 0 ] && [ "$nrev" = "$nfiles" ] \
                        && ! printf '%s\n' "$out" | grep -q 'FAILED'; then
                    echo "   déjà appliqué   $p (contexte retouché par un patch suivant)"
                else
                    echo "   CONFLIT         $p (ni appliqué ni applicable sans fuzz) :"
                    echo "$out" | sed 's/^/      /'
                    rm -f "$CORE"; return 1
                fi
            fi
        done < typeparse/noyau/ORDRE.txt
        rm -f "$CORE"
    else
        echo "   (pas de typeparse/noyau/ORDRE.txt)"
    fi

    if [ "$DRY" = 1 ]; then echo "== (dry-run) arrêt avant compilation et tests"; return 0; fi

    echo "== 4. Compilation"
    if [ "$BUILD" = 0 ]; then echo "   ignorée (--no-build)"
    elif [ "$applied_now" = 0 ] && [ "$FORCEBUILD" = 0 ] && [ -f "$TCLSH" ] \
            && [ -n "$(find generic -name '*.[ch]' -newer "$TCLSH" 2>/dev/null | head -1)" ]; then
        # FR : rien de copié cette fois, mais une source de generic/ est plus récente que le
        #      tclsh installé (compilation précédente sautée ou interrompue) : on recompile.
        echo "   $(find generic -name '*.[ch]' -newer "$TCLSH" | head -1) est plus récent que $TCLSH"
        FORCEBUILD=1
    fi
    if [ "$BUILD" = 0 ]; then :
    elif [ "$applied_now" = 1 ] || [ "$FORCEBUILD" = 1 ]; then
        # garde-fou : le dossier de build doit être sous l'arbre et contenir un Makefile Tcl
        case "$BUILDDIR" in /*|[A-Za-z]:*|*..*) echo "   TCLWIN doit être un dossier relatif à la racine de l'arbre : $BUILDDIR"; return 1 ;; esac
        if [ ! -f "$BUILDDIR/Makefile" ] || [ ! -f "$BUILDDIR/../generic/tcl.h" ]; then
            echo "   $BUILDDIR n'est pas un dossier de build Tcl (Makefile ou ../generic/tcl.h absent)"; return 1
        fi
        ask "   make clean && make && make install-binaries dans $BUILDDIR ?" && {
            ( cd "$BUILDDIR" && make clean >/dev/null 2>&1 && make -j"$JOBS" && make install-binaries ) \
                || { echo "ÉCHEC de la compilation ou de l'installation (un tclsh ou un wish ouvert ?)"; return 1; }
            # garde-fou : sous Windows, un exécutable ouvert (« resource busy ») peut empêcher la
            # copie ; on vérifie que chaque binaire installé est bien celui qu'on vient de construire
            local bindir b stale=0
            bindir="$(dirname "$TCLABS")"
            for b in "$BUILDDIR"/tclsh*.exe "$BUILDDIR"/*tcl*.dll; do
                [ -f "$b" ] || continue
                [ -f "$bindir/$(basename "$b")" ] || continue
                same "$b" "$bindir/$(basename "$b")" || { echo "   NON INSTALLÉ : $bindir/$(basename "$b") (fichier occupé ?)"; stale=1; }
            done
            [ "$stale" = 0 ] || { echo "   Fermer les tclsh/wish ouverts, puis : (cd $BUILDDIR && make install-binaries)"; return 1; }
            local VAPRES; VAPRES="$(tclversion)"
            if [ "$VAPRES" = "$VARCH" ]; then
                echo "   version installée : $VAPRES"
            else
                echo "   ATTENTION : le tclsh installé dit « $VAPRES », l'archive est la $VARCH"
            fi
        }
    else
        echo "   rien de C à recompiler (--build pour forcer)"
    fi

    local fails=0
    echo "== 5. Tests"
    if [ "$TEST" = 0 ]; then echo "   ignorés (--no-test)"
    elif [ ! -x "$TCLSH" ] && [ ! -f "$TCLSH" ]; then echo "   $TCLSH introuvable : définir TCLSH"; fails=1
    else
        local t o n=0 fo fe
        fo="$(mktemp)"; fe="$(mktemp)"
        for t in typeparse/tests_tclsh/t*.tcl; do
            o="${t%.tcl}.out"; [ -f "$o" ] || o="$t.out"; [ -f "$o" ] || continue
            ( cd typeparse/tests_tclsh && "$TCLABS" "$(basename "$t")" 2>&1 ) | tr -d '\r' > "$fo"
            tr -d '\r' < "$o" > "$fe"
            if same "$fo" "$fe"; then
                n=$((n+1))
            else
                echo "   ECHEC $(basename "$t")"; fails=$((fails+1))
                if command -v diff >/dev/null 2>&1; then
                    diff "$fe" "$fo" | head -6 | sed 's/^/        /'
                fi
            fi
        done
        rm -f "$fo" "$fe"
        echo "   $n test(s) OK, $fails échec(s)  (version $(tclversion))"
    fi

    echo "== 6. Fossil"
    sort -u "$TOUCHED" -o "$TOUCHED"
    echo "   fichiers touchés par ce script :"; sed 's/^/     /' "$TOUCHED"
    if [ -n "$MSG" ]; then
        [ "$fails" = 0 ] || { echo "   tests en échec : pas de commit"; return 1; }
        ask "   fossil commit -m \"$MSG\" sur ces fichiers ?" && {
            fossil add $(cat "$TOUCHED") 2>/dev/null
            fossil commit -m "$MSG" $(cat "$TOUCHED") || return 1
            ask "   fossil push ?" && fossil push
        }
    else
        echo "   (pas de --commit : à faire à la main, en nommant les fichiers ci-dessus)"
    fi
    rm -f "$TOUCHED"
    return "$fails"
}

# Appel et sortie sur UNE ligne : bash la lit d'un coup, puis sort, sans relire ce fichier
# que l'étape 1 vient peut-être de remplacer (sinon : « syntax error » sur la fin du nouveau).
main "$@"; exit $?
