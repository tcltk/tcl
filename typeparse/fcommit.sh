#!/usr/bin/env bash
# fcommit.sh : "fossil commit" des seuls fichiers EDITED / ADDED.
#
# Pourquoi : un "fossil commit" sans noms de fichiers embarque tout ce que
# Fossil voit comme modifié, y compris les fichiers EXECUTABLE (bit x changé
# par un passage sous Cygwin/MinGW) qui déclenchent les avertissements
# binaire / CRLF. Ce script ne retient que EDITED et ADDED ; MISSING,
# DELETED, RENAMED, EXECUTABLE... sont laissés de côté et listés.
#
# Usage : bash typeparse/fcommit.sh [-n] [-y] [-x MOTIF]... -m "message"
#   -m MSG    message de commit (obligatoire)
#   -n        simulation : affiche la liste et la commande, ne commite pas
#   -y        ne demande pas confirmation
#   -x MOTIF  exclut les fichiers dont le chemin correspond à MOTIF
#             (expression régulière étendue ; option répétable)
#             ex. -x '\.zip$' -x '^win/'
set -euo pipefail

main() {
    local msg="" dry=0 yes=0 excl=()
    while [ $# -gt 0 ]; do
        case "$1" in
            -m) msg="${2:?-m demande un message}"; shift 2 ;;
            -n) dry=1; shift ;;
            -y) yes=1; shift ;;
            -x) excl+=("${2:?-x demande un motif}"); shift 2 ;;
            -h|--help) sed -n '2,19p' "$0"; return 0 ;;
            *) echo "option inconnue : $1" >&2; return 2 ;;
        esac
    done
    [ -n "$msg" ] || { echo "il faut un message : -m \"...\"" >&2; return 2; }
    command -v fossil >/dev/null || { echo "fossil introuvable dans le PATH" >&2; return 1; }

    # Les chemins de "fossil changes" sont relatifs à la racine du dépôt.
    local root
    root=$(fossil info | sed -n 's/^local-root:[[:space:]]*//p')
    [ -n "$root" ] || { echo "pas dans un dépôt Fossil ouvert" >&2; return 1; }
    cd "$root"

    local line st path files=() skipped=() p drop
    while IFS= read -r line; do
        st=${line%%[[:space:]]*}
        path=${line#"$st"}
        path=${path#"${path%%[![:space:]]*}"}      # retire les blancs de tête
        [ -n "$path" ] || continue
        case "$st" in
            EDITED|ADDED)
                drop=0
                for p in "${excl[@]+"${excl[@]}"}"; do
                    if printf '%s\n' "$path" | grep -Eq -- "$p"; then drop=1; break; fi
                done
                if [ "$drop" = 1 ]; then skipped+=("exclu    $path"); else files+=("$path"); fi
                ;;
            *) skipped+=("$st  $path") ;;
        esac
    done < <(fossil changes)

    if [ ${#skipped[@]} -gt 0 ]; then
        echo "Laissés de côté (${#skipped[@]}) :"
        printf '  %s\n' "${skipped[@]}"
    fi
    if [ ${#files[@]} -eq 0 ]; then
        echo "Aucun fichier EDITED / ADDED à commiter."
        return 0
    fi
    echo "À commiter (${#files[@]}) :"
    printf '  %s\n' "${files[@]}"

    if [ "$dry" = 1 ]; then
        echo "(simulation) fossil commit -m \"$msg\" <${#files[@]} fichiers>"
        return 0
    fi
    if [ "$yes" = 0 ]; then
        read -r -p "fossil commit ces ${#files[@]} fichiers ? [o/N] " r
        case "$r" in o|O|y|Y) ;; *) echo "annulé"; return 1 ;; esac
    fi
    fossil commit -m "$msg" "${files[@]}"
}
main "$@"
