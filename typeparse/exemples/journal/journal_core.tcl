# journal_core.tcl --
#
# Logique du journal, sans interface : le type "journal", la base SQLite et
# l'ajout d'un message. Utilisé par journal_gui.tcl ; testable seul.
# Exige le Tcl 9.0 avec les annotations de type (TIP 759b) et le paquet sqlite3.

# Un journal : l'horodatage (epoch) et le nom de la machine (host) sont
# calculés à CHAQUE création ; seul le message est fourni par l'appelant.
type def journal {
    {int}epoch = [clock seconds];
    {string}message;
    {string}host = [info hostname]
}

# Date lisible (heure locale), triable : 2026-10-14 18:13:05
proc date_lisible {epoch} {
    clock format $epoch -format {%Y-%m-%d %H:%M:%S}
}

# Ouvre (ou crée) la base et la table.
proc journal_ouvrir {fichier} {
    sqlite3 ::db $fichier
    db eval {
	CREATE TABLE IF NOT EXISTS journal (
	    message TEXT NOT NULL,
	    date    TEXT NOT NULL,
	    host    TEXT NOT NULL
	)
    }
}

# Crée un objet de type "journal" pour ce message, l'enregistre, et le rend.
proc journal_ajouter {texte} {
    set {journal}j {message}$texte
    set m $j(message)
    set d [date_lisible $j(epoch)]
    set h $j(host)
    db eval {INSERT INTO journal (message, date, host) VALUES (:m, :d, :h)}
    return $j
}

# Appelle script pour chaque ligne, avec les variables date, host, message.
proc journal_lire {script} {
    uplevel 1 [list ::db eval {SELECT date, host, message FROM journal ORDER BY rowid} \
	    $script]
}
