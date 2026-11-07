# gen_typedebug.tcl : tclTypeDebug.tcl -> tclTypeDebug.h (v16y)
#   tclsh gen_typedebug.tcl   (dans le dossier des sources de types)
set dir [file dirname [info script]]
set f [open [file join $dir tclTypeDebug.tcl]]; fconfigure $f -encoding utf-8
set src [read $f]; close $f
set out "/* Généré par gen_typedebug.tcl depuis tclTypeDebug.tcl : ne pas modifier. */\n"
append out "static const char typeDebugScript\[\] =\n"
foreach line [split [string trimright $src \n] \n] {
    append out "\"[string map {\\ \\\\ \" \\\" \t \\t ?? ?\\?} $line]\\n\"\n"
}
append out ";\n"
set f [open [file join $dir tclTypeDebug.h] w]; fconfigure $f -encoding utf-8 -translation lf
puts -nonewline $f $out; close $f
