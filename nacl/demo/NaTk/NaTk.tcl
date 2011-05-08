# NaTk - Tk emulation under NaTcl
#
# Colin McCormack
proc ::puts {args} {
    if {[llength $args] == 1} {
        printf "STDOUT: [lindex $args 0]"
    } else {
        printf "[string toupper [lindex $args 0]]: [lindex $args 1]"
    }
}

set _packages {mime base64}
proc package {cmd args} {
    printf "PKG: package $cmd $args"
    switch -- $cmd {
        require {
            lassign $args what
            if {$what in {Tcl Tk TclOO}} return
            if {$what in $::_packages} return
            source lib/Utilities/[lindex $args 0].tcl
        }
        provide {
            lassign $args what
            lappend ::_packages $what
        }
        vsatisfies {
            return 1
        }
    }
}
source tcllib/textutil/string.tcl
source tcllib/textutil/repeat.tcl
source tcllib/textutil/adjust.tcl
source tcllib/textutil/split.tcl
source tcllib/textutil/tabify.tcl
source tcllib/textutil/trim.tcl
source tcllib/textutil/textutil.tcl
source lib/extensions/extend-1.0.tm
source lib/extensions/file-1.0.tm
source lib/extensions/memoize-1.0.tm
source lib/extensions/know-1.0.tm

package require Form

namespace import oo::*
package provide NaTk 1.0

# radiobutton - textvariable command variable value
# entry - textvariable validatecommand
# scale - command variable
# checkbutton - textvariable command variable
# button - textvariable command

# TextVariable: radiobutton entry checkbutton button
# Command: radiobutton scale checkbutton button +validate:entry
# Variable: radiobutton scale checkbutton

# NaTk - class connecting an interp to an NaTcl window
class create ::NaTk {
    superclass FormClass

    method toplevel {args} {
        variable widgets
        if {[llength $args]} {
            {*}[dict get $widgets .] {*}$args
        } else {
            return [dict get $widgets .]
        }
    }

    method widget {widget} {
        variable widgets; return [dict get $widgets $widget]
    }

    method widgets {} {
        variable widgets; return $widgets
    }

    method mkwidget {type widget args} {
        variable ns
        variable interp
        variable widgets

        Debug.widget {mkwidget $widget of type $type ($args)}

        if {$widget ne "."} {
            set parent [join [lrange [split $widget .] 0 end-1] .]
            if {$parent eq ""} {
                set parent .
            }
            if {![dict exists $widgets $parent]} {
                error "Invalid Widget Name - no parent widget '$parent' exists for '$widget'"
            }
            set p [list -parent [dict get $widgets $parent]]	;# parent object
        } elseif {[dict exists $widgets .]} {
            error "Invalid Widget - Toplevel . already exists"
        } else {
            set p {-parent ""}
        }

        # construct a widget of given type
        Debug.widget {::NaTk::$type new -widget $widget $args -interp $interp $p -connection [self]}
        set cmd [::NaTk::$type new -widget $widget {*}$args -interp $interp {*}$p -connection [self]]
        return $widget

        if {[catch {
            ::NaTk::$type new -widget $widget {*}$args -interp $interp {*}$p -connection [self]
        } cmd eo]} {
            Debug.widget {FAILED $widget of type '$type' as obj: $cmd ($eo)}
            return -options $eo $cmd
        } else {
            Debug.widget {created $widget of type '$type' as obj:$cmd}
        }

        return $widget
    }

    # addwidget - to both Interp and mapping
    method addwidget {widget cmd} {
        Interp alias $widget $cmd ;# install widget alias in Interp

        # remember name<->command mapping
        variable widgets
        dict set widgets $widget $cmd
        dict set widgets $cmd $widget
    }

    # delwidget - from both Interp and mapping
    method delwidget {obj} {
        Interp alias $widget {}

        # forget name<->command mapping
        variable widgets
        set widget [dict get $widgets $obj]
        dict unset widgets $widget
        dict unset widgets $obj
    }

    method grid {args} {
        Debug.grid {NATK GRID: $args}
        if {[string index [lindex $args 0]] in {x . ^}} {
            set args [linsert $args 0 configure]
        }
        set args [lassign $args command]
        switch -- $command {
            anchor -
            bbox -
            columnconfigure -
            info -
            location -
            propagate -
            rowconfigure -
            size -
            slaves {
                set anchor {}
                set args [lassign $args widget]
                tailcall [dict get $widgets $widget] grid $command $widget {*}$args
            }

            configure -
            forget -
            remove -
            {
                set slaves {}
                while {[string index [lindex $args 0] 0] in {x . ^}} {
                    set args [lassign $args slave]
                    lappend slaves $slave
                }
                variable widgets
                set result {}
                foreach slave $slaves {
                    lappend result $slave
                    lappend result [[dict get $widgets $slave] grid $command $slave {*}$args]
                }
                return $result
            }
        }
    }

    method interp {script} {
        Debug.interp "INTERP: $script"
        return [Interp eval $script]
    }

    method start {script} {
        variable verbose
        set ::nacl::verbose $verbose

        Debug.interp "INTERP: $script"
        Interp eval $script
        my toplevel render
    }

    method bgerror {args} {
        Debug.interp "INTERP BGERROR: $args"
        ::nacl bgerror {*}$args [Interp info errorstack]
    }

    constructor {args} {
        Debug.widgets {[self] Creating ::NaTk $args}
        variable safe 0
        variable verbose 1
        variable {*}$args
        variable widgets {}
        variable ns [info object namespace [self]]

        # create an interpreter for NaTk program
        variable interp [::interp create {*}[expr {$safe?"-safe":""}] ${ns}::Interp]
        Interp alias ::bgerror [self] bgerror
        Interp alias ::alert ::nacl alert

        my mkwidget toplevel .

        # install each widget creator as alias in the interp
        foreach n {button entry label frame
            checkbutton radiobutton scale
            select combobox
        } {
            Interp alias $n [self] mkwidget $n
        }
        Interp alias $n grid [self] grid

        #::dom get [dict get $::argv script] [self] start
    }
}

class create ::NaTk::Widget {
    variable id props widget interp parent connection rendered

    method js {} {return ""}

    method element {} {
        return $id;
        #return "document.getElementById('$id')"
        return "\$('#$id')"	;# if jQuery
    }

    # catch unknown methods and palm them off to the parent
    method unknown {cmd args} {
        Debug.widgets {unknown $cmd $args}
        if {[string match <*> $cmd]} {
            return [{*}$parent $cmd {*}$args]
        } elseif {[string match .* $cmd]} {
            return [$cmd {*}$args]
        }
        error "Unknown method '$cmd' in [self] of class [info object class [self]]"
    }

    method connection {args} {
        return [{*}$parent {*}$args]
    }

    # grid - percolates up to the parent containing a grid
    method grid {args} {
        Debug.grid {percolating grid to $parent: grid $args}
        tailcall $parent grid {*}$args
    }

    # gridded - record grid's values
    method gridded {args} {
        variable grid
        if {[llength $args]} {
            set grid [dict merge $grid $args]
        }
        return $grid
    }

    # Grid - interpret -grid options to configure as grid commands
    method Grid {args} {
        # this widget needs to be gridded
        set rs 1; set cs 1; set r 0; set c 0
        Debug.widgets {config gridding: '$args'}
        set pargs [lsearch -glob $args -*]
        if {$pargs > -1} {
            # we've got extra args
            set gargs [lrange $args $pargs end]
            set args [lrange $args 0 $pargs-1]
        } else {
            set gargs {}
        }
        lassign $args r c rs cs

        set ga {}
        foreach {v1 v2} {r row c column rs rowspan cs columnspan} {
            if {[info exists $v1] && [set $v1] ne ""} {
                if {$v1 in {rs cs}} {
                    if {[set $v1] <= 0} {
                        set $v1 1
                    }
                } else {
                    if {[set $v1] < 0} {
                        set $v1 0
                    }
                }
                lappend ga -$v2 [set $v1]
            }
        }

        Debug.widgets {option -grid: 'grid configure $widget $ga $gargs'}
        return [my grid configure $widget {*}$ga {*}$gargs]
    }

    # style - make an HTML style form
    method style {gridding} {
        set attrs {}
        foreach {css tk} {
            background-color background
            color foreground
            text-align justify
            vertical-align valign
            border borderwidth
            border-color bordercolor
            width width
        } {
            set tkval [dict get? $props $tk]
            if {$tkval eq ""} continue

            if {$tk eq "background"} {
                lappend attrs background "none $tkval !important"
                if {![dict exists $props bordercolor]} {
                    dict set attrs border-color $tkval
                }
                # TODO: background images, URLs
            } else {
                dict set attrs $css $tkval
            }
        }

        if {0} {
            # process -sticky gridding
            set sticky [dict gridding.sticky?]
            if {$sticky ne ""} {
                # we have to use float and width CSS to emulate sticky
                set sticky [string trim [string tolower $sticky]]
                set sticky [string map {n "" s ""} $sticky];# forget NS
                if {[string first e $sticky] > -1} {
                    dict set attrs float "left"
                } elseif {[string first w $sticky] > -1} {
                    dict set attrs float "right"
                }

                if {$sticky in {"ew" "we"}} {
                    # this is the usual case 'stretch me'
                    dict set attrs width "100%"
                }
            }
        }

        # todo - padding
        set result ""
        dict for {n v} $attrs {
            append result "$n: $v;"
        }
        append result [dict gridding.style?]

        if {$result ne ""} {
            set result [list style $result]
        }

        Debug.widgets {style attrs:($attrs), style:($result)}

        if {[dict exists $props class]} {
            lappend result class [dict get $props class]
        }

        if {[dict exists $props state]
            && [dict get $props state] ne "normal"
        } {
            lappend result disabled 1
        }

        return $result
    }

    # getvalue - return a widget's textvariable or text, where applicable
    method getvalue {} {
        if {[dict exists $props textvariable]} {
            set result [my iget? [dict get $props textvariable]]
            Debug.widgets {[self] getvalue from textvariable:'[dict get $props textvariable]' value:'$result'}
        } elseif {[dict exists $props text]} {
            set result [dict get $props text]
            Debug.widgets {[self] getvalue from text '$result'}
        } else {
            Debug.widgets {[self] getvalue default to ""}
            set result ""
        }
        return $result
    }

    # compound - return a widget's image/value compound
    method compound {args} {
        if {[llength $args] == 1} {
            set text [lindex $args 0]
            Debug.widget {[self] compound explicit '$text'}
        } else {
            set text [my getvalue]
            Debug.widget {[self] compound implicit '$text'}
        }
        set text [armour $text]

        set image [dict get? $props image]
        if {$image ne ""} {
            set image [$image render]
        }
        set result ""
        switch -- [dict get? $props compound] {
            left {
                set result $image$text
            }
            right {
                set result $text$image
            }
            center -
            top {
                set result "$image[my <br>]$text"
            }
            bottom {
                set result "$text[my <br>]$image"
            }
            none -
            default {
                # image instead of text
                if {$image ne ""} {
                    set result "$image"
                } else {
                    set result "$text"
                }
            }
        }
        Debug.widget {[self] compound ($args) -> $result}
        return $result
    }

    # access interp variables
    method iset {n v} {
        Debug.interp {iset '$n' <- '$v' traces:([{*}$interp eval [list trace info variable $n]]) ([lrange [info level -1] 0 1])}
        return [{*}$interp eval [list ::set $n $v]]
    }

    method iget? {n} {
        try {
            Debug.widgets {iget $n: {*}$interp eval ::if \{\[info exists $n\]\} \{set $n\}}
            set result [{*}$interp eval ::if \{\[info exists $n\]\} \{set $n\}]
        } on error {e eo} {
            set result ""
        }

        Debug.interp {iget '$n' -> '$result' ([lrange [info level -1] 0 1])}
        return $result
    }

    method iget {n} {
        try {
            set result [{*}$interp eval [list ::set $n]]
        } on error {e eo} {
            set result ""
        }

        Debug.interp {iget '$n' -> '$result' ([lrange [info level -1] 0 1])}
        return $result
    }

    method iexists {n} {
        set result [{*}$interp eval [list ::info exists $n]]
        Debug.interp {iexists '$n' -> $result}
        return $result
    }

    # itrace - trace an Interp variable
    method itrace {what args} {
        variable trace	;# set of active traces for this widget
        if {[llength $args]} {
            # add a trace
            dict set trace $what $args
            {*}$interp eval [list trace add variable $what write $args]
            Debug.interp {itrace add $what $args: ([{*}$interp eval [list trace info variable $what]])}
        } elseif {[dict exists $trace $what]} {
            {*}$interp eval [list trace remove variable $what write [dict get $trace $what]]
            Debug.interp {itrace removed $what $args ([dict get $trace $what]) leaving ([{*}$interp eval [list trace info variable $what]])}
            dict unset trace $what
        }
    }

    # Configure - reflect configuration in $props dict
    # give trace-effect to -textvariable and -variable opts
    method Configure {args} {
        # set configuration opts
        Debug.widgets {[info coroutine] Configure [self] ($args)/[llength $args]}
        if {[llength $args]%2} {
            error "Configure requires even number of args"
        }

        # clean the '-' prefix from property names
        dict for {n v} $args {
            if {[string match -* $n]} {
                dict unset args $n
                dict set args [string trim $n -] $v
            }
        }

        dict for {n v} $args {
            switch -- $n {
                variable -
                textvariable {
                    # remove any old traces first
                    if {[dict get? $props $n] ne ""} {
                        my itrace [dict get $props $n]
                    }

                    dict set props $n $v

                    if {$v ne ""} {
                        # (re-)establish variable trace
                        Debug.widgets {tracking -$n '$v' changes}
                        if {![my iexists $v]} {
                            # create variable if necessary
                            {*}$interp eval [list ::variable ::$v]
                        }

                        if {[dict exists $args text]} {
                            my iset $v [dict get $args text]
                        }

                        # track changes to textvariable
                        my itrace [dict get $props $n] $widget $n
                    } else {
                        catch {dict unset props $n}
                    }
                }

                grid {
                    my Grid {*}$v
                }

                default {
                    # normal old configuration variable
                    dict set props $n $v
                }
            }
        }
    }

    method configure {args} {
        switch -- [llength $args] {
            0 {
                # get the names of all configuration opts
                set _result {};
                foreach v $props {
                    lappend result -$v [dict get $props $v]
                }
                return $result
            }

            1 {
                # get the value of a single configuration opt
                set v [string trimleft [llength $args 0] -]
                return [dict get $props $v]
            }

            default {
                # handle configure with sensitivity to -command, -variable, etc
                tailcall my Configure {*}$args
            }
        }
    }

    destructor {
        $connection delwidget [self]	;# remove full widget name
    }

    method id {} {
        return $id
    }

    method nacl {args} {
        if {!$rendered} return
        set script [join $args \;\n]
        ::nacl js $script
    }

    method render {args} {
        incr rendered
    }

    constructor {args} {
        variable grid {}
        set id [namespace tail [self]]
        set rendered 0	;# not yet rendered

        # get structural vars from NaTk mkwidget
        foreach v {widget interp parent connection} {
            set $v [dict get $args -$v]
            dict unset args -$v
        }
        Debug.widgets "create $widget ([self])  parent: $parent"

        # add widget to Interp and mapping
        $connection addwidget $widget [self]

        set props {}		;# no properties yet
        variable trace {}	;# no traces yet
        my Configure {*}$args	;# fill up props
    }
}

class create ::NaTk::button {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    # button - textvariable command

    # textvariable - interp's textvariable has changed
    method textvariable {} {
        variable ignore; if {$ignore} return
        set val [my iget [dict get $props textvariable]]
        my nacl [my element].innerHTML='[my compound]'
    }

    method changed {} {
        set cmd [dict get? $props command]
        if {$cmd ne ""} {
            {*}$interp eval $cmd
        }
    }

    method render {args} {
        next
        set event [list onclick "tcl(\"[self]\",\"changed\");"]
        return [my <button> $id id $id class button {*}$event {*}[my style $args] [my compound]]
    }

    constructor {args} {
        next justify left {*}$args
    }
}

class create ::NaTk::entry {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    # entry - textvariable validatecommand

    # textvariable - interp's textvariable has changed
    # reflect change onto browser widget
    method textvariable {args} {
        variable ignore; if {$ignore} return
        set val [my iget [dict get $props textvariable]]
        my nacl [my element].value='$val'
    }

    method changed {value} {
        # the entry value has changed
        # TODO call validation cmd
        variable ignore; incr ignore
        my iset [dict get $props textvariable] $value
        incr ignore -1
    }

    method render {args} {
        next
        Debug.widgets {[info coroutine] rendering Entry [self]}

        switch -- [dict get? $props type] {
            password {
                set tag <password>
            }
            date {
                set tag <text>
                #append js [my element].datepicker();
            }
            default {
                set tag <text>
            }
        }
        set event [list onchange "tcl(\"[self]\",\"changed\",[my element].value);return false;"]
        set result [my $tag $id id $id class var {*}$event {*}[my style $args] size [dict get $props width] [tclarmour [my getvalue]]]
        return $result
    }

    constructor {args} {
        variable ignore 1
        next {*}[dict merge {
            justify left
            state normal width 16
        } $args]
        if {![dict exists $props textvariable]} {
            my Configure textvariable [lindex [split $widget .] end]
        }
        if {[dict exists $props show] && ![dict exists $props type]} {
            my configure -type password
        }
        incr ignore -1
    }
}

class create ::NaTk::label {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    # label - textvariable text

    method textvariable {args} {
        variable ignore; if {$ignore} return
        Debug.widget {label $id: getvalue: '[my getvalue]'}
        my nacl [my element].innerHTML='[my compound]'
    }

    method render {args} {
        next
        return [my <div> id $id class label {*}[my style $args] [my compound]]
    }

    constructor {args} {
        variable ignore; incr ignore
        next justify left {*}$args
        if {![dict exists $props textvariable]} {
            my Configure textvariable [lindex [split $widget .] end]
        }
        incr ignore -1
    }
}

class create ::NaTk::checkbutton {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    # checkbutton - textvariable command variable

    method textvariable {} {
        variable ignore; if {$ignore} return
        set val [my iget [dict get $props textvariable]]
        my nacl [my element].whatever
    }

    # changed - button state in browser - reflect to var/command
    method changed {value} {
        variable ignore; incr ignore
        my iset [dict get $props variable] $value
        incr ignore -1
        if {[dict exists $props command]} {
            {*}$interp eval [dict get $props command]
        }
    }

    method js {} {
        return "\$('#$id'>:checkbox').change(cbuttonTk);"
    }

    method render {args} {
        next
        Debug.widgets {checkbutton render: getting '[dict get $props variable]' == [my iget [dict get $props variable]]}

        set val [my iget [dict get $props variable]]
        if {$val ne "" && $val} {
            set checked 1
        } else {
            set checked 0
        }

        Debug.widgets {[self] checkbox render: checked:$checked}
        return [my <checkbox> $id id $id {*}[my style $args] checked $checked [tclarmour [my compound]]]
    }

    constructor {args} {
        variable ignore 0
        next justify left {*}$args
        if {![dict exists $props variable]} {
            my Configure variable [lindex [split $widget .] end]
        }
    }
}

# rbC - pseudo-widget which handles change events for a set of
# radiobuttons sharing the same variable.
class create ::NaTk::rb {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    method render {args} {
        next
        error "Can't render an rbC"
    }

    constructor {args} {
        next {*}$args	;# set up traces etc
    }
}

class create ::NaTk::radiobutton {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    # radiobutton - textvariable command variable value

    method textvariable {} {
        variable ignore; if {$ignore} return
        set val [my iget [dict get $props textvariable]]
        my nacl [my element].whatever
    }

    method variable {} {
        set val [my iget [dict get $props textvariable]]
        my nacl [my element].whatever
    }

    # changed - button state in browser - reflect to var/command
    method changed {value} {
        variable ignore; incr ignore
        my iset [dict get $props variable] $value
        incr ignore -1
        if {[dict exists $props command]} {
            {*}$interp eval [dict get $props command]
        }
    }

    method js {} {
        return "[my element].click(rbuttonTk);"
    }

    method update {args} {
        Debug.widgets {radiobutton render: getting '[dict get $props variable]' == [my iget [dict get $props variable]]}

        set checked 0
        set var [dict get $props variable]
        if {[my iexists $var]} {
            set val [my iget $var]
            if {$val eq [dict get $props value]} {
                set checked 1
            }
        }

        Debug.widgets {[self] radiobox render: checked:$checked}
        set result [my <radio> [[my connection rbvar $var] widget] id $id class rbutton {*}[my style $args] checked $checked value [dict get $props value] [dict args.label?]]
        Debug.widgets {RADIO html: $result}
        return $result
    }

    method render {args} {
        next
        set label [tclarmour [my compound]]
        return [my update {*}$args label $label]
    }

    constructor {args} {
        variable ignore 0
        next justify left {*}$args

        if {![dict exists $props variable]} {
            my Configure variable selectedButton
        }
    }
}

class create ::NaTk::scale {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    # scale - command variable

    # scale value has changed in browser
    method changed {value} {
        variable ignore; incr ignore
        my iset [dict get $props variable] $value
        incr ignore -1
        if {[dict exists $props command]} {
            {*}$interp eval [dict get $props command]
        }
    }

    # variable - interp var has changed - reflect to browser widget
    method variable {} {
        variable ignore; if {$ignore} return
        set val [my iget [dict get $props variable]]
        my nacl [my element].whatever
    }

    # js - javascript to track changes to the browser widget
    method js {} {
        # need to generate the slider interaction
        lappend opts orientation '[dict get $props orient]'
        lappend opts min [dict get $props from]
        lappend opts max [dict get $props to]
        lappend opts value [my iget [dict get $props variable]]
        lappend opts change [string map [list %ID% $id] {
            function (event,ui) {
                sliderTk(event, '%ID%', ui);
            }
        }]

        return "[my element].slider([::nacl opts {*}$opts]); [my element].change(sliderTk);"
    }

    method render {args} {
        next
        Debug.widgets {scale $id render $args}
        set result ""
        if {[dict get? $props label] ne ""} {
            set result [my <label> [dict get $props label]]
        }

        append result [my <div> id $id class slider {*}[my style $args] {}]
        Debug.widgets {scale $id rendered '$result'}

        return $result
    }

    constructor {args} {
        variable ignore 0
        set var [lindex [split [dict get $args -name] .] end]
        next {*}[dict merge [list -variable $var] {
            justify left state active
            label "" command "" -length 100 -width 10 -showvalue 0
            -from 0 -to 100 -tickinterval 0
            -orient horizontal
        } $args]
    }
}

class create ::NaTk::select {
    superclass ::NaTk::Widget
    variable id props widget interp parent connection rendered

    method textvariable {} {
        variable ignore; if {$ignore} return
        set val [my iget [dict get $props textvariable]]
        my nacl [my element].value='$val'
    }

    # variable - interp var has changed - reflect to browser widget
    method variable {} {
        variable ignore; if {$ignore} return
        set val [my iget [dict get $props variable]]
        my nacl [my element].value='$val'
    }

    # select value has changed in browser
    method changed {value} {
        variable ignore; incr ignore
        my iset [dict get $props variable] $value
        incr ignore -1
        if {[dict exists $props command]} {
            {*}$interp eval [dict get $props command]
        }
    }

    method render {args} {
        next
        set var [dict get $props textvariable]
        if {[my iexists $var]} {
            set val [my iget $var]
        }

        set values [dict get $props values]
        Debug.widgets {select: val=$val values=$values}
        foreach opt $values {
            lappend opts [my <option> value [tclarmour $opt]]
        }
        set opts [join $opts \n]

        set class {variable ui-widget ui-state-default ui-corner-all}
        if {[dict exists $props combobox]
            && [dict get $props combobox]
        } {
            lappend class combobox
        }

        set event [list onclick "tcl(\"[self]\",\"changed\",[my element].value);return false;"]

        return [my <select> $id id $id {*}$event class [join $class] {*}[my style $args] $opts]
    }

    constructor {args} {
        variable ignore 0
        next justify left {*}$args
        if {![dict exists $props variable]} {
            my Configure variable [lindex [split $widget .] end]
        }
    }
}

class create ::NaTk::combobox {
    superclass ::NaTk::select
    variable id props widget interp parent connection rendered

    method render {} {
        next
        Debug.widgets {combobox}
        set result [next]
        my nacl [my element].combobox()
        return $result
    }

    constructor {args} {
        next {*}$args -combobox 1
    }
}

# grid store grid info in an x/y array gridLayout(column.row)
class create ::NaTk::grid {
    variable id props widget interp parent connection rendered

    # traverse grid looking for changes.
    method changes {r} {
        if {[dict exists $r -repaint]} {
            # repaint short-circuits change search
            #Debug.grid {Grid '[namespace tail [self]]' repainting}
            return [list $r {}]
        }

        variable grid;variable oldgrid
        #Debug.grid {Grid detecting changes: '[namespace tail [self]]'}

        # look for modified grid entries, these will cause a repaint
        dict for {row rval} $grid {
            dict for {col val} $rval {
                if {$val ne [dict oldgrid.$row.$col?]} {
                    # grid has changed ... force reload
                    Debug.grid {[namespace tail [self]] repainting}
                    dict set r -repaint 1
                    return [list $r {}]	;# return the dict of changes by id
                }
            }
        }

        if {[dict size [dict ni $oldgrid [dict keys $grid]]]} {
            # a grid element has been deleted
            Debug.grid {Grid repainting: '[namespace tail [self]]'}
            dict set r -repaint 1
            return [list $r {}]	;# return the dict of changes by id
        }

        set changes {}
        dict for {row rval} $grid {
            dict for {col val} $rval {
                dict with val {
                    set type [uplevel 1 [list $widget type]]
                    set changed {}
                    switch -- $type {
                        accordion -
                        notebook -
                        frame {
                            # propagate change request to geometry managing widgets
                            #Debug.grid {Grid changing $type '$widget' at ($row,$col)}
                            set changed [lassign [uplevel 1 [list $widget changes $r]] r]
                            if {[dict exists $r -repaint]} {
                                Debug.grid {Grid '[namespace tail [self]]' subgrid repaint $type '$widget'}
                                return [list $r {}]	;# repaint
                            } else {
                                #Debug.grid {Grid '[namespace tail [self]]' subgrid [string totitle $type] '$widget' at ($row,$col) ($val) -> ($changed)}
                            }
                        }

                        default {
                            if {[uplevel 1 [list $widget changed?]]} {
                                Debug.grid {[namespace tail [self]] changing: ($row,$col) $widget [uplevel 1 [list $widget type]] reports it's changed}
                                uplevel 1 [list $widget reset]
                                set changed $widget
                                lappend changed [uplevel 1 [list $widget wid]]
                                lappend changed [uplevel 1 [list $widget update]]
                                lappend changed [uplevel 1 [list $widget type]]

                                Debug.grid {Grid '[namespace tail [self]]' accumulate changes to [string totitle $type] '$widget' at ($row,$col) ($val) -> ($changed)}
                            }
                        }
                    }

                    lappend changes {*}$changed
                    set r [uplevel 1 [list $widget js $r]]
                }
            }
        }

        return [list $r {*}$changes]	;# return the dict of changes by id
    }

    method changed? {} {return 1}

    # render - construct a grid's <table>
    method grid_render {args} {
        variable maxrows; variable maxcols; variable grid
        Debug.grid {grid render [dict keys grid] over $maxrows/$maxcols}

        set rows {}	;# accumulate rows
        for {set row 0} {$row < $maxrows} {incr row} {
            set cols {}	;# accumulate cols

            for {set col 0} {$col < $maxcols} {} {
                set columnspan 1
                if {[dict exists $grid $row $col]} {
                    set el [dict get $grid $row $col]
                    dict with el {
                        Debug.grid {'[namespace tail [self]]' grid rendering $widget/$object}
                        for {set rt $row} {$rt < $rowspan} {incr rt} {
                            set rspan($widget,[expr {$row + $rt}].$col) 1
                            for {set ct $col} {$ct < $columnspan} {incr ct} {
                                set rspan($widget,$rt.[expr {$col + $ct}]) 1
                            }
                        }

                        if {$rowspan != 1} {
                            set rowspan [list rowspan $rowspan]
                        } else {
                            set rowspan {}
                        }

                        lappend cols [my <td> id [my id]_${row}_$col colspan $columnspan {*}$rowspan [$object render style $style sticky $sticky]]
                    }
                    incr col $columnspan
                } else {
                    # empty cell
                    if {[info exists widget]
                        && ![info exists rspan($widget,$row.$col)]
                    } {
                        lappend cols [my <td> id [my id]_${row}_$col "&nbsp;"]
                    }
                    incr col $columnspan
                }
            }

            # now we have a complete row - accumulate it
            # align and valign not allowed here
            lappend rows [my <tr> id [my id]_$row style width:100% [join $cols \n]]
        }

        variable oldgrid $grid	;# record the old grid
        set content [my <tbody> [join $rows \n]]
        dict set args width 100%

        set border [dict get? $props border]
        if {$border eq ""} {
            set b {}
        } elseif {[string is integer -strict $border]} {
            set b [list border ${border}px]
        } else {
            set b [list border $border]
        }

        set content [my <table> class grid {*}$b {*}[my style $args] $content]
        Debug.grid {Grid '[namespace tail [self]]' rendered ($content)}
        return $content
    }

    method grid_js {} {
        set result ""
        variable grid
        dict for {rc row} $grid {
            dict for {cc col} $row {
                append result [[dict get $col object] js]
            }
        }
        return $result
    }

    method grid_anchor {master {anchor ""}} {
        if {$master ne $widget} {
            return [$parent grid anchor $master {*}$args]
        }
    }

    method grid_bbox {master args} {
        if {$master ne $widget} {
            return [$parent grid bbox $master {*}$args]
        }
    }
    method grid_columnconfigure {master index args} {
        if {$master ne $widget} {
            return [$parent grid columnconfigure $master $index {*}$args]
        }
    }
    method grid_location {master x y} {
        if {$master ne $widget} {
            return [$parent grid location $master $x $y]
        }
    }
    method grid_propagate {master boolean} {
        if {$master ne $widget} {
            return [$parent grid propagate $master $boolean]
        }
    }
    method grid_rowconfigure {master index args} {
        if {$master ne $widget} {
            return [$parent grid rowconfigure $master $index {*}$args]
        }
    }

    method grid_size {master} {
        if {$master ne $widget} {
            return [$parent grid size $master]
        }
        variable maxcols; variable maxrows
        return [list $maxcols $maxrows]
    }

    method grid_slaves {master {option all} {value 0}} {
        if {$master ne $widget} {
            return [$parent grid slaves $master $option $value]
        }
        variable grid
        switch -- $option {
            -row {
                dict for {col v} [dict get $grid $value] {
                    lappend result [dict get $v widget]
                }
            }
            -column {
                dict for {row v} $grid {
                    if {[dict exists $v $value]} {
                        lappend result [dict get $v $value widget]
                    }
                }
            }
            default {
                dict for {row c} $grid {
                    dict for {col v} $c {
                        lappend result [dict get $v widget]
                    }
                }
            }
        }
        return $result
    }

    method grid_forget {slave} {
        if {$slave eq $widget} {
            return [$parent grid forget $widget {*}$args]
        }
    }

    method grid_info {slave} {
        if {$slave eq $widget} {
            return [$parent grid info $widget {*}$args]
        }
    }

    method grid_remove {slave} {
        if {$slave eq $widget} {
            return [$parent grid remove $widget {*}$args]
        }
        [$connection widget $slave] hide
    }

    method grid_configure {slave args} {
        if {$slave eq $widget} {
            return [$parent grid configure $widget {*}$args]
        }

        # set defaults
        set column 0
        set row 0
        set columnspan 1
        set rowspan 1
        set sticky ""
        set in ""
        set style ""

        foreach {var val} $args {
            set [string trim $var -] $val
        }

        variable maxcols
        set width [expr {$column + $columnspan}]
        if {$width > $maxcols} {
            set maxcols $width
        }

        variable maxrows
        set height [expr {$row + $rowspan}]
        if {$height > $maxrows} {
            set maxrows $height
        }

        set object [$connection widget $slave]

        variable grid
        dict set grid $row $column [list widget $slave columnspan $columnspan rowspan $rowspan sticky $sticky in $in style $style object $object]
        $object gridded columnspan $columnspan rowspan $rowspan sticky $sticky in $in style $style

        Debug.grid {[namespace tail [self]] configure gridding $slave/$object}
        return $slave
    }

    method grid {cmd args} {
        Debug.grid {[self] Calling grid_$cmd $args}
        tailcall my grid_$cmd {*}$args
    }

    constructor {args} {
        Debug.grid {[self] GRID constructing ($args)}
        variable maxcols 1
        variable maxrows 1
        variable border 0
        variable grid {}
        variable connection [dict get $args -connection]

        next {*}$args
        Debug.grid {[self] GRID constructed ($args)}
    }
}

# frame widget
class create ::NaTk::frame {
    superclass ::NaTk::Widget
    mixin ::NaTk::grid
    variable id props widget interp parent connection rendered

