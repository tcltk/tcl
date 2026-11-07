# libc.tcl -- ffi : des fonctions de la bibliothèque C, appelées depuis leur
# signature typée.
#
#   tclsh libc.tcl ?COMPILATEUR?        (gcc par défaut ; tcc, ou un chemin)
#
# Linux : libc.so.6 et libm.so.6 ; Windows : ucrtbase.dll ; macOS :
# libSystem.dylib.
source [file join [file dirname [file normalize [info script]]] ffi.tcl]
if {$argc > 0} {cproc::config -cc [lindex $argv 0]}

switch -- $tcl_platform(os) {
    Linux   {set libc libc.so.6; set libm libm.so.6}
    Darwin  {set libc libSystem.dylib; set libm $libc}
    default {set libc ucrtbase.dll; set libm $libc}
}

proc montre {script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERREUR $r" }
    puts [format "%-36s -> %s" [string trim $script] $r]
}

puts "--- des nombres et un texte"
ffi::fn $libm {double}hypot {{double}x; {double}y}
ffi::fn $libc {size_t}strlen {{string(utf-8,z)}s}
montre {hypot 3 4}
montre {strlen héllo}                 ;# 6 : les octets de l'UTF-8
montre {hypot 3 x}                    ;# la signature refuse, avant le C
montre {strlen}

puts "\n--- un char * en retour"
ffi::fn $libc {string(utf-8,z)}getenv {{string}nom}
set env(FFI_ESSAI) bonjour
montre {getenv FFI_ESSAI}
montre {getenv FFI_ABSENT}            ;# NULL : la chaîne vide

puts "\n--- une struct en sortie : var.T"
# struct timespec { time_t tv_sec; long tv_nsec; } ; long a 32 bits sous
# Windows, 64 sous Linux et macOS : c_long le dit
type def timespec {{int(s,64)}sec; {c_long}nsec}
ffi::fn $libc {c_int}timespec_get {{var.timespec}ts; {c_int}base ?= 1}
montre {timespec_get t}               ;# 1 : TIME_UTC
montre {type get $t}
montre {expr {abs($t(sec) - [clock seconds]) <= 2}}
montre {expr {$t(nsec) >= 0 && $t(nsec) < 1000000000}}

puts "\n--- un double en sortie"
ffi::fn $libm {double}modf {{double}x; {var.double(64)}entier}
montre {list [modf 3.25 e] $e}

puts "\n--- un rappel : qsort et une lambda typée"
# int (*)(const void *, const void *) : ref.T, l'élément pointé, lu
ffi::callback {c_int}compare {{ref.c_int}a; {ref.c_int}b}
# void qsort(void *base, size_t n, size_t size, compare f) : le tableau est
# la variable, n et size des options calculées
ffi::fn $libc qsort {{var.list.c_int}base; {size_t}-n ?= [llength $base];
                     {size_t}-size ?= [type binary {sizeof?}c_int];
                     {compare}f}
set L {5 3 9 1 7}
montre {qsort L {{{c_int}a; {c_int}b} {expr {$a - $b}}}; set L}
montre {qsort L {{{c_int}a; {c_int}b} {expr {$b - $a}}}; set L}
montre {qsort -n 3 L {{{c_int}a; {c_int}b} {expr {$a - $b}}}; set L}   ;# les 3 premiers
montre {qsort L {{{c_int}a; {c_int}b} {error oups}}}
montre {set L}                        ;# une erreur du rappel : L intacte
montre {set M {1 x 2}; qsort M {{{c_int}a; {c_int}b} {expr {$a - $b}}}}
