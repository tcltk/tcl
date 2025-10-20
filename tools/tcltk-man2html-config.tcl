##
## Define the global stylesheet.
##
proc css-stylesheet {} {
    set hBd "1px dotted #11577B"

    css-style body div p th td li dd ul ol dl dt blockquote {
	font-family: Verdana, sans-serif;
    }
    css-style pre code {
	font-family: 'Courier New', Courier, monospace;
    }
    css-style pre {
	background-color:  #F6FCEC;
	border-top:        1px solid #6A6A6A;
	border-bottom:     1px solid #6A6A6A;
	padding:           1em;
	overflow:          auto;
    }
    css-style body {
	background-color:  #FFFFFF;
	font-size:         12px;
	line-height:       1.25;
	letter-spacing:    .2px;
	padding-left:      .5em;
    }
    css-style h1 h2 h3 h4 {
	font-family:       Georgia, serif;
	padding-left:      1em;
	margin-top:        1em;
    }
    css-style h1 {
	font-size:         18px;
	color:             #11577B;
	border-bottom:     $hBd;
	margin-top:        0px;
    }
    css-style h2 {
	font-size:         14px;
	color:             #11577B;
	background-color:  #C5DCE8;
	padding-left:      1em;
	border:            1px solid #6A6A6A;
    }
    css-style h3 h4 {
	color:             #1674A4;
	background-color:  #E8F2F6;
	border-bottom:     $hBd;
	border-top:        $hBd;
    }
    css-style h3 {
	font-size: 12px;
    }
    css-style h4 {
	font-size: 11px;
    }
    css-style ".keylist dt" ".arguments dt" {
	width: 25em;
	float: left;
	padding: 2px;
	border-top: 1px solid #999999;
    }
    css-style ".keylist dt" { font-weight: bold; }
    css-style ".keylist dd" ".arguments dd" {
	margin-left: 25em;
	padding: 2px;
	border-top: 1px solid #999999;
    }
    css-style .copy {
	background-color:  #F6FCFC;
	white-space:       pre;
	font-size:         80%;
	border-top:        1px solid #6A6A6A;
	margin-top:        2em;
    }
    css-style .tablecell {
	font-size:	   12px;
	padding-left:	   .5em;
	padding-right:	   .5em;
    }
}

##
## Set up some special cases. It would be nice if we didn't have them,
## but we do...
##

##
## Pages to not be generated.
##
set excluded_pages {}

##
## Pages to have index generation forced.
##
set forced_index_pages {GetDash}

##
## Pages to be generated with priority.
##
set process_first_patterns {*/ttk_widget.n */options.n}

##
## Default mapping from package directory names to package names.
##
set default_package_dir_name_map {
    itcl {[incr Tcl]}
    tdbc {TDBC}
    thread Thread
}

##
## Pages containing ensemble-form commands. Used for handling links to
## their subcommands.
##
set ensemble_commands {
    after
    array
    binary
    chan
    clock
    dde
    dict
    encoding
    file
    history
    info
    interp
    memory
    namespace
    package
    registry
    self
    string
    trace
    update
    zlib

    clipboard
    console
    font
    grab
    grid
    image
    option
    pack
    place
    selection
    tk
    tkwait
    ttk::style
    winfo
    wm

    itcl::delete
    itcl::find
    itcl::is
}

##
## Where some links should target when automatic guessing doesn't work.
##
array set remap_link_target {
    stdin  Tcl_GetStdChannel
    stdout Tcl_GetStdChannel
    stderr Tcl_GetStdChannel
    style  ttk::style
    "style map" ttk::style
    "tk busy"   busy
    library     auto_execok
    safe-tcl    safe
    tclvars     env
    tcl_break   catch
    tcl_continue catch
    tcl_error   catch
    tcl_ok      catch
    tcl_return  catch
    int()       mathfunc
    wide()      mathfunc
    packagens   pkg::create
    pkgMkIndex  pkg_mkIndex
    pkg_mkIndex pkg_mkIndex
    Tcl_Obj     Tcl_NewObj
    Tcl_ObjType Tcl_RegisterObjType
    Tcl_OpenFileChannelProc Tcl_FSOpenFileChannel
    errorinfo	env
    errorcode	env
    tcl_pkgpath env
    Tcl_Command Tcl_CreateObjCommand
    Tcl_CmdProc Tcl_CreateObjCommand
    Tcl_CmdDeleteProc Tcl_CreateObjCommand
    Tcl_ObjCmdProc Tcl_CreateObjCommand
    Tcl_Channel Tcl_OpenFileChannel
    Tcl_WideInt Tcl_NewIntObj
    Tcl_ChannelType Tcl_CreateChannel
    Tcl_DString Tcl_DStringInit
    Tcl_Namespace Tcl_AppendExportList
    Tcl_Object  Tcl_NewObjectInstance
    Tcl_Class   Tcl_GetObjectAsClass
    Tcl_Event   Tcl_QueueEvent
    Tcl_Time	Tcl_GetTime
    Tcl_ThreadId Tcl_CreateThread
    Tk_Window	Tk_WindowId
    Tk_3DBorder Tk_Get3DBorder
    Tk_Anchor	Tk_GetAnchor
    Tk_Cursor	Tk_GetCursor
    Tk_Dash	Tk_GetDash
    Tk_Font	Tk_GetFont
    Tk_Image	Tk_GetImage
    Tk_ImageMaster Tk_GetImage
    Tk_ImageModel Tk_GetImage
    Tk_ItemType Tk_CreateItemType
    Tk_Justify	Tk_GetJustify
    Ttk_Theme	Ttk_GetTheme
}

##
## Bold terms that shouldn't be links in specific pages.
##

array set exclude_refs_map {
    clock.n	next
    history.n	exec
    interp.n	time
    next.n	unknown
    safe.n	{join split}
    tcltest.n	error
    tm.n	exec
    zlib.n	{binary close filename text}
    TclZlib.3	{binary flush filename text}

    bind.n		{button destroy option}
    canvas.n		{bitmap text}
    console.n		{eval}
    checkbutton.n	{image}
    clipboard.n		{string}
    entry.n		{string}
    event.n		{return}
    font.n		{menu}
    getOpenFile.n	{file open text}
    grab.n		{global}
    menu.n		{checkbutton radiobutton}
    messageBox.n	{error info}
    options.n		{bitmap image set}
    radiobutton.n	{image}
    scale.n		{label variable}
    scrollbar.n		{set}
    selection.n		{string}
    text.n		{bind image lower raise}
    tkvars.n		{tk}
    tkwait.n		{variable}
    ttk_checkbutton.n	{variable}
    ttk_combobox.n	{selection}
    ttk_entry.n		{focus variable}
    ttk_intro.n		{focus text}
    ttk_label.n		{font text}
    ttk_labelframe.n	{text}
    ttk_menubutton.n	{flush}
    ttk_notebook.n	{image text}
    ttk_progressbar.n	{variable}
    ttk_radiobutton.n	{variable}
    ttk_scale.n		{variable}
    ttk_scrollbar.n	{set}
    ttk_spinbox.n	{format}
    ttk_treeview.n	{text open focus selection}
    ttk_widget.n	{image text variable}
}

##
## Bold terms that shouldn't be links in specific pages in some cases.
##
array set exclude_when_followed_by_map {
    canvas.n {
	bind widget
	focus widget
	image are
	lower widget
	raise widget
    }
    selection.n {
	clipboard selection
	clipboard ;
    }
    ttk_image.n {
	image imageSpec
    }
    fontchooser.n {
	tk fontchooser
    }
}

return

# Local-Variables:
# mode: tcl
# End:
