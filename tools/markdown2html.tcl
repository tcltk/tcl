# this script takes the manual pages in markdown format,
# converts them into html and builds a website structure to viwe the content
# in a web browser

# currently, this script must be executed from the tools directory!

package require Tcl 9


# all pages with metadata determining how they will occur in the HTML outline:
# - file   = the file name of the markdown file without the .md extension
# - title  = the page title for the outline item linking to the page
# - group1 = the initial level of the hierarchical outline
# - group2 = the second level of the hierarchical ouline
# - TclOO  = yes|no - whether the page is part of TclOO object system
#
foreach {
	file             title                             group1  group2                       TclOO
} {
	Tcl              {Language syntax}                  Tcl {Tcl syntax}                    no
	tclsh            tclsh                              Tcl {Tcl applications}              no
	after            after                              Tcl {Tcl commands}                  no
	append           append                             Tcl {Tcl commands}                  no
	apply            apply                              Tcl {Tcl commands}                  no
	array            array                              Tcl {Tcl commands}                  no
	bgerror          bgerror                            Tcl {Tcl commands}                  no
	binary           binary                             Tcl {Tcl commands}                  no
	break            break                              Tcl {Tcl commands}                  no
	buildinfo        buildinfo                          Tcl {Tcl commands}                  no
	callback         callback                           Tcl {Tcl commands}                  yes
	catch            catch                              Tcl {Tcl commands}                  no
	cd               cd                                 Tcl {Tcl commands}                  no
	chan             chan                               Tcl {Tcl commands}                  no
	classvariable    classvariable                      Tcl {Tcl commands}                  yes
	clock            clock                              Tcl {Tcl commands}                  no
	close            close                              Tcl {Tcl commands}                  no
	concat           concat                             Tcl {Tcl commands}                  no
	configurable     configure                          Tcl {Tcl commands}                  yes
	const            const                              Tcl {Tcl commands}                  no
	continue         continue                           Tcl {Tcl commands}                  no
	cookiejar        cookiejar                          Tcl {Tcl commands}                  no
	coroutine        coroinject                         Tcl {Tcl commands}                  no
	coroutine        coroprobe                          Tcl {Tcl commands}                  no
	coroutine        coroutine                          Tcl {Tcl commands}                  no
	dde              dde                                Tcl {Tcl commands}                  no
	dict             dict                               Tcl {Tcl commands}                  no
	divmod           divmod                             Tcl {Tcl commands}                  no
	encoding         encoding                           Tcl {Tcl commands}                  no
	eof              eof                                Tcl {Tcl commands}                  no
	error            error                              Tcl {Tcl commands}                  no
	eval             eval                               Tcl {Tcl commands}                  no
	exec             exec                               Tcl {Tcl commands}                  no
	exit             exit                               Tcl {Tcl commands}                  no
	expr             expr                               Tcl {Tcl commands}                  no
	fblocked         fblocked                           Tcl {Tcl commands}                  no
	fconfigure       fconfigure                         Tcl {Tcl commands}                  no
	fcopy            fcopy                              Tcl {Tcl commands}                  no
	file             file                               Tcl {Tcl commands}                  no
	fileevent        fileevent                          Tcl {Tcl commands}                  no
	flush            flush                              Tcl {Tcl commands}                  no
	for              for                                Tcl {Tcl commands}                  no
	foreach          foreach                            Tcl {Tcl commands}                  no
	format           format                             Tcl {Tcl commands}                  no
	fpclassify       fpclassify                         Tcl {Tcl commands}                  no
	frexp            frexp                              Tcl {Tcl commands}                  no
	gets             gets                               Tcl {Tcl commands}                  no
	glob             glob                               Tcl {Tcl commands}                  no
	global           global                             Tcl {Tcl commands}                  no
	history          history                            Tcl {Tcl commands}                  no
	http             http                               Tcl {Tcl commands}                  no
	if               if                                 Tcl {Tcl commands}                  no
	incr             incr                               Tcl {Tcl commands}                  no
	info             info                               Tcl {Tcl commands}                  no
	interp           interp                             Tcl {Tcl commands}                  no
	join             join                               Tcl {Tcl commands}                  no
	lappend          lappend                            Tcl {Tcl commands}                  no
	lassign          lassign                            Tcl {Tcl commands}                  no
	ledit            ledit                              Tcl {Tcl commands}                  no
	lfilter          lfilter                            Tcl {Tcl commands}                  no
	lindex           lindex                             Tcl {Tcl commands}                  no
	link             link                               Tcl {Tcl commands}                  yes
	linsert          linsert                            Tcl {Tcl commands}                  no
	list             list                               Tcl {Tcl commands}                  no
	llength          llength                            Tcl {Tcl commands}                  no
	lmap             lmap                               Tcl {Tcl commands}                  no
	load             load                               Tcl {Tcl commands}                  no
	lpop             lpop                               Tcl {Tcl commands}                  no
	lrange           lrange                             Tcl {Tcl commands}                  no
	lremove          lremove                            Tcl {Tcl commands}                  no
	lrepeat          lrepeat                            Tcl {Tcl commands}                  no
	lreplace         lreplace                           Tcl {Tcl commands}                  no
	lreverse         lreverse                           Tcl {Tcl commands}                  no
	lsearch          lsearch                            Tcl {Tcl commands}                  no
	lseq             lseq                               Tcl {Tcl commands}                  no
	lset             lset                               Tcl {Tcl commands}                  no
	lsort            lsort                              Tcl {Tcl commands}                  no
	memory           memory                             Tcl {Tcl commands}                  no
	modf             modf                               Tcl {Tcl commands}                  no
	msgcat           msgcat                             Tcl {Tcl commands}                  no
	my               my                                 Tcl {Tcl commands}                  yes
	my               myclass                            Tcl {Tcl commands}                  yes
	callback         mymethod                           Tcl {Tcl commands}                  yes
	namespace        namespace                          Tcl {Tcl commands}                  no
	next             next                               Tcl {Tcl commands}                  yes
	next             nextto                             Tcl {Tcl commands}                  yes
	abstract         oo::abstract                       Tcl {Tcl commands}                  yes
	class            oo::class                          Tcl {Tcl commands}                  yes
	configurable     oo::configurable                   Tcl {Tcl commands}                  yes
	copy             oo::copy                           Tcl {Tcl commands}                  yes
	define           oo::define                         Tcl {Tcl commands}                  yes
	define           oo::objdefine                      Tcl {Tcl commands}                  yes
	object           oo::object                         Tcl {Tcl commands}                  yes
	singleton        oo::singleton                      Tcl {Tcl commands}                  yes
	define           oo::Slot                           Tcl {Tcl commands}                  yes
	open             open                               Tcl {Tcl commands}                  no
	package          package                            Tcl {Tcl commands}                  no
	pid              pid                                Tcl {Tcl commands}                  no
	packagens        pkg::create                        Tcl {Tcl commands}                  no
	pkgMkIndex       pkg_mkIndex                        Tcl {Tcl commands}                  no
	platform         platform                           Tcl {Tcl commands}                  no
	platform_shell   platform::shell                    Tcl {Tcl commands}                  no
	proc             proc                               Tcl {Tcl commands}                  no
	configurable     property                           Tcl {Tcl commands}                  yes
	puts             puts                               Tcl {Tcl commands}                  no
	pwd              pwd                                Tcl {Tcl commands}                  no
	read             read                               Tcl {Tcl commands}                  no
	refchan          refchan                            Tcl {Tcl commands}                  no
	regexp           regexp                             Tcl {Tcl commands}                  no
	registry         registry                           Tcl {Tcl commands}                  no
	regsub           regsub                             Tcl {Tcl commands}                  no
	remquo           remquo                             Tcl {Tcl commands}                  no
	rename           rename                             Tcl {Tcl commands}                  no
	return           return                             Tcl {Tcl commands}                  no
	safe             safe                               Tcl {Tcl commands}                  no
	scan             scan                               Tcl {Tcl commands}                  no
	seek             seek                               Tcl {Tcl commands}                  no
	self             self                               Tcl {Tcl commands}                  yes
	set              set                                Tcl {Tcl commands}                  no
	socket           socket                             Tcl {Tcl commands}                  no
	source           source                             Tcl {Tcl commands}                  no
	split            split                              Tcl {Tcl commands}                  no
	string           string                             Tcl {Tcl commands}                  no
	subst            subst                              Tcl {Tcl commands}                  no
	switch           switch                             Tcl {Tcl commands}                  no
	tailcall         tailcall                           Tcl {Tcl commands}                  no
	idna             tcl::idna                          Tcl {Tcl commands}                  no
	prefix           tcl::prefix                        Tcl {Tcl commands}                  no
	process          tcl::process                       Tcl {Tcl commands}                  no
	tcltest          tcltest                            Tcl {Tcl commands}                  no
	tell             tell                               Tcl {Tcl commands}                  no
	throw            throw                              Tcl {Tcl commands}                  no
	time             time                               Tcl {Tcl commands}                  no
	timer            timer                              Tcl {Tcl commands}                  no
	timerate         timerate                           Tcl {Tcl commands}                  no
	tm               tm                                 Tcl {Tcl commands}                  no
	trace            trace                              Tcl {Tcl commands}                  no
	transchan        transchan                          Tcl {Tcl commands}                  no
	try              try                                Tcl {Tcl commands}                  no
	unicode          unicode                            Tcl {Tcl commands}                  no
	unknown          unknown                            Tcl {Tcl commands}                  no
	unload           unload                             Tcl {Tcl commands}                  no
	unset            unset                              Tcl {Tcl commands}                  no
	update           update                             Tcl {Tcl commands}                  no
	uplevel          uplevel                            Tcl {Tcl commands}                  no
	upvar            upvar                              Tcl {Tcl commands}                  no
	variable         variable                           Tcl {Tcl commands}                  no
	vwait            vwait                              Tcl {Tcl commands}                  no
	while            while                              Tcl {Tcl commands}                  no
	coroutine        yield                              Tcl {Tcl commands}                  no
	coroutine        yieldto                            Tcl {Tcl commands}                  no
	zipfs            zipfs                              Tcl {Tcl commands}                  no
	zlib             zlib                               Tcl {Tcl commands}                  no
	tclvars          argc                               Tcl {Tcl variables}                 no
	tclvars          argv                               Tcl {Tcl variables}                 no
	tclvars          argv0                              Tcl {Tcl variables}                 no
	tclvars          auto_path                          Tcl {Tcl variables}                 no
	tclvars          env                                Tcl {Tcl variables}                 no
	tclvars          errorCode                          Tcl {Tcl variables}                 no
	tclvars          errorInfo                          Tcl {Tcl variables}                 no
	tclvars          tcl_interactive                    Tcl {Tcl variables}                 no
	tclvars          tcl_library                        Tcl {Tcl variables}                 no
	tclvars          tcl_patchLevel                     Tcl {Tcl variables}                 no
	tclvars          tcl_pkgPath                        Tcl {Tcl variables}                 no
	tclvars          tcl_platform                       Tcl {Tcl variables}                 no
	tclvars          tcl_rcFileName                     Tcl {Tcl variables}                 no
	tclvars          tcl_traceCompile                   Tcl {Tcl variables}                 no
	tclvars          tcl_traceExec                      Tcl {Tcl variables}                 no
	tclvars          tcl_version                        Tcl {Tcl variables}                 no
	mathfunc         {Tcl math functions}               Tcl {Tcl math functions}            no
	mathop           {Tcl math operators}               Tcl {Tcl math operators}            no
	filename         {Tcl filename conventions}         Tcl {Tcl filename conventions}      no
	re_syntax        {Tcl regular expression syntax}    Tcl {Tcl regular expression syntax} no
	library          {Library procedures}               Tcl {Tcl library procedures}        no
	Alloc            attemptckalloc                     Tcl {Tcl C API}                     no
	Alloc            attemptckrealloc                   Tcl {Tcl C API}                     no
	Alloc            ckalloc                            Tcl {Tcl C API}                     no
	Alloc            ckfree                             Tcl {Tcl C API}                     no
	Alloc            ckrealloc                          Tcl {Tcl C API}                     no
	Access           Tcl_Access                         Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_AddErrorInfo                   Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_AddObjErrorInfo                Tcl {Tcl C API}                     no
	Notifier         Tcl_AlertNotifier                  Tcl {Tcl C API}                     no
	Alloc            Tcl_Alloc                          Tcl {Tcl C API}                     no
	FileSystem       Tcl_AllocStatBuf                   Tcl {Tcl C API}                     no
	AllowExc         Tcl_AllowExceptions                Tcl {Tcl C API}                     no
	ObjectType       Tcl_AppendAllObjTypes              Tcl {Tcl C API}                     no
	SetResult        Tcl_AppendElement                  Tcl {Tcl C API}                     no
	Namespace3       Tcl_AppendExportList               Tcl {Tcl C API}                     no
	StringObj        Tcl_AppendFormatToObj              Tcl {Tcl C API}                     no
	StringObj        Tcl_AppendLimitedToObj             Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_AppendObjToErrorInfo           Tcl {Tcl C API}                     no
	StringObj        Tcl_AppendObjToObj                 Tcl {Tcl C API}                     no
	StringObj        Tcl_AppendPrintfToObj              Tcl {Tcl C API}                     no
	SetResult        Tcl_AppendResult                   Tcl {Tcl C API}                     no
	StringObj        Tcl_AppendStringsToObj             Tcl {Tcl C API}                     no
	StringObj        Tcl_AppendToObj                    Tcl {Tcl C API}                     no
	StringObj        Tcl_AppendUnicodeToObj             Tcl {Tcl C API}                     no
	AppInit          Tcl_AppInit                        Tcl {Tcl C API}                     no
	Async            Tcl_AsyncCreate                    Tcl {Tcl C API}                     no
	Async            Tcl_AsyncDelete                    Tcl {Tcl C API}                     no
	Async            Tcl_AsyncInvoke                    Tcl {Tcl C API}                     no
	Async            Tcl_AsyncMark                      Tcl {Tcl C API}                     no
	Async            Tcl_AsyncMarkFromSignal            Tcl {Tcl C API}                     no
	Async            Tcl_AsyncReady                     Tcl {Tcl C API}                     no
	Alloc            Tcl_AttemptAlloc                   Tcl {Tcl C API}                     no
	Hash             Tcl_AttemptCreateHashEntry         Tcl {Tcl C API}                     no
	Alloc            Tcl_AttemptRealloc                 Tcl {Tcl C API}                     no
	StringObj        Tcl_AttemptSetObjLength            Tcl {Tcl C API}                     no
	BackgdErr        Tcl_BackgroundError                Tcl {Tcl C API}                     no
	BackgdErr        Tcl_BackgroundException            Tcl {Tcl C API}                     no
	CrtChannel       Tcl_BadChannelOption               Tcl {Tcl C API}                     no
	Object3          Tcl_BounceRefCount                 Tcl {Tcl C API}                     no
	CallDel          Tcl_CallWhenDeleted                Tcl {Tcl C API}                     no
	Cancel           Tcl_Canceled                       Tcl {Tcl C API}                     no
	Cancel           Tcl_CancelEval                     Tcl {Tcl C API}                     no
	DoWhenIdle       Tcl_CancelIdleCall                 Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelBlockModeProc           Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelBuffered                Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelClose2Proc              Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelFlushProc               Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelGetHandleProc           Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelGetOptionProc           Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelHandlerProc             Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelInputProc               Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelName                    Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelOutputProc              Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelSetOptionProc           Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelThreadActionProc        Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelTruncateProc            Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelVersion                 Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelWatchProc               Tcl {Tcl C API}                     no
	CrtChannel       Tcl_ChannelWideSeekProc            Tcl {Tcl C API}                     no
	Utf              Tcl_Char16Len                      Tcl {Tcl C API}                     no
	Utf              Tcl_Char16ToUtfDString             Tcl {Tcl C API}                     no
	GetCwd           Tcl_Chdir                          Tcl {Tcl C API}                     no
	Class3           Tcl_ClassGetMetadata               Tcl {Tcl C API}                     yes
	Method           Tcl_ClassSetConstructor            Tcl {Tcl C API}                     yes
	Method           Tcl_ClassSetDestructor             Tcl {Tcl C API}                     yes
	Class3           Tcl_ClassSetMetadata               Tcl {Tcl C API}                     yes
	CrtChannel       Tcl_ClearChannelHandlers           Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Close                          Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_CloseEx                        Tcl {Tcl C API}                     no
	CmdCmplt         Tcl_CommandComplete                Tcl {Tcl C API}                     no
	TraceCmd         Tcl_CommandTraceInfo               Tcl {Tcl C API}                     no
	Concat3          Tcl_Concat                         Tcl {Tcl C API}                     no
	StringObj        Tcl_ConcatObj                      Tcl {Tcl C API}                     no
	Thread           Tcl_ConditionFinalize              Tcl {Tcl C API}                     no
	Thread           Tcl_ConditionNotify                Tcl {Tcl C API}                     no
	Thread           Tcl_ConditionWait                  Tcl {Tcl C API}                     no
	Panic            Tcl_ConsolePanic                   Tcl {Tcl C API}                     no
	SplitList        Tcl_ConvertCountedElement          Tcl {Tcl C API}                     no
	SplitList        Tcl_ConvertElement                 Tcl {Tcl C API}                     no
	ObjectType       Tcl_ConvertToType                  Tcl {Tcl C API}                     no
	Class3           Tcl_CopyObjectInstance             Tcl {Tcl C API}                     yes
	CrtAlias         Tcl_CreateAlias                    Tcl {Tcl C API}                     no
	CrtAlias         Tcl_CreateAliasObj                 Tcl {Tcl C API}                     no
	CrtChannel       Tcl_CreateChannel                  Tcl {Tcl C API}                     no
	CrtChnlHdlr      Tcl_CreateChannelHandler           Tcl {Tcl C API}                     no
	CrtAlias         Tcl_CreateChild                    Tcl {Tcl C API}                     no
	CrtCloseHdlr     Tcl_CreateCloseHandler             Tcl {Tcl C API}                     no
	CrtCommand       Tcl_CreateCommand                  Tcl {Tcl C API}                     no
	Encoding3        Tcl_CreateEncoding                 Tcl {Tcl C API}                     no
	Ensemble         Tcl_CreateEnsemble                 Tcl {Tcl C API}                     no
	Notifier         Tcl_CreateEventSource              Tcl {Tcl C API}                     no
	Exit3            Tcl_CreateExitHandler              Tcl {Tcl C API}                     no
	CrtFileHdlr      Tcl_CreateFileHandler              Tcl {Tcl C API}                     no
	Hash             Tcl_CreateHashEntry                Tcl {Tcl C API}                     no
	CrtInterp        Tcl_CreateInterp                   Tcl {Tcl C API}                     no
	Namespace3       Tcl_CreateNamespace                Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_CreateObjCommand               Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_CreateObjCommand2              Tcl {Tcl C API}                     no
	CrtTrace         Tcl_CreateObjTrace                 Tcl {Tcl C API}                     no
	CrtTrace         Tcl_CreateObjTrace2                Tcl {Tcl C API}                     no
	Thread           Tcl_CreateThread                   Tcl {Tcl C API}                     no
	Exit3            Tcl_CreateThreadExitHandler        Tcl {Tcl C API}                     no
	CrtTimerHdlr     Tcl_CreateTimerHandler             Tcl {Tcl C API}                     no
	CrtTrace         Tcl_CreateTrace                    Tcl {Tcl C API}                     no
	CrtChannel       Tcl_CutChannel                     Tcl {Tcl C API}                     no
	Object3          Tcl_DecrRefCount                   Tcl {Tcl C API}                     no
	AssocData        Tcl_DeleteAssocData                Tcl {Tcl C API}                     no
	CrtChnlHdlr      Tcl_DeleteChannelHandler           Tcl {Tcl C API}                     no
	CrtCloseHdlr     Tcl_DeleteCloseHandler             Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_DeleteCommand                  Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_DeleteCommandFromToken         Tcl {Tcl C API}                     no
	Notifier         Tcl_DeleteEvents                   Tcl {Tcl C API}                     no
	Notifier         Tcl_DeleteEventSource              Tcl {Tcl C API}                     no
	Exit3            Tcl_DeleteExitHandler              Tcl {Tcl C API}                     no
	CrtFileHdlr      Tcl_DeleteFileHandler              Tcl {Tcl C API}                     no
	Hash             Tcl_DeleteHashEntry                Tcl {Tcl C API}                     no
	Hash             Tcl_DeleteHashTable                Tcl {Tcl C API}                     no
	CrtInterp        Tcl_DeleteInterp                   Tcl {Tcl C API}                     no
	Namespace3       Tcl_DeleteNamespace                Tcl {Tcl C API}                     no
	Exit3            Tcl_DeleteThreadExitHandler        Tcl {Tcl C API}                     no
	CrtTimerHdlr     Tcl_DeleteTimerHandler             Tcl {Tcl C API}                     no
	CrtTrace         Tcl_DeleteTrace                    Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_DetachChannel                  Tcl {Tcl C API}                     no
	DetachPids       Tcl_DetachPids                     Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjDone                    Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjFirst                   Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjGet                     Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjNext                    Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjPut                     Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjPutKeyList              Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjRemove                  Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjRemoveKeyList           Tcl {Tcl C API}                     no
	DictObj          Tcl_DictObjSize                    Tcl {Tcl C API}                     no
	SaveInterpState  Tcl_DiscardInterpState             Tcl {Tcl C API}                     no
	CallDel          Tcl_DontCallWhenDeleted            Tcl {Tcl C API}                     no
	DoOneEvent       Tcl_DoOneEvent                     Tcl {Tcl C API}                     no
	DoWhenIdle       Tcl_DoWhenIdle                     Tcl {Tcl C API}                     no
	DString          Tcl_DStringAppend                  Tcl {Tcl C API}                     no
	DString          Tcl_DStringAppendElement           Tcl {Tcl C API}                     no
	DString          Tcl_DStringEndSublist              Tcl {Tcl C API}                     no
	DString          Tcl_DStringFree                    Tcl {Tcl C API}                     no
	DString          Tcl_DStringGetResult               Tcl {Tcl C API}                     no
	DString          Tcl_DStringInit                    Tcl {Tcl C API}                     no
	DString          Tcl_DStringLength                  Tcl {Tcl C API}                     no
	DString          Tcl_DStringResult                  Tcl {Tcl C API}                     no
	DString          Tcl_DStringSetLength               Tcl {Tcl C API}                     no
	DString          Tcl_DStringStartSublist            Tcl {Tcl C API}                     no
	DString          Tcl_DStringToObj                   Tcl {Tcl C API}                     no
	DString          Tcl_DStringValue                   Tcl {Tcl C API}                     no
	DumpActiveMemory Tcl_DumpActiveMemory               Tcl {Tcl C API}                     no
	Object3          Tcl_DuplicateObj                   Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Eof                            Tcl {Tcl C API}                     no
	SetErrno         Tcl_ErrnoId                        Tcl {Tcl C API}                     no
	SetErrno         Tcl_ErrnoMsg                       Tcl {Tcl C API}                     no
	Eval3            Tcl_Eval                           Tcl {Tcl C API}                     no
	Eval3            Tcl_EvalEx                         Tcl {Tcl C API}                     no
	Eval3            Tcl_EvalFile                       Tcl {Tcl C API}                     no
	Eval3            Tcl_EvalObjEx                      Tcl {Tcl C API}                     no
	Eval3            Tcl_EvalObjv                       Tcl {Tcl C API}                     no
	ParseCmd         Tcl_EvalTokensStandard             Tcl {Tcl C API}                     no
	Preserve         Tcl_EventuallyFree                 Tcl {Tcl C API}                     no
	Exit3            Tcl_Exit                           Tcl {Tcl C API}                     no
	Exit3            Tcl_ExitThread                     Tcl {Tcl C API}                     no
	Namespace3       Tcl_Export                         Tcl {Tcl C API}                     no
	CrtAlias         Tcl_ExposeCommand                  Tcl {Tcl C API}                     no
	ExprLong         Tcl_ExprBoolean                    Tcl {Tcl C API}                     no
	ExprLongObj      Tcl_ExprBooleanObj                 Tcl {Tcl C API}                     no
	ExprLong         Tcl_ExprDouble                     Tcl {Tcl C API}                     no
	ExprLongObj      Tcl_ExprDoubleObj                  Tcl {Tcl C API}                     no
	ExprLong         Tcl_ExprLong                       Tcl {Tcl C API}                     no
	ExprLongObj      Tcl_ExprLongObj                    Tcl {Tcl C API}                     no
	ExprLongObj      Tcl_ExprObj                        Tcl {Tcl C API}                     no
	ExprLong         Tcl_ExprString                     Tcl {Tcl C API}                     no
	Encoding3        Tcl_ExternalToUtf                  Tcl {Tcl C API}                     no
	Encoding3        Tcl_ExternalToUtfDString           Tcl {Tcl C API}                     no
	Encoding3        Tcl_ExternalToUtfDStringEx         Tcl {Tcl C API}                     no
	Encoding3        Tcl_ExternalToUtfEx                Tcl {Tcl C API}                     no
	ObjectType       Tcl_FetchInternalRep               Tcl {Tcl C API}                     no
	Exit3            Tcl_Finalize                       Tcl {Tcl C API}                     no
	Notifier         Tcl_FinalizeNotifier               Tcl {Tcl C API}                     no
	Exit3            Tcl_FinalizeThread                 Tcl {Tcl C API}                     no
	Namespace3       Tcl_FindCommand                    Tcl {Tcl C API}                     no
	Ensemble         Tcl_FindEnsemble                   Tcl {Tcl C API}                     no
	FindExec         Tcl_FindExecutable                 Tcl {Tcl C API}                     no
	Hash             Tcl_FindHashEntry                  Tcl {Tcl C API}                     no
	Namespace3       Tcl_FindNamespace                  Tcl {Tcl C API}                     no
	Load3            Tcl_FindSymbol                     Tcl {Tcl C API}                     no
	Hash             Tcl_FirstHashEntry                 Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Flush                          Tcl {Tcl C API}                     no
	Namespace3       Tcl_ForgetImport                   Tcl {Tcl C API}                     no
	StringObj        Tcl_Format                         Tcl {Tcl C API}                     no
	Alloc            Tcl_Free                           Tcl {Tcl C API}                     no
	Encoding3        Tcl_FreeEncoding                   Tcl {Tcl C API}                     no
	ObjectType       Tcl_FreeInternalRep                Tcl {Tcl C API}                     no
	ParseCmd         Tcl_FreeParse                      Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSAccess                       Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSChdir                        Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSConvertToPathType            Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSCopyDirectory                Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSCopyFile                     Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSCreateDirectory              Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSData                         Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSDeleteFile                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSEqualPaths                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSEvalFile                     Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSEvalFileEx                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSFileAttrsGet                 Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSFileAttrsSet                 Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSFileAttrStrings              Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSFileSystemInfo               Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetCwd                       Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetFileSystemForPath         Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetInternalRep               Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetNativePath                Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetNormalizedPath            Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetPathType                  Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetTranslatedPath            Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSGetTranslatedStringPath      Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSJoinPath                     Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSJoinToPath                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSLink                         Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSListVolumes                  Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSLoadFile                     Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSLstat                        Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSMatchInDirectory             Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSMountsChanged                Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSNewNativePath                Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSOpenFileChannel              Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSPathSeparator                Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSRegister                     Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSRemoveDirectory              Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSRenameFile                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSSplitPath                    Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSStat                         Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSTildeExpand                  Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSUnloadFile                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSUnregister                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_FSUtime                        Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetAccessTimeFromStat          Tcl {Tcl C API}                     no
	CrtAlias         Tcl_GetAliasObj                    Tcl {Tcl C API}                     no
	AssocData        Tcl_GetAssocData                   Tcl {Tcl C API}                     no
	IntObj           Tcl_GetBignumFromObj               Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetBlocksFromStat              Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetBlockSizeFromStat           Tcl {Tcl C API}                     no
	GetInt           Tcl_GetBoolean                     Tcl {Tcl C API}                     no
	BoolObj          Tcl_GetBooleanFromObj              Tcl {Tcl C API}                     no
	BoolObj          Tcl_GetBoolFromObj                 Tcl {Tcl C API}                     no
	ByteArrObj       Tcl_GetByteArrayFromObj            Tcl {Tcl C API}                     no
	ByteArrObj       Tcl_GetBytesFromObj                Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetChangeTimeFromStat          Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_GetChannel                     Tcl {Tcl C API}                     no
	CrtChannel       Tcl_GetChannelBufferSize           Tcl {Tcl C API}                     no
	SetChanErr       Tcl_GetChannelError                Tcl {Tcl C API}                     no
	SetChanErr       Tcl_GetChannelErrorInterp          Tcl {Tcl C API}                     no
	CrtChannel       Tcl_GetChannelHandle               Tcl {Tcl C API}                     no
	CrtChannel       Tcl_GetChannelInstanceData         Tcl {Tcl C API}                     no
	CrtChannel       Tcl_GetChannelMode                 Tcl {Tcl C API}                     no
	CrtChannel       Tcl_GetChannelName                 Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_GetChannelNames                Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_GetChannelNamesEx              Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_GetChannelOption               Tcl {Tcl C API}                     no
	CrtChannel       Tcl_GetChannelThread               Tcl {Tcl C API}                     no
	CrtChannel       Tcl_GetChannelType                 Tcl {Tcl C API}                     no
	StringObj        Tcl_GetCharLength                  Tcl {Tcl C API}                     no
	CrtAlias         Tcl_GetChild                       Tcl {Tcl C API}                     no
	Class3           Tcl_GetClassAsObject               Tcl {Tcl C API}                     yes
	CrtObjCmd        Tcl_GetCommandFromObj              Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_GetCommandFullName             Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_GetCommandInfo                 Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_GetCommandInfoFromToken        Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_GetCommandName                 Tcl {Tcl C API}                     no
	Namespace3       Tcl_GetCurrentNamespace            Tcl {Tcl C API}                     no
	Notifier         Tcl_GetCurrentThread               Tcl {Tcl C API}                     no
	GetCwd           Tcl_GetCwd                         Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetDeviceTypeFromStat          Tcl {Tcl C API}                     no
	GetInt           Tcl_GetDouble                      Tcl {Tcl C API}                     no
	DoubleObj        Tcl_GetDoubleFromObj               Tcl {Tcl C API}                     no
	Encoding3        Tcl_GetEncoding                    Tcl {Tcl C API}                     no
	Encoding3        Tcl_GetEncodingFromObj             Tcl {Tcl C API}                     no
	Encoding3        Tcl_GetEncodingName                Tcl {Tcl C API}                     no
	Encoding3        Tcl_GetEncodingNameForUser         Tcl {Tcl C API}                     no
	Encoding3        Tcl_GetEncodingNameFromEnvironment Tcl {Tcl C API}                     no
	Encoding3        Tcl_GetEncodingNames               Tcl {Tcl C API}                     no
	Encoding3        Tcl_GetEncodingSearchPath          Tcl {Tcl C API}                     no
	Ensemble         Tcl_GetEnsembleFlags               Tcl {Tcl C API}                     no
	Ensemble         Tcl_GetEnsembleMappingDict         Tcl {Tcl C API}                     no
	Ensemble         Tcl_GetEnsembleNamespace           Tcl {Tcl C API}                     no
	Ensemble         Tcl_GetEnsembleParameterList       Tcl {Tcl C API}                     no
	Ensemble         Tcl_GetEnsembleSubcommandList      Tcl {Tcl C API}                     no
	Ensemble         Tcl_GetEnsembleUnknownHandler      Tcl {Tcl C API}                     no
	SetErrno         Tcl_GetErrno                       Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_GetErrorLine                   Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetFSDeviceFromStat            Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetFSInodeFromStat             Tcl {Tcl C API}                     no
	Namespace3       Tcl_GetGlobalNamespace             Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetGroupIdFromStat             Tcl {Tcl C API}                     no
	Hash             Tcl_GetHashKey                     Tcl {Tcl C API}                     no
	Hash             Tcl_GetHashValue                   Tcl {Tcl C API}                     no
	GetHostName      Tcl_GetHostName                    Tcl {Tcl C API}                     no
	GetIndex         Tcl_GetIndexFromObj                Tcl {Tcl C API}                     no
	GetIndex         Tcl_GetIndexFromObjStruct          Tcl {Tcl C API}                     no
	GetInt           Tcl_GetInt                         Tcl {Tcl C API}                     no
	CrtAlias         Tcl_GetInterpPath                  Tcl {Tcl C API}                     no
	IntObj           Tcl_GetIntForIndex                 Tcl {Tcl C API}                     no
	IntObj           Tcl_GetIntFromObj                  Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetLinkCountFromStat           Tcl {Tcl C API}                     no
	IntObj           Tcl_GetLongFromObj                 Tcl {Tcl C API}                     no
	Alloc            Tcl_GetMemoryInfo                  Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetModeFromStat                Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetModificationTimeFromStat    Tcl {Tcl C API}                     no
	GetTime          Tcl_GetMonotonicTime               Tcl {Tcl C API}                     no
	FindExec         Tcl_GetNameOfExecutable            Tcl {Tcl C API}                     no
	Namespace3       Tcl_GetNamespaceUnknownHandler     Tcl {Tcl C API}                     no
	Number           Tcl_GetNumber                      Tcl {Tcl C API}                     no
	Number           Tcl_GetNumberFromObj               Tcl {Tcl C API}                     no
	Class3           Tcl_GetObjectAsClass               Tcl {Tcl C API}                     yes
	Class3           Tcl_GetObjectCommand               Tcl {Tcl C API}                     yes
	Class3           Tcl_GetObjectFromObj               Tcl {Tcl C API}                     yes
	Class3           Tcl_GetObjectName                  Tcl {Tcl C API}                     yes
	Class3           Tcl_GetObjectNamespace             Tcl {Tcl C API}                     yes
	SetResult        Tcl_GetObjResult                   Tcl {Tcl C API}                     no
	ObjectType       Tcl_GetObjType                     Tcl {Tcl C API}                     no
	GetOpnFl         Tcl_GetOpenFile                    Tcl {Tcl C API}                     no
	CrtAlias         Tcl_GetParent                      Tcl {Tcl C API}                     no
	SplitPath        Tcl_GetPathType                    Tcl {Tcl C API}                     no
	StringObj        Tcl_GetRange                       Tcl {Tcl C API}                     no
	RegExp3          Tcl_GetRegExpFromObj               Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_GetReturnOptions               Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Gets                           Tcl {Tcl C API}                     no
	Notifier         Tcl_GetServiceMode                 Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetSizeFromStat                Tcl {Tcl C API}                     no
	IntObj           Tcl_GetSizeIntFromObj              Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_GetsObj                        Tcl {Tcl C API}                     no
	ChnlStack        Tcl_GetStackedChannel              Tcl {Tcl C API}                     no
	Tcl_Main         Tcl_GetStartupScript               Tcl {Tcl C API}                     no
	GetStdChan       Tcl_GetStdChannel                  Tcl {Tcl C API}                     no
	StringObj        Tcl_GetString                      Tcl {Tcl C API}                     no
	StringObj        Tcl_GetStringFromObj               Tcl {Tcl C API}                     no
	SetResult        Tcl_GetStringResult                Tcl {Tcl C API}                     no
	Thread           Tcl_GetThreadData                  Tcl {Tcl C API}                     no
	GetTime          Tcl_GetTime                        Tcl {Tcl C API}                     no
	ChnlStack        Tcl_GetTopChannel                  Tcl {Tcl C API}                     no
	StringObj        Tcl_GetUniChar                     Tcl {Tcl C API}                     no
	StringObj        Tcl_GetUnicode                     Tcl {Tcl C API}                     no
	StringObj        Tcl_GetUnicodeFromObj              Tcl {Tcl C API}                     no
	FileSystem       Tcl_GetUserIdFromStat              Tcl {Tcl C API}                     no
	SetVar           Tcl_GetVar                         Tcl {Tcl C API}                     no
	SetVar           Tcl_GetVar2                        Tcl {Tcl C API}                     no
	SetVar           Tcl_GetVar2Ex                      Tcl {Tcl C API}                     no
	GetVersion       Tcl_GetVersion                     Tcl {Tcl C API}                     no
	IntObj           Tcl_GetWideIntFromObj              Tcl {Tcl C API}                     no
	IntObj           Tcl_GetWideUIntFromObj             Tcl {Tcl C API}                     no
	Eval3            Tcl_GlobalEval                     Tcl {Tcl C API}                     no
	Eval3            Tcl_GlobalEvalObj                  Tcl {Tcl C API}                     no
	Hash             Tcl_HashStats                      Tcl {Tcl C API}                     no
	ObjectType       Tcl_HasStringRep                   Tcl {Tcl C API}                     no
	CrtAlias         Tcl_HideCommand                    Tcl {Tcl C API}                     no
	Namespace3       Tcl_Import                         Tcl {Tcl C API}                     no
	Object3          Tcl_IncrRefCount                   Tcl {Tcl C API}                     no
	Init             Tcl_Init                           Tcl {Tcl C API}                     no
	Hash             Tcl_InitCustomHashTable            Tcl {Tcl C API}                     no
	Hash             Tcl_InitHashTable                  Tcl {Tcl C API}                     no
	DumpActiveMemory Tcl_InitMemory                     Tcl {Tcl C API}                     no
	Notifier         Tcl_InitNotifier                   Tcl {Tcl C API}                     no
	Hash             Tcl_InitObjHashTable               Tcl {Tcl C API}                     no
	ObjectType       Tcl_InitStringRep                  Tcl {Tcl C API}                     no
	InitStubs        Tcl_InitStubs                      Tcl {Tcl C API}                     no
	InitSubSyst      Tcl_InitSubsystems                 Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_InputBlocked                   Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_InputBuffered                  Tcl {Tcl C API}                     no
	CrtInterp        Tcl_InterpActive                   Tcl {Tcl C API}                     no
	CrtInterp        Tcl_InterpDeleted                  Tcl {Tcl C API}                     no
	Object3          Tcl_InvalidateStringRep            Tcl {Tcl C API}                     no
	CrtChannel       Tcl_IsChannelExisting              Tcl {Tcl C API}                     no
	CrtChannel       Tcl_IsChannelRegistered            Tcl {Tcl C API}                     no
	CrtChannel       Tcl_IsChannelShared                Tcl {Tcl C API}                     no
	StringObj        Tcl_IsEmpty                        Tcl {Tcl C API}                     no
	Ensemble         Tcl_IsEnsemble                     Tcl {Tcl C API}                     no
	CrtAlias         Tcl_IsSafe                         Tcl {Tcl C API}                     no
	Object3          Tcl_IsShared                       Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_IsStandardChannel              Tcl {Tcl C API}                     no
	SplitPath        Tcl_JoinPath                       Tcl {Tcl C API}                     no
	Thread           Tcl_JoinThread                     Tcl {Tcl C API}                     no
	Limit            Tcl_LimitAddHandler                Tcl {Tcl C API}                     no
	Limit            Tcl_LimitCheck                     Tcl {Tcl C API}                     no
	Limit            Tcl_LimitExceeded                  Tcl {Tcl C API}                     no
	Limit            Tcl_LimitGetCommands               Tcl {Tcl C API}                     no
	Limit            Tcl_LimitGetGranularity            Tcl {Tcl C API}                     no
	Limit            Tcl_LimitGetTime                   Tcl {Tcl C API}                     no
	Limit            Tcl_LimitReady                     Tcl {Tcl C API}                     no
	Limit            Tcl_LimitRemoveHandler             Tcl {Tcl C API}                     no
	Limit            Tcl_LimitSetCommands               Tcl {Tcl C API}                     no
	Limit            Tcl_LimitSetGranularity            Tcl {Tcl C API}                     no
	Limit            Tcl_LimitSetTime                   Tcl {Tcl C API}                     no
	Limit            Tcl_LimitTypeEnabled               Tcl {Tcl C API}                     no
	Limit            Tcl_LimitTypeExceeded              Tcl {Tcl C API}                     no
	Limit            Tcl_LimitTypeReset                 Tcl {Tcl C API}                     no
	Limit            Tcl_LimitTypeSet                   Tcl {Tcl C API}                     no
	LinkVar          Tcl_LinkArray                      Tcl {Tcl C API}                     no
	LinkVar          Tcl_LinkVar                        Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjAppendElement           Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjAppendList              Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjGetElements             Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjIndex                   Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjLength                  Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjRange                   Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjRepeat                  Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjReplace                 Tcl {Tcl C API}                     no
	ListObj          Tcl_ListObjReverse                 Tcl {Tcl C API}                     no
	Load3            Tcl_LoadFile                       Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_LogCommandInfo                 Tcl {Tcl C API}                     no
	Tcl_Main         Tcl_Main                           Tcl {Tcl C API}                     no
	Tcl_Main         Tcl_MainEx                         Tcl {Tcl C API}                     no
	Tcl_Main         Tcl_MainExW                        Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_MakeFileChannel                Tcl {Tcl C API}                     no
	OpenTcp          Tcl_MakeTcpClientChannel           Tcl {Tcl C API}                     no
	TCL_MEM_DEBUG    TCL_MEM_DEBUG                      Tcl {Tcl C API}                     no
	SplitList        Tcl_Merge                          Tcl {Tcl C API}                     no
	Method           Tcl_MethodDeclarerClass            Tcl {Tcl C API}                     yes
	Method           Tcl_MethodDeclarerObject           Tcl {Tcl C API}                     yes
	Method           Tcl_MethodIsPrivate                Tcl {Tcl C API}                     yes
	Method           Tcl_MethodIsPublic                 Tcl {Tcl C API}                     yes
	Method           Tcl_MethodIsType                   Tcl {Tcl C API}                     yes
	Method           Tcl_MethodIsType2                  Tcl {Tcl C API}                     yes
	Method           Tcl_MethodName                     Tcl {Tcl C API}                     yes
	Thread           Tcl_MutexFinalize                  Tcl {Tcl C API}                     no
	Thread           Tcl_MutexLock                      Tcl {Tcl C API}                     no
	Thread           Tcl_MutexUnlock                    Tcl {Tcl C API}                     no
	IntObj           Tcl_NewBignumObj                   Tcl {Tcl C API}                     no
	BoolObj          Tcl_NewBooleanObj                  Tcl {Tcl C API}                     no
	ByteArrObj       Tcl_NewByteArrayObj                Tcl {Tcl C API}                     no
	DictObj          Tcl_NewDictObj                     Tcl {Tcl C API}                     no
	DoubleObj        Tcl_NewDoubleObj                   Tcl {Tcl C API}                     no
	Method           Tcl_NewInstanceMethod              Tcl {Tcl C API}                     yes
	Method           Tcl_NewInstanceMethod2             Tcl {Tcl C API}                     yes
	IntObj           Tcl_NewIntObj                      Tcl {Tcl C API}                     no
	ListObj          Tcl_NewListObj                     Tcl {Tcl C API}                     no
	IntObj           Tcl_NewLongObj                     Tcl {Tcl C API}                     no
	Method           Tcl_NewMethod                      Tcl {Tcl C API}                     yes
	Method           Tcl_NewMethod2                     Tcl {Tcl C API}                     yes
	Object3          Tcl_NewObj                         Tcl {Tcl C API}                     no
	Class3           Tcl_NewObjectInstance              Tcl {Tcl C API}                     yes
	StringObj        Tcl_NewStringObj                   Tcl {Tcl C API}                     no
	StringObj        Tcl_NewUnicodeObj                  Tcl {Tcl C API}                     no
	IntObj           Tcl_NewWideIntObj                  Tcl {Tcl C API}                     no
	IntObj           Tcl_NewWideUIntObj                 Tcl {Tcl C API}                     no
	Hash             Tcl_NextHashEntry                  Tcl {Tcl C API}                     no
	CrtChannel       Tcl_NotifyChannel                  Tcl {Tcl C API}                     no
	NRE              Tcl_NRAddCallback                  Tcl {Tcl C API}                     no
	NRE              Tcl_NRCallObjProc                  Tcl {Tcl C API}                     no
	NRE              Tcl_NRCallObjProc2                 Tcl {Tcl C API}                     no
	NRE              Tcl_NRCmdSwap                      Tcl {Tcl C API}                     no
	NRE              Tcl_NRCreateCommand                Tcl {Tcl C API}                     no
	NRE              Tcl_NRCreateCommand2               Tcl {Tcl C API}                     no
	NRE              Tcl_NREvalObj                      Tcl {Tcl C API}                     no
	NRE              Tcl_NREvalObjv                     Tcl {Tcl C API}                     no
	NRE              Tcl_NRExprObj                      Tcl {Tcl C API}                     no
	Utf              Tcl_NumUtfChars                    Tcl {Tcl C API}                     no
	Method           Tcl_ObjectContextInvokeNext        Tcl {Tcl C API}                     yes
	Method           Tcl_ObjectContextIsFiltering       Tcl {Tcl C API}                     yes
	Method           Tcl_ObjectContextMethod            Tcl {Tcl C API}                     yes
	Method           Tcl_ObjectContextObject            Tcl {Tcl C API}                     yes
	Method           Tcl_ObjectContextSkippedArgs       Tcl {Tcl C API}                     yes
	Class3           Tcl_ObjectDeleted                  Tcl {Tcl C API}                     yes
	Class3           Tcl_ObjectGetMetadata              Tcl {Tcl C API}                     yes
	Class3           Tcl_ObjectGetMethodNameMapper      Tcl {Tcl C API}                     yes
	Class3           Tcl_ObjectSetMetadata              Tcl {Tcl C API}                     yes
	Class3           Tcl_ObjectSetMethodNameMapper      Tcl {Tcl C API}                     yes
	SetVar           Tcl_ObjGetVar2                     Tcl {Tcl C API}                     no
	StringObj        Tcl_ObjPrintf                      Tcl {Tcl C API}                     no
	SetVar           Tcl_ObjSetVar2                     Tcl {Tcl C API}                     no
	OOInitStubs      Tcl_OOInitStubs                    Tcl {Tcl C API}                     yes
	OpenFileChnl     Tcl_OpenCommandChannel             Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_OpenFileChannel                Tcl {Tcl C API}                     no
	OpenTcp          Tcl_OpenTcpClient                  Tcl {Tcl C API}                     no
	OpenTcp          Tcl_OpenTcpServer                  Tcl {Tcl C API}                     no
	OpenTcp          Tcl_OpenTcpServerEx                Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_OutputBuffered                 Tcl {Tcl C API}                     no
	Panic            Tcl_Panic                          Tcl {Tcl C API}                     no
	ParseArgs        Tcl_ParseArgsObjv                  Tcl {Tcl C API}                     no
	ParseCmd         Tcl_ParseBraces                    Tcl {Tcl C API}                     no
	ParseCmd         Tcl_ParseCommand                   Tcl {Tcl C API}                     no
	ParseCmd         Tcl_ParseExpr                      Tcl {Tcl C API}                     no
	ParseCmd         Tcl_ParseQuotedString              Tcl {Tcl C API}                     no
	ParseCmd         Tcl_ParseVar                       Tcl {Tcl C API}                     no
	ParseCmd         Tcl_ParseVarName                   Tcl {Tcl C API}                     no
	PkgRequire       Tcl_PkgPresent                     Tcl {Tcl C API}                     no
	PkgRequire       Tcl_PkgPresentEx                   Tcl {Tcl C API}                     no
	PkgRequire       Tcl_PkgProvide                     Tcl {Tcl C API}                     no
	PkgRequire       Tcl_PkgProvideEx                   Tcl {Tcl C API}                     no
	PkgRequire       Tcl_PkgRequire                     Tcl {Tcl C API}                     no
	PkgRequire       Tcl_PkgRequireEx                   Tcl {Tcl C API}                     no
	PkgRequire       Tcl_PkgRequireProc                 Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_PosixError                     Tcl {Tcl C API}                     no
	Preserve         Tcl_Preserve                       Tcl {Tcl C API}                     no
	PrintDbl         Tcl_PrintDouble                    Tcl {Tcl C API}                     no
	Environment      Tcl_PutEnv                         Tcl {Tcl C API}                     no
	GetTime          Tcl_QueryTimeProc                  Tcl {Tcl C API}                     no
	Notifier         Tcl_QueueEvent                     Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Read                           Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_ReadChars                      Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_ReadRaw                        Tcl {Tcl C API}                     no
	Alloc            Tcl_Realloc                        Tcl {Tcl C API}                     no
	DetachPids       Tcl_ReapDetachedProcs              Tcl {Tcl C API}                     no
	RecordEval       Tcl_RecordAndEval                  Tcl {Tcl C API}                     no
	RecEvalObj       Tcl_RecordAndEvalObj               Tcl {Tcl C API}                     no
	RegExp3          Tcl_RegExpCompile                  Tcl {Tcl C API}                     no
	RegExp3          Tcl_RegExpExec                     Tcl {Tcl C API}                     no
	RegExp3          Tcl_RegExpExecObj                  Tcl {Tcl C API}                     no
	RegExp3          Tcl_RegExpGetInfo                  Tcl {Tcl C API}                     no
	RegExp3          Tcl_RegExpMatch                    Tcl {Tcl C API}                     no
	RegExp3          Tcl_RegExpMatchObj                 Tcl {Tcl C API}                     no
	RegExp3          Tcl_RegExpRange                    Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_RegisterChannel                Tcl {Tcl C API}                     no
	RegConfig        Tcl_RegisterConfig                 Tcl {Tcl C API}                     no
	ObjectType       Tcl_RegisterObjType                Tcl {Tcl C API}                     no
	Preserve         Tcl_Release                        Tcl {Tcl C API}                     no
	SetResult        Tcl_ResetResult                    Tcl {Tcl C API}                     no
	SaveInterpState  Tcl_RestoreInterpState             Tcl {Tcl C API}                     no
	SaveInterpState  Tcl_SaveInterpState                Tcl {Tcl C API}                     no
	SplitList        Tcl_ScanCountedElement             Tcl {Tcl C API}                     no
	SplitList        Tcl_ScanElement                    Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Seek                           Tcl {Tcl C API}                     no
	Notifier         Tcl_ServiceAll                     Tcl {Tcl C API}                     no
	Notifier         Tcl_ServiceEvent                   Tcl {Tcl C API}                     no
	Notifier         Tcl_ServiceModeHook                Tcl {Tcl C API}                     no
	AssocData        Tcl_SetAssocData                   Tcl {Tcl C API}                     no
	IntObj           Tcl_SetBignumObj                   Tcl {Tcl C API}                     no
	BoolObj          Tcl_SetBooleanObj                  Tcl {Tcl C API}                     no
	ByteArrObj       Tcl_SetByteArrayLength             Tcl {Tcl C API}                     no
	ByteArrObj       Tcl_SetByteArrayObj                Tcl {Tcl C API}                     no
	CrtChannel       Tcl_SetChannelBufferSize           Tcl {Tcl C API}                     no
	SetChanErr       Tcl_SetChannelError                Tcl {Tcl C API}                     no
	SetChanErr       Tcl_SetChannelErrorInterp          Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_SetChannelOption               Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_SetCommandInfo                 Tcl {Tcl C API}                     no
	CrtObjCmd        Tcl_SetCommandInfoFromToken        Tcl {Tcl C API}                     no
	DoubleObj        Tcl_SetDoubleObj                   Tcl {Tcl C API}                     no
	Encoding3        Tcl_SetEncodingSearchPath          Tcl {Tcl C API}                     no
	Ensemble         Tcl_SetEnsembleFlags               Tcl {Tcl C API}                     no
	Ensemble         Tcl_SetEnsembleMappingDict         Tcl {Tcl C API}                     no
	Ensemble         Tcl_SetEnsembleParameterList       Tcl {Tcl C API}                     no
	Ensemble         Tcl_SetEnsembleSubcommandList      Tcl {Tcl C API}                     no
	Ensemble         Tcl_SetEnsembleUnknownHandler      Tcl {Tcl C API}                     no
	SetErrno         Tcl_SetErrno                       Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_SetErrorCode                   Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_SetErrorLine                   Tcl {Tcl C API}                     no
	Exit3            Tcl_SetExitProc                    Tcl {Tcl C API}                     no
	Hash             Tcl_SetHashValue                   Tcl {Tcl C API}                     no
	IntObj           Tcl_SetIntObj                      Tcl {Tcl C API}                     no
	ListObj          Tcl_SetListObj                     Tcl {Tcl C API}                     no
	IntObj           Tcl_SetLongObj                     Tcl {Tcl C API}                     no
	Tcl_Main         Tcl_SetMainLoop                    Tcl {Tcl C API}                     no
	Notifier         Tcl_SetMaxBlockTime                Tcl {Tcl C API}                     no
	Namespace3       Tcl_SetNamespaceUnknownHandler     Tcl {Tcl C API}                     no
	Notifier         Tcl_SetNotifier                    Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_SetObjErrorCode                Tcl {Tcl C API}                     no
	StringObj        Tcl_SetObjLength                   Tcl {Tcl C API}                     no
	SetResult        Tcl_SetObjResult                   Tcl {Tcl C API}                     no
	Panic            Tcl_SetPanicProc                   Tcl {Tcl C API}                     no
	SetRecLmt        Tcl_SetRecursionLimit              Tcl {Tcl C API}                     no
	SetResult        Tcl_SetResult                      Tcl {Tcl C API}                     no
	AddErrInfo       Tcl_SetReturnOptions               Tcl {Tcl C API}                     no
	Notifier         Tcl_SetServiceMode                 Tcl {Tcl C API}                     no
	Tcl_Main         Tcl_SetStartupScript               Tcl {Tcl C API}                     no
	GetStdChan       Tcl_SetStdChannel                  Tcl {Tcl C API}                     no
	StringObj        Tcl_SetStringObj                   Tcl {Tcl C API}                     no
	Encoding3        Tcl_SetSystemEncoding              Tcl {Tcl C API}                     no
	GetTime          Tcl_SetTimeProc                    Tcl {Tcl C API}                     no
	Notifier         Tcl_SetTimer                       Tcl {Tcl C API}                     no
	StringObj        Tcl_SetUnicodeObj                  Tcl {Tcl C API}                     no
	SetVar           Tcl_SetVar                         Tcl {Tcl C API}                     no
	SetVar           Tcl_SetVar2                        Tcl {Tcl C API}                     no
	SetVar           Tcl_SetVar2Ex                      Tcl {Tcl C API}                     no
	IntObj           Tcl_SetWideIntObj                  Tcl {Tcl C API}                     no
	IntObj           Tcl_SetWideUIntObj                 Tcl {Tcl C API}                     no
	Signal           Tcl_SignalId                       Tcl {Tcl C API}                     no
	Signal           Tcl_SignalMsg                      Tcl {Tcl C API}                     no
	Sleep            Tcl_Sleep                          Tcl {Tcl C API}                     no
	SourceRCFile     Tcl_SourceRCFile                   Tcl {Tcl C API}                     no
	CrtChannel       Tcl_SpliceChannel                  Tcl {Tcl C API}                     no
	SplitList        Tcl_SplitList                      Tcl {Tcl C API}                     no
	SplitPath        Tcl_SplitPath                      Tcl {Tcl C API}                     no
	ChnlStack        Tcl_StackChannel                   Tcl {Tcl C API}                     no
	StdChannels      Tcl_StandardChannels               Tcl {Tcl C API}                     no
	Access           Tcl_Stat                           Tcl {Tcl C API}                     no
	StaticLibrary    Tcl_StaticLibrary                  Tcl {Tcl C API}                     no
	StaticLibrary    Tcl_StaticPackage                  Tcl {Tcl C API}                     no
	ObjectType       Tcl_StoreInternalRep               Tcl {Tcl C API}                     no
	StrMatch         Tcl_StringCaseMatch                Tcl {Tcl C API}                     no
	StrMatch         Tcl_StringMatch                    Tcl {Tcl C API}                     no
	SubstObj         Tcl_SubstObj                       Tcl {Tcl C API}                     no
	IntObj           Tcl_TakeBignumFromObj              Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Tell                           Tcl {Tcl C API}                     no
	Notifier         Tcl_ThreadAlert                    Tcl {Tcl C API}                     no
	Notifier         Tcl_ThreadQueueEvent               Tcl {Tcl C API}                     no
	TraceCmd         Tcl_TraceCommand                   Tcl {Tcl C API}                     no
	TraceVar         Tcl_TraceVar                       Tcl {Tcl C API}                     no
	TraceVar         Tcl_TraceVar2                      Tcl {Tcl C API}                     no
	SetResult        Tcl_TransferResult                 Tcl {Tcl C API}                     no
	Translate        Tcl_TranslateFileName              Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_TruncateChannel                Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Ungets                         Tcl {Tcl C API}                     no
	Utf              Tcl_UniChar                        Tcl {Tcl C API}                     no
	Utf              Tcl_UniCharAtIndex                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsAlnum                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsAlpha                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsControl               Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsDigit                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsGraph                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsLower                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsPrint                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsPunct                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsSpace                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsUpper                 Tcl {Tcl C API}                     no
	UniCharIsAlpha   Tcl_UniCharIsWordChar              Tcl {Tcl C API}                     no
	Utf              Tcl_UniCharLen                     Tcl {Tcl C API}                     no
	ToUpper          Tcl_UniCharToLower                 Tcl {Tcl C API}                     no
	ToUpper          Tcl_UniCharToTitle                 Tcl {Tcl C API}                     no
	ToUpper          Tcl_UniCharToUpper                 Tcl {Tcl C API}                     no
	Utf              Tcl_UniCharToUtf                   Tcl {Tcl C API}                     no
	Utf              Tcl_UniCharToUtfDString            Tcl {Tcl C API}                     no
	LinkVar          Tcl_UnlinkVar                      Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_UnregisterChannel              Tcl {Tcl C API}                     no
	SetVar           Tcl_UnsetVar                       Tcl {Tcl C API}                     no
	SetVar           Tcl_UnsetVar2                      Tcl {Tcl C API}                     no
	ChnlStack        Tcl_UnstackChannel                 Tcl {Tcl C API}                     no
	TraceCmd         Tcl_UntraceCommand                 Tcl {Tcl C API}                     no
	TraceVar         Tcl_UntraceVar                     Tcl {Tcl C API}                     no
	TraceVar         Tcl_UntraceVar2                    Tcl {Tcl C API}                     no
	LinkVar          Tcl_UpdateLinkedVar                Tcl {Tcl C API}                     no
	UpVar3           Tcl_UpVar                          Tcl {Tcl C API}                     no
	UpVar3           Tcl_UpVar2                         Tcl {Tcl C API}                     no
	Utf              Tcl_UtfAtIndex                     Tcl {Tcl C API}                     no
	Utf              Tcl_UtfBackslash                   Tcl {Tcl C API}                     no
	Utf              Tcl_UtfCharComplete                Tcl {Tcl C API}                     no
	Utf              Tcl_UtfFindFirst                   Tcl {Tcl C API}                     no
	Utf              Tcl_UtfFindLast                    Tcl {Tcl C API}                     no
	Utf              Tcl_UtfNcasecmp                    Tcl {Tcl C API}                     no
	Utf              Tcl_UtfNcmp                        Tcl {Tcl C API}                     no
	Utf              Tcl_UtfNext                        Tcl {Tcl C API}                     no
	Utf              Tcl_UtfPrev                        Tcl {Tcl C API}                     no
	Utf              Tcl_UtfToChar16                    Tcl {Tcl C API}                     no
	Utf              Tcl_UtfToChar16DString             Tcl {Tcl C API}                     no
	Encoding3        Tcl_UtfToExternal                  Tcl {Tcl C API}                     no
	Encoding3        Tcl_UtfToExternalDString           Tcl {Tcl C API}                     no
	Encoding3        Tcl_UtfToExternalDStringEx         Tcl {Tcl C API}                     no
	Encoding3        Tcl_UtfToExternalEx                Tcl {Tcl C API}                     no
	ToUpper          Tcl_UtfToLower                     Tcl {Tcl C API}                     no
	UnicodeNormalize Tcl_UtfToNormalized                Tcl {Tcl C API}                     no
	UnicodeNormalize Tcl_UtfToNormalizedDString         Tcl {Tcl C API}                     no
	ToUpper          Tcl_UtfToTitle                     Tcl {Tcl C API}                     no
	Utf              Tcl_UtfToUniChar                   Tcl {Tcl C API}                     no
	Utf              Tcl_UtfToUniCharDString            Tcl {Tcl C API}                     no
	ToUpper          Tcl_UtfToUpper                     Tcl {Tcl C API}                     no
	Utf              Tcl_UtfToWChar                     Tcl {Tcl C API}                     no
	Utf              Tcl_UtfToWCharDString              Tcl {Tcl C API}                     no
	DumpActiveMemory Tcl_ValidateAllMemory              Tcl {Tcl C API}                     no
	Eval3            Tcl_VarEval                        Tcl {Tcl C API}                     no
	TraceVar         Tcl_VarTraceInfo                   Tcl {Tcl C API}                     no
	TraceVar         Tcl_VarTraceInfo2                  Tcl {Tcl C API}                     no
	Notifier         Tcl_WaitForEvent                   Tcl {Tcl C API}                     no
	DetachPids       Tcl_WaitPid                        Tcl {Tcl C API}                     no
	Utf              Tcl_WCharLen                       Tcl {Tcl C API}                     no
	Utf              Tcl_WCharToUtfDString              Tcl {Tcl C API}                     no
	SetErrno         Tcl_WinConvertError                Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_Write                          Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_WriteChars                     Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_WriteObj                       Tcl {Tcl C API}                     no
	OpenFileChnl     Tcl_WriteRaw                       Tcl {Tcl C API}                     no
	WrongNumArgs     Tcl_WrongNumArgs                   Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibAdler32                    Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibCRC32                      Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibDeflate                    Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibInflate                    Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibStreamChecksum             Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibStreamClose                Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibStreamEof                  Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibStreamGet                  Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibStreamGetCommandName       Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibStreamInit                 Tcl {Tcl C API}                     no
	TclZlib          Tcl_ZlibStreamPut                  Tcl {Tcl C API}                     no
	CrtTimerHdlr     TclCreateTimerHandlerMicroSeconds  Tcl {Tcl C API}                     no
	zipfs3           TclZipfs_AppHook                   Tcl {Tcl C API}                     no
	zipfs3           TclZipfs_Mount                     Tcl {Tcl C API}                     no
	zipfs3           TclZipfs_MountBuffer               Tcl {Tcl C API}                     no
	zipfs3           TclZipfs_Unmount                   Tcl {Tcl C API}                     no
} {
	# code here to build the left panel of the webpage for navigation:
	# a page may appear several times (e.g. one entry per C API function documented on it)
	dict set manFiles $group2/$title [list file $file title $title group1 $group1 group2 $group2 TclOO $TclOO]
}


