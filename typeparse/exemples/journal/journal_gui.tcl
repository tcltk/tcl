# journal_gui.tcl --
#
# Petit journal : un champ de saisie (text), un bouton ttk « Envoyer » qui
# crée un objet de type "journal" et l'ajoute à la table SQLite
# (message, date lisible, host), et un affichage en lecture seule.
#
# Usage : tclsh journal_gui.tcl ?base.db?

package require Tk
package require sqlite3
source [file join [file dirname [info script]] journal_core.tcl]

journal_ouvrir [expr {$argc ? [lindex $argv 0] : "journal.db"}]

wm title . "Journal"

# --- saisie ---------------------------------------------------------------
ttk::frame .f -padding 8
pack .f -fill both -expand 1

ttk::label .f.l1 -text "Message :"
text .f.saisie -height 4 -width 60 -wrap word
ttk::button .f.envoyer -text "Envoyer" -command envoyer

# --- affichage (lecture seule) ---------------------------------------------
ttk::label .f.l2 -text "Journal :"
text .f.log -height 14 -width 60 -wrap word -state disabled
ttk::scrollbar .f.sb -orient vertical -command {.f.log yview}
.f.log configure -yscrollcommand {.f.sb set}
.f.log tag configure date -foreground #1a5fb4
.f.log tag configure host -foreground #26a269
.f.log tag configure message -lmargin1 2m -lmargin2 2m

grid .f.l1      -row 0 -column 0 -columnspan 2 -sticky w
grid .f.saisie  -row 1 -column 0 -columnspan 2 -sticky ew
grid .f.envoyer -row 2 -column 0 -columnspan 2 -sticky e -pady 6
grid .f.l2      -row 3 -column 0 -columnspan 2 -sticky w
grid .f.log     -row 4 -column 0 -sticky nsew
grid .f.sb      -row 4 -column 1 -sticky ns
grid columnconfigure .f 0 -weight 1
grid rowconfigure    .f 4 -weight 1

# --- comportement ---------------------------------------------------------
proc afficher {date host message} {
    .f.log configure -state normal
    .f.log insert end "$date " date "$host\n" host "$message\n" message
    .f.log see end
    .f.log configure -state disabled
}

proc envoyer {} {
    set texte [string trim [.f.saisie get 1.0 end]]
    if {$texte eq ""} {
	return
    }
    set j [journal_ajouter $texte]
    afficher [date_lisible $j(epoch)] $j(host) $j(message)
    .f.saisie delete 1.0 end
    focus .f.saisie
}

bind .f.saisie <Control-Return> {envoyer; break}

# Historique déjà en base.
journal_lire {afficher $date $host $message}
focus .f.saisie
