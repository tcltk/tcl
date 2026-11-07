# tclTypeDebug.tcl -- v16y : le crochet par défaut de « type debug ».
#
# Source de tclTypeDebug.h (une chaîne C, chargée par « type debug on ») :
#     tclsh gen_typedebug.tcl
# Chargé une fois par interpréteur (garde sur ::tcl::typedebug::hit).
#
#   hit NOTE MOTS INDICE   appelé par la trace avant une commande dont
#                          l'argument INDICE porte la note NOTE (0 : un
#                          arrêt « type debug next », note @next)
#   when ?NOM ?EXPR??      les conditions des notes @NOM
#   console ?on|off?       la console Tk (sinon l'invite sur stdin)
#   report INV NOMS        le texte de « type debug report »

if {[info commands ::tcl::typedebug::hit] eq {}} {
namespace eval ::tcl::typedebug {
    variable run 0		;# 1 : plus d'arrêt (r)
    variable when {}		;# NOM -> expression
    variable hits {}		;# NOM -> passages
    variable ui stdin		;# stdin | tk
    variable action {}		;# console : ce que l'utilisateur a choisi
    variable stop {}		;# console : l'arrêt courant (dict)

    proc short {v {n 60}} {
        set s [string map {\n { } \t { }} $v]
        if {[string length $s] > $n} {set s [string range $s 0 $n-4]...}
        return $s
    }

    # Le lieu de la commande tracée : UP cadres sous l'appelant de where.
    proc where {{up 3}} {
        set n [info frame]
        for {set k [expr {$n - $up}]} {$k > 0} {incr k -1} {
            set f [info frame $k]
            if {[dict exists $f file] && [dict exists $f line]} {
                return "[file tail [dict get $f file]]:[dict get $f line]"
            }
        }
        return ?
    }

    # type debug when ?NOM ?EXPR?? : sans argument, toutes les conditions ;
    # EXPR vide : retire la condition.
    proc when {args} {
        variable when
        switch [llength $args] {
            0 {return $when}
            1 {
                set n [string trimleft [lindex $args 0] @]
                return [expr {[dict exists $when $n] ? [dict get $when $n] : {}}]
            }
            2 {
                lassign $args n e
                set n [string trimleft $n @]
                if {$e eq {}} {
                    dict unset when $n
                } else {
                    # l'expression doit au moins se compiler
                    if {[catch {apply [list {value cmd arg hits} [list expr $e]] 0 {} {} 0} msg opt]
                            && [lrange [dict get $opt -errorcode] 0 1] eq {TCL PARSE}} {
                        return -code error "type debug when: $msg"
                    }
                    dict set when $n $e
                }
                return
            }
        }
        return -code error {wrong # args: should be "type debug when ?note ?expr??"}
    }

    proc reset {} {
        variable run 0
        variable hits {}
    }

    proc hit {note words index} {
        variable run
        variable when
        variable hits
        set name [string range $note 1 end]
        dict incr hits $name
        set cmd [lindex $words 0]
        set full [uplevel 1 [list namespace which -command $cmd]]
        set lv [expr {[info level] - 1}]
        set in [expr {$lv > 0 ? "[lindex [info level $lv] 0]" : "(global)"}]
        set value {}
        if {$index == 0} {
            set what {}
            set line "$note  [short $words]"
        } else {
            set value [lindex $words $index]
            set what "argument $index"
            if {$full ne "" && [info procs $full] ne ""} {
                set a [info args $full]
                if {$index - 1 < [llength $a]} {set what [lindex $a [expr {$index - 1}]]}
                set cmd $full
            }
            set line "$note  $cmd $what = [short $value]"
        }
        # une condition : la note ne se signale que si elle est vraie
        if {[dict exists $when $name]} {
            if {[catch {apply [list {value cmd arg hits} [list expr [dict get $when $name]]] \
                    $value $cmd $what [dict get $hits $name]} ok]} {
                set line "$line   (condition: $ok)"
            } elseif {![string is true -strict $ok]} {
                return
            }
        }
        set at [where]
        set line "$line   ($at, in $in)"
        set stop [expr {$note ne "@trace" && !$run}]
        variable ui
        if {$ui eq "tk" && [winfo exists .tcltypedebug]} {
            return [TkHit $line $stop [dict create note $note cmd $cmd \
                    what $what value $value words $words at $at in $in level $lv]]
        }
        puts stderr $line
        if {!$stop} return
        while 1 {
            puts -nonewline stderr "step> "; flush stderr
            if {[gets stdin line] < 0} {set run 1; return}
            set line [string trim $line]
            switch -- $line {
                {} - c {return}
                s {type debug next; return}
                r {set run 1; return}
                o {set run 0; type debug off; return}
                q {return -code error "type debug: stopped at $cmd"}
                w {for {set k $lv} {$k > 0} {incr k -1} {puts stderr "  $k: [short [info level $k]]"}; continue}
                v {puts stderr "  [uplevel 1 {info locals}]"; continue}
                h - ? {puts stderr {  c: continue  s: step (stop at the next command)  r: run (no more pauses)  o: debug off  q: stop (error)  w: where  v: local variables  p NAME: a variable  other: a Tcl command, run here}; continue}
            }
            if {[lindex $line 0] eq "p" && [llength $line] == 2} {
                set line [list set [lindex $line 1]]
            }
            if {[catch {uplevel 1 $line} r]} {puts stderr "  error: $r"} else {puts stderr "  $r"}
        }
    }

    # ---------------------------------------------------------------- Tk

    # type debug console ?on|off?
    proc console {{how on}} {
        variable ui
        switch -- $how {
            on {
                if {[catch {package require Tk} msg]} {
                    return -code error "type debug console: Tk is not available ($msg)"
                }
                TkBuild
                set ui tk
            }
            off {
                set ui stdin
                destroy .tcltypedebug
                variable action c	;# un arrêt en cours continue
            }
            default {return -code error {wrong # args: should be "type debug console ?on|off?"}}
        }
        return
    }

    proc TkBuild {} {
        set w .tcltypedebug
        if {[winfo exists $w]} {wm deiconify $w; raise $w; return}
        toplevel $w
        wm title $w "type debug"
        wm geometry $w 860x600
        wm protocol $w WM_DELETE_WINDOW {::tcl::typedebug::console off}

        ttk::frame $w.top -padding 6
        ttk::label $w.top.where -text "(en marche)" -font TkHeadingFont -anchor w
        ttk::label $w.top.value -text "" -anchor w -font TkFixedFont
        pack $w.top.where $w.top.value -side top -fill x

        ttk::frame $w.bar -padding {6 0}
        foreach {b label key} {c Continuer F5 s Pas F10 r Courir F8 q Arrêter F12 o Couper {}} {
            ttk::button $w.bar.$b -text [expr {$key ne {} ? "$label ($key)" : $label}] \
                -command [list ::tcl::typedebug::TkAct $b]
            pack $w.bar.$b -side left -padx 2
            if {$key ne {}} {bind $w <$key> [list ::tcl::typedebug::TkAct $b]}
        }

        ttk::panedwindow $w.pw -orient vertical
        ttk::panedwindow $w.pw.h -orient horizontal
        ttk::labelframe $w.pw.h.stack -text Pile -padding 2
        listbox $w.pw.h.stack.l -exportselection 0 -font TkFixedFont -width 30 -activestyle none
        pack $w.pw.h.stack.l -fill both -expand 1
        bind $w.pw.h.stack.l <<ListboxSelect>> {::tcl::typedebug::TkAct vars}
        ttk::labelframe $w.pw.h.vars -text Variables -padding 2
        ttk::treeview $w.pw.h.vars.t -columns {type value} -height 8
        $w.pw.h.vars.t heading #0 -text nom -anchor w
        $w.pw.h.vars.t heading type -text {type {notes}} -anchor w
        $w.pw.h.vars.t heading value -text valeur -anchor w
        $w.pw.h.vars.t column #0 -width 110 -stretch 0
        $w.pw.h.vars.t column type -width 120 -stretch 0
        $w.pw.h.vars.t column value -width 300
        ttk::scrollbar $w.pw.h.vars.s -command [list $w.pw.h.vars.t yview]
        $w.pw.h.vars.t configure -yscrollcommand [list $w.pw.h.vars.s set]
        pack $w.pw.h.vars.s -side right -fill y
        pack $w.pw.h.vars.t -fill both -expand 1
        $w.pw.h add $w.pw.h.stack -weight 1
        $w.pw.h add $w.pw.h.vars -weight 3

        ttk::labelframe $w.pw.log -text Journal -padding 2
        text $w.pw.log.t -height 10 -font TkFixedFont -wrap none -state disabled
        ttk::scrollbar $w.pw.log.s -command [list $w.pw.log.t yview]
        $w.pw.log.t configure -yscrollcommand [list $w.pw.log.s set]
        $w.pw.log.t tag configure stop -foreground #b00000
        $w.pw.log.t tag configure out -foreground #005a9e
        $w.pw.log.t tag configure err -foreground #b00000
        pack $w.pw.log.s -side right -fill y
        pack $w.pw.log.t -fill both -expand 1
        $w.pw add $w.pw.h -weight 2
        $w.pw add $w.pw.log -weight 1

        ttk::frame $w.cmd -padding 6
        ttk::label $w.cmd.l -text "Tcl :"
        ttk::entry $w.cmd.e -font TkFixedFont
        bind $w.cmd.e <Return> {::tcl::typedebug::TkAct eval}
        pack $w.cmd.l -side left
        pack $w.cmd.e -side left -fill x -expand 1 -padx {4 0}

        pack $w.top -side top -fill x
        pack $w.bar -side top -fill x
        pack $w.cmd -side bottom -fill x
        pack $w.pw -side top -fill both -expand 1 -padx 6
        TkState 0
    }

    proc TkLog {text {tag {}}} {
        set t .tcltypedebug.pw.log.t
        $t configure -state normal
        $t insert end $text\n $tag
        $t see end
        $t configure -state disabled
    }

    proc TkState {stopped} {
        set w .tcltypedebug
        foreach b {c s r q} {
            $w.bar.$b state [expr {$stopped ? {!disabled} : {disabled}}]
        }
        $w.cmd.e state [expr {$stopped ? {!disabled} : {disabled}}]
        if {!$stopped} {
            $w.top.where configure -text "(en marche)"
            $w.top.value configure -text ""
            $w.pw.h.stack.l delete 0 end
            $w.pw.h.vars.t delete [$w.pw.h.vars.t children {}]
        }
    }

    # Appelé par hit, au niveau de l'appelant de la commande tracée.
    proc TkHit {line stop info} {
        variable action
        variable stopInfo $info
        if {!$stop} {
            TkLog $line
            return
        }
        TkLog $line stop
        set w .tcltypedebug
        set lv [dict get $info level]
        $w.top.where configure -text "[dict get $info note]  [short [dict get $info words] 70]   ([dict get $info at], in [dict get $info in])"
        if {[dict get $info what] ne {}} {
            set v [dict get $info value]
            $w.top.value configure -text "[dict get $info what] = [short $v 70]   [TkTypeOf $v]"
        } else {
            $w.top.value configure -text ""
        }
        set l $w.pw.h.stack.l
        $l delete 0 end
        for {set k $lv} {$k > 0} {incr k -1} {
            $l insert end "$k  [short [info level $k] 40]"
        }
        $l insert end "0  (global)"
        $l selection set 0
        TkVars
        TkState 1
        wm deiconify $w; raise $w
        focus $w.cmd.e
        # Les gestionnaires d'événements tournent au niveau global : ils ne
        # voient pas la pile arrêtée. Ils posent une action ; c'est ici,
        # dans la pile, qu'on lit les variables et qu'on évalue.
        while 1 {
            set action {}
            vwait ::tcl::typedebug::action
            if {![winfo exists $w]} {set action c; break}
            switch -- $action {
                vars {TkVars; continue}
                eval {TkEval; continue}
            }
            break
        }
        if {[winfo exists $w]} {TkState 0}
        switch -- $action {
            s {type debug next}
            r {variable run 1}
            o {variable run 0; type debug off}
            q {return -code error "type debug: stopped at [dict get $info cmd]"}
        }
        return
    }

    proc TkAct {a} {
        variable action
        set action $a
    }

    # Le type et les notes d'une valeur, pour l'affichage.
    proc TkTypeOf {v} {
        set t [type get $v]
        set n [note get $v]
        if {$n ne {} && $n ne $t} {return [string trim "$t {$n}"]}
        return $t
    }

    # Le niveau choisi dans la pile (#k).
    proc TkLevel {} {
        set l .tcltypedebug.pw.h.stack.l
        set s [$l curselection]
        if {$s eq {}} {set s 0}
        return [lindex [$l get $s] 0]
    }

    proc TkVars {} {
        set t .tcltypedebug.pw.h.vars.t
        $t delete [$t children {}]
        set k [TkLevel]
        if {$k == 0} {
            set names [lsort [lmap n [info globals] {
                if {[string match tcl_* $n] || [string match auto_* $n]
                    || $n in {env errorInfo errorCode argv argv0 argc}} continue
                set n
            }]]
        } else {
            set names [lsort [uplevel #$k {info locals}]]
        }
        foreach n $names {
            if {[uplevel #$k [list array exists $n]]} {
                $t insert {} end -text $n -values [list array "([uplevel #$k [list array size $n]] éléments)"]
            } elseif {[uplevel #$k [list info exists $n]]} {
                set v [uplevel #$k [list set $n]]
                $t insert {} end -text $n -values [list [TkTypeOf $v] [short $v 80]]
            } else {
                $t insert {} end -text $n -values [list {} (indéfinie)]
            }
        }
    }

    proc TkEval {} {
        set e .tcltypedebug.cmd.e
        set script [$e get]
        if {$script eq {}} return
        $e delete 0 end
        TkLog "% $script"
        if {[catch {uplevel #[TkLevel] $script} r]} {
            TkLog "  $r" err
        } else {
            TkLog "  $r" out
        }
        TkVars
    }

    # ------------------------------------------------------------ rapport

    proc distance {a b} {
        set la [string length $a]; set lb [string length $b]
        set prev {}; for {set j 0} {$j <= $lb} {incr j} {lappend prev $j}
        for {set i 1} {$i <= $la} {incr i} {
            set cur [list $i]
            for {set j 1} {$j <= $lb} {incr j} {
                set c [expr {[string index $a $i-1] ne [string index $b $j-1]}]
                lappend cur [expr {min([lindex $prev $j] + 1, [lindex $cur end] + 1, [lindex $prev $j-1] + $c)}]
            }
            set prev $cur
        }
        lindex $prev end
    }

    proc near {name names} {
        set best {}; set bd 3
        foreach t [lsort $names] {
            set d [distance [string tolower $name] [string tolower $t]]
            if {$d < $bd && $d <= max(1, [string length $name] / 3)} {set best $t; set bd $d}
        }
        return $best
    }

    proc report {inv names} {
        if {![dict size $inv]} {return "no notes seen"}
        set out {}
        foreach name [lsort [dict keys $inv]] {
            set e [dict get $inv $name]
            set n [dict get $e count]
            set l "$name: seen $n time[expr {$n > 1 ? {s} : {}}], first at [dict get $e where], via [join [dict get $e commands] {, }]"
            append l "\n    values: [join [lmap p [dict get $e samples] {short [lindex $p 0]}] { | }]"
            switch [dict get $e status] {
                ok {append l "\n    now a type: the values seen fit it"}
                mismatch {
                    append l "\n    now a type, but:"
                    foreach p [dict get $e errors] {append l "\n      [short [lindex $p 0]] ([lindex $p 1]): [lindex $p 2]"}
                }
                default {
                    set t [near $name $names]
                    if {$t ne {}} {append l "\n    not a type; did you mean $t?"} else {append l "\n    not a type yet"}
                }
            }
            lappend out $l
        }
        join $out \n
    }
}
}