# convert the markdown-formatted manual pages 
# (as produced by man2markdown.tcl) into HTML-formatted manual pages,
# using Pandoc.
# These parts are shwon in the right panel of the webpage when clicking on the respective entry in the outline of the left panel:
# - the Tcl pages go into the Tcl folder, the Tk pages into the Tk folder (as determined by "group2" in the table above)
# - the Tcl C API goes into the TclCAPI folder, the Tk C API into the TkCAPI folder (as determined by "group2" in the table above)
file mkdir \
	../doc/html/Tcl \
	../doc/html/Tk \
	../doc/html/TclCAPI \
	../doc/html/TkCAPI

set converted [dict create] 

foreach entry [dict keys $manFiles] {
	set myFile [dict get $manFiles $entry file]
	if {[dict exists $converted $myFile]} continue
	dict set converted $myFile 1
	set myFolder {}
	set g [dict get $manFiles $entry group2]
	switch $g {
		{Tcl C API} {set myFolder TclCAPI}
		{Tk C API}  {set myFolder TkCAPI}
		default {
			if {[string match Tcl* $g]} {set myFolder Tcl}
			if {[string match Tk* $g]}  {set myFolder Tk}
		}
	}
	if {$myFolder eq ""} {return -code error "no folder for file '$myFile'.md"}
	exec pandoc -f markdown-tex_math_dollars-smart -t html \
		-s -c [file join .. tcl-docs.css] -o [file join .. doc html $myFolder $myFile.html] [file join .. doc markdown $myFile.md]
}