    # render widget
    method render {args} {
        next
        Debug.widgets {Frame [namespace tail [self]] render}
        if {[dict exists $props div]
            || [dict get? $props text] eq ""
        } {
            append content \n [my grid_render {*}$args]
            return [my <div> id $id class frame {*}[my style $args] $content]
        } else {
            set label [dict get? $props text]
            if {$label ne ""} {
                set content [my <legend> [tclarmour $label]]
            }
            append content \n [my grid_render {*}$args]
            return [my <fieldset> $id class frame {*}[my style $args] -raw 1 $content]
        }
    }

    destructor {
        # TODO destroy all child widgets
        next
    }

    constructor {args} {
        Debug.widgets {creating Frame [self]}
        next -border 0 {*}$args
        Debug.widgets {created Frame [self]}
    }
}

# toplevel widget
class create ::NaTk::toplevel {
    superclass ::NaTk::Widget
    mixin ::NaTk::grid
    variable id props widget interp parent connection rendered

    # render widget
    method render {args} {
        next
        Debug.widgets {[namespace tail [self]] toplevel render}

        my nacl toplevel.innerHTML=[::nacl jsquote [my <form> form_$id onsubmit "return false;" [my grid_render {*}$args]]]
        my nacl [::nacl jsquote [my grid_js]]
    }

    constructor {args} {
        next -titlebar 1 -menubar 1 -border 0 -toolbar 1 -location 1 -scrollbars 1 -status 1 -resizable 1 {*}$args
        set id toplevel
    }
}

Debug on form
Debug on widgets
Debug on widget
Debug on interp
Debug on grid

set ::nacl::verbose 1

# fetch all the <script> elements which are type text/tcl
# pass them as one big string to the NaTk object for evaluation.
::nacl js "runTclScripts('[::NaTk new] start')"
