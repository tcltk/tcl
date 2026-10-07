# markdown2html.tcl --
#
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
#
foreach {
	file             title                             group1  group2
} {
	Tcl              {Language syntax}                  Tcl {Tcl syntax}
	tclsh            tclsh                              Tcl {Tcl applications}
	after            after                              Tcl {Tcl commands}
	append           append                             Tcl {Tcl commands}
	apply            apply                              Tcl {Tcl commands}
	array            array                              Tcl {Tcl commands}
	bgerror          bgerror                            Tcl {Tcl commands}
	binary           binary                             Tcl {Tcl commands}
	break            break                              Tcl {Tcl commands}
	buildinfo        buildinfo                          Tcl {Tcl commands}
	callback         callback                           Tcl {TclOO commands}
	catch            catch                              Tcl {Tcl commands}
	cd               cd                                 Tcl {Tcl commands}
	chan             chan                               Tcl {Tcl commands}
	classvariable    classvariable                      Tcl {TclOO commands}
	clock            clock                              Tcl {Tcl commands}
	close            close                              Tcl {Tcl commands}
	concat           concat                             Tcl {Tcl commands}
	configurable     configure                          Tcl {TclOO commands}
	const            const                              Tcl {Tcl commands}
	continue         continue                           Tcl {Tcl commands}
	cookiejar        cookiejar                          Tcl {Tcl commands}
	coroutine        coroinject                         Tcl {Tcl commands}
	coroutine        coroprobe                          Tcl {Tcl commands}
	coroutine        coroutine                          Tcl {Tcl commands}
	dde              dde                                Tcl {Tcl commands}
	dict             dict                               Tcl {Tcl commands}
	divmod           divmod                             Tcl {Tcl commands}
	encoding         encoding                           Tcl {Tcl commands}
	eof              eof                                Tcl {Tcl commands}
	error            error                              Tcl {Tcl commands}
	eval             eval                               Tcl {Tcl commands}
	exec             exec                               Tcl {Tcl commands}
	exit             exit                               Tcl {Tcl commands}
	expr             expr                               Tcl {Tcl commands}
	fblocked         fblocked                           Tcl {Tcl commands}
	fconfigure       fconfigure                         Tcl {Tcl commands}
	fcopy            fcopy                              Tcl {Tcl commands}
	file             file                               Tcl {Tcl commands}
	fileevent        fileevent                          Tcl {Tcl commands}
	flush            flush                              Tcl {Tcl commands}
	for              for                                Tcl {Tcl commands}
	foreach          foreach                            Tcl {Tcl commands}
	format           format                             Tcl {Tcl commands}
	fpclassify       fpclassify                         Tcl {Tcl commands}
	frexp            frexp                              Tcl {Tcl commands}
	gets             gets                               Tcl {Tcl commands}
	glob             glob                               Tcl {Tcl commands}
	global           global                             Tcl {Tcl commands}
	history          history                            Tcl {Tcl commands}
	http             http                               Tcl {Tcl commands}
	if               if                                 Tcl {Tcl commands}
	incr             incr                               Tcl {Tcl commands}
	info             info                               Tcl {Tcl commands}
	interp           interp                             Tcl {Tcl commands}
	join             join                               Tcl {Tcl commands}
	lappend          lappend                            Tcl {Tcl commands}
	lassign          lassign                            Tcl {Tcl commands}
	ledit            ledit                              Tcl {Tcl commands}
	lfilter          lfilter                            Tcl {Tcl commands}
	lindex           lindex                             Tcl {Tcl commands}
	link             link                               Tcl {TclOO commands}
	linsert          linsert                            Tcl {Tcl commands}
	list             list                               Tcl {Tcl commands}
	llength          llength                            Tcl {Tcl commands}
	lmap             lmap                               Tcl {Tcl commands}
	load             load                               Tcl {Tcl commands}
	lpop             lpop                               Tcl {Tcl commands}
	lrange           lrange                             Tcl {Tcl commands}
	lremove          lremove                            Tcl {Tcl commands}
	lrepeat          lrepeat                            Tcl {Tcl commands}
	lreplace         lreplace                           Tcl {Tcl commands}
	lreverse         lreverse                           Tcl {Tcl commands}
	lsearch          lsearch                            Tcl {Tcl commands}
	lseq             lseq                               Tcl {Tcl commands}
	lset             lset                               Tcl {Tcl commands}
	lsort            lsort                              Tcl {Tcl commands}
	memory           memory                             Tcl {Tcl commands}
	modf             modf                               Tcl {Tcl commands}
	msgcat           msgcat                             Tcl {Tcl commands}
	my               my                                 Tcl {TclOO commands}
	my               myclass                            Tcl {TclOO commands}
	callback         mymethod                           Tcl {TclOO commands}
	namespace        namespace                          Tcl {Tcl commands}
	next             next                               Tcl {TclOO commands}
	next             nextto                             Tcl {TclOO commands}
	abstract         oo::abstract                       Tcl {TclOO commands}
	class            oo::class                          Tcl {TclOO commands}
	configurable     oo::configurable                   Tcl {TclOO commands}
	copy             oo::copy                           Tcl {TclOO commands}
	define           oo::define                         Tcl {TclOO commands}
	define           oo::objdefine                      Tcl {TclOO commands}
	object           oo::object                         Tcl {TclOO commands}
	singleton        oo::singleton                      Tcl {TclOO commands}
	define           oo::Slot                           Tcl {TclOO commands}
	open             open                               Tcl {Tcl commands}
	package          package                            Tcl {Tcl commands}
	pid              pid                                Tcl {Tcl commands}
	packagens        pkg::create                        Tcl {Tcl commands}
	pkgMkIndex       pkg_mkIndex                        Tcl {Tcl commands}
	platform         platform                           Tcl {Tcl commands}
	platform_shell   platform::shell                    Tcl {Tcl commands}
	proc             proc                               Tcl {Tcl commands}
	configurable     property                           Tcl {TclOO commands}
	puts             puts                               Tcl {Tcl commands}
	pwd              pwd                                Tcl {Tcl commands}
	read             read                               Tcl {Tcl commands}
	refchan          refchan                            Tcl {Tcl commands}
	regexp           regexp                             Tcl {Tcl commands}
	registry         registry                           Tcl {Tcl commands}
	regsub           regsub                             Tcl {Tcl commands}
	remquo           remquo                             Tcl {Tcl commands}
	rename           rename                             Tcl {Tcl commands}
	return           return                             Tcl {Tcl commands}
	safe             safe                               Tcl {Tcl commands}
	scan             scan                               Tcl {Tcl commands}
	seek             seek                               Tcl {Tcl commands}
	self             self                               Tcl {TclOO commands}
	set              set                                Tcl {Tcl commands}
	socket           socket                             Tcl {Tcl commands}
	source           source                             Tcl {Tcl commands}
	split            split                              Tcl {Tcl commands}
	string           string                             Tcl {Tcl commands}
	subst            subst                              Tcl {Tcl commands}
	switch           switch                             Tcl {Tcl commands}
	tailcall         tailcall                           Tcl {Tcl commands}
	idna             tcl::idna                          Tcl {Tcl commands}
	prefix           tcl::prefix                        Tcl {Tcl commands}
	process          tcl::process                       Tcl {Tcl commands}
	tcltest          tcltest                            Tcl {Tcl commands}
	tell             tell                               Tcl {Tcl commands}
	throw            throw                              Tcl {Tcl commands}
	time             time                               Tcl {Tcl commands}
	timer            timer                              Tcl {Tcl commands}
	timerate         timerate                           Tcl {Tcl commands}
	tm               tm                                 Tcl {Tcl commands}
	trace            trace                              Tcl {Tcl commands}
	transchan        transchan                          Tcl {Tcl commands}
	try              try                                Tcl {Tcl commands}
	unicode          unicode                            Tcl {Tcl commands}
	unknown          unknown                            Tcl {Tcl commands}
	unload           unload                             Tcl {Tcl commands}
	unset            unset                              Tcl {Tcl commands}
	update           update                             Tcl {Tcl commands}
	uplevel          uplevel                            Tcl {Tcl commands}
	upvar            upvar                              Tcl {Tcl commands}
	variable         variable                           Tcl {Tcl commands}
	vwait            vwait                              Tcl {Tcl commands}
	while            while                              Tcl {Tcl commands}
	coroutine        yield                              Tcl {Tcl commands}
	coroutine        yieldto                            Tcl {Tcl commands}
	zipfs            zipfs                              Tcl {Tcl commands}
	zlib             zlib                               Tcl {Tcl commands}
	tclvars          argc                               Tcl {Tcl variables}
	tclvars          argv                               Tcl {Tcl variables}
	tclvars          argv0                              Tcl {Tcl variables}
	tclvars          auto_path                          Tcl {Tcl variables}
	tclvars          env                                Tcl {Tcl variables}
	tclvars          errorCode                          Tcl {Tcl variables}
	tclvars          errorInfo                          Tcl {Tcl variables}
	tclvars          tcl_interactive                    Tcl {Tcl variables}
	tclvars          tcl_library                        Tcl {Tcl variables}
	tclvars          tcl_patchLevel                     Tcl {Tcl variables}
	tclvars          tcl_pkgPath                        Tcl {Tcl variables}
	tclvars          tcl_platform                       Tcl {Tcl variables}
	tclvars          tcl_rcFileName                     Tcl {Tcl variables}
	tclvars          tcl_traceCompile                   Tcl {Tcl variables}
	tclvars          tcl_traceExec                      Tcl {Tcl variables}
	tclvars          tcl_version                        Tcl {Tcl variables}
	mathfunc         {Tcl math functions}               Tcl {Tcl math functions}
	mathop           {Tcl math operators}               Tcl {Tcl math operators}
	filename         {Tcl filename conventions}         Tcl {Tcl filename conventions}
	re_syntax        {Tcl regular expression syntax}    Tcl {Tcl regular expression syntax}
	library          {Library procedures}               Tcl {Tcl library procedures}
	Access           Tcl_Access                         Tcl {Tcl C API}
	AddErrInfo       Tcl_AddErrorInfo                   Tcl {Tcl C API}
	AddErrInfo       Tcl_AddObjErrorInfo                Tcl {Tcl C API}
	Notifier         Tcl_AlertNotifier                  Tcl {Tcl C API}
	Alloc            Tcl_Alloc                          Tcl {Tcl C API}
	FileSystem       Tcl_AllocStatBuf                   Tcl {Tcl C API}
	AllowExc         Tcl_AllowExceptions                Tcl {Tcl C API}
	ObjectType       Tcl_AppendAllObjTypes              Tcl {Tcl C API}
	SetResult        Tcl_AppendElement                  Tcl {Tcl C API}
	Namespace        Tcl_AppendExportList               Tcl {Tcl C API}
	StringObj        Tcl_AppendFormatToObj              Tcl {Tcl C API}
	StringObj        Tcl_AppendLimitedToObj             Tcl {Tcl C API}
	AddErrInfo       Tcl_AppendObjToErrorInfo           Tcl {Tcl C API}
	StringObj        Tcl_AppendObjToObj                 Tcl {Tcl C API}
	StringObj        Tcl_AppendPrintfToObj              Tcl {Tcl C API}
	SetResult        Tcl_AppendResult                   Tcl {Tcl C API}
	StringObj        Tcl_AppendStringsToObj             Tcl {Tcl C API}
	StringObj        Tcl_AppendToObj                    Tcl {Tcl C API}
	StringObj        Tcl_AppendUnicodeToObj             Tcl {Tcl C API}
	AppInit          Tcl_AppInit                        Tcl {Tcl C API}
	Async            Tcl_AsyncCreate                    Tcl {Tcl C API}
	Async            Tcl_AsyncDelete                    Tcl {Tcl C API}
	Async            Tcl_AsyncInvoke                    Tcl {Tcl C API}
	Async            Tcl_AsyncMark                      Tcl {Tcl C API}
	Async            Tcl_AsyncMarkFromSignal            Tcl {Tcl C API}
	Async            Tcl_AsyncReady                     Tcl {Tcl C API}
	Alloc            Tcl_AttemptAlloc                   Tcl {Tcl C API}
	Hash             Tcl_AttemptCreateHashEntry         Tcl {Tcl C API}
	Alloc            Tcl_AttemptRealloc                 Tcl {Tcl C API}
	StringObj        Tcl_AttemptSetObjLength            Tcl {Tcl C API}
	BackgdErr        Tcl_BackgroundError                Tcl {Tcl C API}
	BackgdErr        Tcl_BackgroundException            Tcl {Tcl C API}
	CrtChannel       Tcl_BadChannelOption               Tcl {Tcl C API}
	Object           Tcl_BounceRefCount                 Tcl {Tcl C API}
	CallDel          Tcl_CallWhenDeleted                Tcl {Tcl C API}
	Cancel           Tcl_Canceled                       Tcl {Tcl C API}
	Cancel           Tcl_CancelEval                     Tcl {Tcl C API}
	DoWhenIdle       Tcl_CancelIdleCall                 Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelBlockModeProc           Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelBuffered                Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelClose2Proc              Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelFlushProc               Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelGetHandleProc           Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelGetOptionProc           Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelHandlerProc             Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelInputProc               Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelName                    Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelOutputProc              Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelSetOptionProc           Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelThreadActionProc        Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelTruncateProc            Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelVersion                 Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelWatchProc               Tcl {Tcl C API}
	CrtChannel       Tcl_ChannelWideSeekProc            Tcl {Tcl C API}
	Utf              Tcl_Char16Len                      Tcl {Tcl C API}
	Utf              Tcl_Char16ToUtfDString             Tcl {Tcl C API}
	GetCwd           Tcl_Chdir                          Tcl {Tcl C API}
	Class            Tcl_ClassGetMetadata               Tcl {TclOO C API}           
	Method           Tcl_ClassSetConstructor            Tcl {TclOO C API}           
	Method           Tcl_ClassSetDestructor             Tcl {TclOO C API}           
	Class            Tcl_ClassSetMetadata               Tcl {TclOO C API}           
	CrtChannel       Tcl_ClearChannelHandlers           Tcl {Tcl C API}
	OpenFileChnl     Tcl_Close                          Tcl {Tcl C API}
	OpenFileChnl     Tcl_CloseEx                        Tcl {Tcl C API}
	CmdCmplt         Tcl_CommandComplete                Tcl {Tcl C API}
	TraceCmd         Tcl_CommandTraceInfo               Tcl {Tcl C API}
	Concat           Tcl_Concat                         Tcl {Tcl C API}
	StringObj        Tcl_ConcatObj                      Tcl {Tcl C API}
	Thread           Tcl_ConditionFinalize              Tcl {Tcl C API}
	Thread           Tcl_ConditionNotify                Tcl {Tcl C API}
	Thread           Tcl_ConditionWait                  Tcl {Tcl C API}
	Panic            Tcl_ConsolePanic                   Tcl {Tcl C API}
	SplitList        Tcl_ConvertCountedElement          Tcl {Tcl C API}
	SplitList        Tcl_ConvertElement                 Tcl {Tcl C API}
	ObjectType       Tcl_ConvertToType                  Tcl {Tcl C API}
	Class            Tcl_CopyObjectInstance             Tcl {TclOO C API}
	CrtAlias         Tcl_CreateAlias                    Tcl {Tcl C API}
	CrtAlias         Tcl_CreateAliasObj                 Tcl {Tcl C API}
	CrtChannel       Tcl_CreateChannel                  Tcl {Tcl C API}
	CrtChnlHdlr      Tcl_CreateChannelHandler           Tcl {Tcl C API}
	CrtAlias         Tcl_CreateChild                    Tcl {Tcl C API}
	CrtCloseHdlr     Tcl_CreateCloseHandler             Tcl {Tcl C API}
	CrtCommand       Tcl_CreateCommand                  Tcl {Tcl C API}
	Encoding         Tcl_CreateEncoding                 Tcl {Tcl C API}
	Ensemble         Tcl_CreateEnsemble                 Tcl {Tcl C API}
	Notifier         Tcl_CreateEventSource              Tcl {Tcl C API}
	Exit             Tcl_CreateExitHandler              Tcl {Tcl C API}
	CrtFileHdlr      Tcl_CreateFileHandler              Tcl {Tcl C API}
	Hash             Tcl_CreateHashEntry                Tcl {Tcl C API}
	CrtInterp        Tcl_CreateInterp                   Tcl {Tcl C API}
	Namespace        Tcl_CreateNamespace                Tcl {Tcl C API}
	CrtObjCmd        Tcl_CreateObjCommand               Tcl {Tcl C API}
	CrtObjCmd        Tcl_CreateObjCommand2              Tcl {Tcl C API}
	CrtTrace         Tcl_CreateObjTrace                 Tcl {Tcl C API}
	CrtTrace         Tcl_CreateObjTrace2                Tcl {Tcl C API}
	Thread           Tcl_CreateThread                   Tcl {Tcl C API}
	Exit             Tcl_CreateThreadExitHandler        Tcl {Tcl C API}
	CrtTimerHdlr     Tcl_CreateTimerHandler             Tcl {Tcl C API}
	CrtTrace         Tcl_CreateTrace                    Tcl {Tcl C API}
	CrtChannel       Tcl_CutChannel                     Tcl {Tcl C API}
	Object           Tcl_DecrRefCount                   Tcl {Tcl C API}
	AssocData        Tcl_DeleteAssocData                Tcl {Tcl C API}
	CrtChnlHdlr      Tcl_DeleteChannelHandler           Tcl {Tcl C API}
	CrtCloseHdlr     Tcl_DeleteCloseHandler             Tcl {Tcl C API}
	CrtObjCmd        Tcl_DeleteCommand                  Tcl {Tcl C API}
	CrtObjCmd        Tcl_DeleteCommandFromToken         Tcl {Tcl C API}
	Notifier         Tcl_DeleteEvents                   Tcl {Tcl C API}
	Notifier         Tcl_DeleteEventSource              Tcl {Tcl C API}
	Exit             Tcl_DeleteExitHandler              Tcl {Tcl C API}
	CrtFileHdlr      Tcl_DeleteFileHandler              Tcl {Tcl C API}
	Hash             Tcl_DeleteHashEntry                Tcl {Tcl C API}
	Hash             Tcl_DeleteHashTable                Tcl {Tcl C API}
	CrtInterp        Tcl_DeleteInterp                   Tcl {Tcl C API}
	Namespace        Tcl_DeleteNamespace                Tcl {Tcl C API}
	Exit             Tcl_DeleteThreadExitHandler        Tcl {Tcl C API}
	CrtTimerHdlr     Tcl_DeleteTimerHandler             Tcl {Tcl C API}
	CrtTrace         Tcl_DeleteTrace                    Tcl {Tcl C API}
	OpenFileChnl     Tcl_DetachChannel                  Tcl {Tcl C API}
	DetachPids       Tcl_DetachPids                     Tcl {Tcl C API}
	DictObj          Tcl_DictObjDone                    Tcl {Tcl C API}
	DictObj          Tcl_DictObjFirst                   Tcl {Tcl C API}
	DictObj          Tcl_DictObjGet                     Tcl {Tcl C API}
	DictObj          Tcl_DictObjNext                    Tcl {Tcl C API}
	DictObj          Tcl_DictObjPut                     Tcl {Tcl C API}
	DictObj          Tcl_DictObjPutKeyList              Tcl {Tcl C API}
	DictObj          Tcl_DictObjRemove                  Tcl {Tcl C API}
	DictObj          Tcl_DictObjRemoveKeyList           Tcl {Tcl C API}
	DictObj          Tcl_DictObjSize                    Tcl {Tcl C API}
	SaveInterpState  Tcl_DiscardInterpState             Tcl {Tcl C API}
	CallDel          Tcl_DontCallWhenDeleted            Tcl {Tcl C API}
	DoOneEvent       Tcl_DoOneEvent                     Tcl {Tcl C API}
	DoWhenIdle       Tcl_DoWhenIdle                     Tcl {Tcl C API}
	DString          Tcl_DStringAppend                  Tcl {Tcl C API}
	DString          Tcl_DStringAppendElement           Tcl {Tcl C API}
	DString          Tcl_DStringEndSublist              Tcl {Tcl C API}
	DString          Tcl_DStringFree                    Tcl {Tcl C API}
	DString          Tcl_DStringGetResult               Tcl {Tcl C API}
	DString          Tcl_DStringInit                    Tcl {Tcl C API}
	DString          Tcl_DStringLength                  Tcl {Tcl C API}
	DString          Tcl_DStringResult                  Tcl {Tcl C API}
	DString          Tcl_DStringSetLength               Tcl {Tcl C API}
	DString          Tcl_DStringStartSublist            Tcl {Tcl C API}
	DString          Tcl_DStringToObj                   Tcl {Tcl C API}
	DString          Tcl_DStringValue                   Tcl {Tcl C API}
	DumpActiveMemory Tcl_DumpActiveMemory               Tcl {Tcl C API}
	Object           Tcl_DuplicateObj                   Tcl {Tcl C API}
	OpenFileChnl     Tcl_Eof                            Tcl {Tcl C API}
	SetErrno         Tcl_ErrnoId                        Tcl {Tcl C API}
	SetErrno         Tcl_ErrnoMsg                       Tcl {Tcl C API}
	Eval             Tcl_Eval                           Tcl {Tcl C API}
	Eval             Tcl_EvalEx                         Tcl {Tcl C API}
	Eval             Tcl_EvalFile                       Tcl {Tcl C API}
	Eval             Tcl_EvalObjEx                      Tcl {Tcl C API}
	Eval             Tcl_EvalObjv                       Tcl {Tcl C API}
	ParseCmd         Tcl_EvalTokensStandard             Tcl {Tcl C API}
	Preserve         Tcl_EventuallyFree                 Tcl {Tcl C API}
	Exit             Tcl_Exit                           Tcl {Tcl C API}
	Exit             Tcl_ExitThread                     Tcl {Tcl C API}
	Namespace        Tcl_Export                         Tcl {Tcl C API}
	CrtAlias         Tcl_ExposeCommand                  Tcl {Tcl C API}
	ExprLong         Tcl_ExprBoolean                    Tcl {Tcl C API}
	ExprLongObj      Tcl_ExprBooleanObj                 Tcl {Tcl C API}
	ExprLong         Tcl_ExprDouble                     Tcl {Tcl C API}
	ExprLongObj      Tcl_ExprDoubleObj                  Tcl {Tcl C API}
	ExprLong         Tcl_ExprLong                       Tcl {Tcl C API}
	ExprLongObj      Tcl_ExprLongObj                    Tcl {Tcl C API}
	ExprLongObj      Tcl_ExprObj                        Tcl {Tcl C API}
	ExprLong         Tcl_ExprString                     Tcl {Tcl C API}
	Encoding         Tcl_ExternalToUtf                  Tcl {Tcl C API}
	Encoding         Tcl_ExternalToUtfDString           Tcl {Tcl C API}
	Encoding         Tcl_ExternalToUtfDStringEx         Tcl {Tcl C API}
	Encoding         Tcl_ExternalToUtfEx                Tcl {Tcl C API}
	ObjectType       Tcl_FetchInternalRep               Tcl {Tcl C API}
	Exit             Tcl_Finalize                       Tcl {Tcl C API}
	Notifier         Tcl_FinalizeNotifier               Tcl {Tcl C API}
	Exit             Tcl_FinalizeThread                 Tcl {Tcl C API}
	Namespace        Tcl_FindCommand                    Tcl {Tcl C API}
	Ensemble         Tcl_FindEnsemble                   Tcl {Tcl C API}
	FindExec         Tcl_FindExecutable                 Tcl {Tcl C API}
	Hash             Tcl_FindHashEntry                  Tcl {Tcl C API}
	Namespace        Tcl_FindNamespace                  Tcl {Tcl C API}
	Load             Tcl_FindSymbol                     Tcl {Tcl C API}
	Hash             Tcl_FirstHashEntry                 Tcl {Tcl C API}
	OpenFileChnl     Tcl_Flush                          Tcl {Tcl C API}
	Namespace        Tcl_ForgetImport                   Tcl {Tcl C API}
	StringObj        Tcl_Format                         Tcl {Tcl C API}
	Alloc            Tcl_Free                           Tcl {Tcl C API}
	Encoding         Tcl_FreeEncoding                   Tcl {Tcl C API}
	ObjectType       Tcl_FreeInternalRep                Tcl {Tcl C API}
	ParseCmd         Tcl_FreeParse                      Tcl {Tcl C API}
	FileSystem       Tcl_FSAccess                       Tcl {Tcl C API}
	FileSystem       Tcl_FSChdir                        Tcl {Tcl C API}
	FileSystem       Tcl_FSConvertToPathType            Tcl {Tcl C API}
	FileSystem       Tcl_FSCopyDirectory                Tcl {Tcl C API}
	FileSystem       Tcl_FSCopyFile                     Tcl {Tcl C API}
	FileSystem       Tcl_FSCreateDirectory              Tcl {Tcl C API}
	FileSystem       Tcl_FSData                         Tcl {Tcl C API}
	FileSystem       Tcl_FSDeleteFile                   Tcl {Tcl C API}
	FileSystem       Tcl_FSEqualPaths                   Tcl {Tcl C API}
	FileSystem       Tcl_FSEvalFile                     Tcl {Tcl C API}
	FileSystem       Tcl_FSEvalFileEx                   Tcl {Tcl C API}
	FileSystem       Tcl_FSFileAttrsGet                 Tcl {Tcl C API}
	FileSystem       Tcl_FSFileAttrsSet                 Tcl {Tcl C API}
	FileSystem       Tcl_FSFileAttrStrings              Tcl {Tcl C API}
	FileSystem       Tcl_FSFileSystemInfo               Tcl {Tcl C API}
	FileSystem       Tcl_FSGetCwd                       Tcl {Tcl C API}
	FileSystem       Tcl_FSGetFileSystemForPath         Tcl {Tcl C API}
	FileSystem       Tcl_FSGetInternalRep               Tcl {Tcl C API}
	FileSystem       Tcl_FSGetNativePath                Tcl {Tcl C API}
	FileSystem       Tcl_FSGetNormalizedPath            Tcl {Tcl C API}
	FileSystem       Tcl_FSGetPathType                  Tcl {Tcl C API}
	FileSystem       Tcl_FSGetTranslatedPath            Tcl {Tcl C API}
	FileSystem       Tcl_FSGetTranslatedStringPath      Tcl {Tcl C API}
	FileSystem       Tcl_FSJoinPath                     Tcl {Tcl C API}
	FileSystem       Tcl_FSJoinToPath                   Tcl {Tcl C API}
	FileSystem       Tcl_FSLink                         Tcl {Tcl C API}
	FileSystem       Tcl_FSListVolumes                  Tcl {Tcl C API}
	FileSystem       Tcl_FSLoadFile                     Tcl {Tcl C API}
	FileSystem       Tcl_FSLstat                        Tcl {Tcl C API}
	FileSystem       Tcl_FSMatchInDirectory             Tcl {Tcl C API}
	FileSystem       Tcl_FSMountsChanged                Tcl {Tcl C API}
	FileSystem       Tcl_FSNewNativePath                Tcl {Tcl C API}
	FileSystem       Tcl_FSOpenFileChannel              Tcl {Tcl C API}
	FileSystem       Tcl_FSPathSeparator                Tcl {Tcl C API}
	FileSystem       Tcl_FSRegister                     Tcl {Tcl C API}
	FileSystem       Tcl_FSRemoveDirectory              Tcl {Tcl C API}
	FileSystem       Tcl_FSRenameFile                   Tcl {Tcl C API}
	FileSystem       Tcl_FSSplitPath                    Tcl {Tcl C API}
	FileSystem       Tcl_FSStat                         Tcl {Tcl C API}
	FileSystem       Tcl_FSTildeExpand                  Tcl {Tcl C API}
	FileSystem       Tcl_FSUnloadFile                   Tcl {Tcl C API}
	FileSystem       Tcl_FSUnregister                   Tcl {Tcl C API}
	FileSystem       Tcl_FSUtime                        Tcl {Tcl C API}
	FileSystem       Tcl_GetAccessTimeFromStat          Tcl {Tcl C API}
	CrtAlias         Tcl_GetAliasObj                    Tcl {Tcl C API}
	AssocData        Tcl_GetAssocData                   Tcl {Tcl C API}
	IntObj           Tcl_GetBignumFromObj               Tcl {Tcl C API}
	FileSystem       Tcl_GetBlocksFromStat              Tcl {Tcl C API}
	FileSystem       Tcl_GetBlockSizeFromStat           Tcl {Tcl C API}
	GetInt           Tcl_GetBoolean                     Tcl {Tcl C API}
	BoolObj          Tcl_GetBooleanFromObj              Tcl {Tcl C API}
	BoolObj          Tcl_GetBoolFromObj                 Tcl {Tcl C API}
	ByteArrObj       Tcl_GetByteArrayFromObj            Tcl {Tcl C API}
	ByteArrObj       Tcl_GetBytesFromObj                Tcl {Tcl C API}
	FileSystem       Tcl_GetChangeTimeFromStat          Tcl {Tcl C API}
	OpenFileChnl     Tcl_GetChannel                     Tcl {Tcl C API}
	CrtChannel       Tcl_GetChannelBufferSize           Tcl {Tcl C API}
	SetChanErr       Tcl_GetChannelError                Tcl {Tcl C API}
	SetChanErr       Tcl_GetChannelErrorInterp          Tcl {Tcl C API}
	CrtChannel       Tcl_GetChannelHandle               Tcl {Tcl C API}
	CrtChannel       Tcl_GetChannelInstanceData         Tcl {Tcl C API}
	CrtChannel       Tcl_GetChannelMode                 Tcl {Tcl C API}
	CrtChannel       Tcl_GetChannelName                 Tcl {Tcl C API}
	OpenFileChnl     Tcl_GetChannelNames                Tcl {Tcl C API}
	OpenFileChnl     Tcl_GetChannelNamesEx              Tcl {Tcl C API}
	OpenFileChnl     Tcl_GetChannelOption               Tcl {Tcl C API}
	CrtChannel       Tcl_GetChannelThread               Tcl {Tcl C API}
	CrtChannel       Tcl_GetChannelType                 Tcl {Tcl C API}
	StringObj        Tcl_GetCharLength                  Tcl {Tcl C API}
	CrtAlias         Tcl_GetChild                       Tcl {Tcl C API}
	Class            Tcl_GetClassAsObject               Tcl {TclOO C API}
	CrtObjCmd        Tcl_GetCommandFromObj              Tcl {Tcl C API}
	CrtObjCmd        Tcl_GetCommandFullName             Tcl {Tcl C API}
	CrtObjCmd        Tcl_GetCommandInfo                 Tcl {Tcl C API}
	CrtObjCmd        Tcl_GetCommandInfoFromToken        Tcl {Tcl C API}
	CrtObjCmd        Tcl_GetCommandName                 Tcl {Tcl C API}
	Namespace        Tcl_GetCurrentNamespace            Tcl {Tcl C API}
	Notifier         Tcl_GetCurrentThread               Tcl {Tcl C API}
	GetCwd           Tcl_GetCwd                         Tcl {Tcl C API}
	FileSystem       Tcl_GetDeviceTypeFromStat          Tcl {Tcl C API}
	GetInt           Tcl_GetDouble                      Tcl {Tcl C API}
	DoubleObj        Tcl_GetDoubleFromObj               Tcl {Tcl C API}
	Encoding         Tcl_GetEncoding                    Tcl {Tcl C API}
	Encoding         Tcl_GetEncodingFromObj             Tcl {Tcl C API}
	Encoding         Tcl_GetEncodingName                Tcl {Tcl C API}
	Encoding         Tcl_GetEncodingNameForUser         Tcl {Tcl C API}
	Encoding         Tcl_GetEncodingNameFromEnvironment Tcl {Tcl C API}
	Encoding         Tcl_GetEncodingNames               Tcl {Tcl C API}
	Encoding         Tcl_GetEncodingSearchPath          Tcl {Tcl C API}
	Ensemble         Tcl_GetEnsembleFlags               Tcl {Tcl C API}
	Ensemble         Tcl_GetEnsembleMappingDict         Tcl {Tcl C API}
	Ensemble         Tcl_GetEnsembleNamespace           Tcl {Tcl C API}
	Ensemble         Tcl_GetEnsembleParameterList       Tcl {Tcl C API}
	Ensemble         Tcl_GetEnsembleSubcommandList      Tcl {Tcl C API}
	Ensemble         Tcl_GetEnsembleUnknownHandler      Tcl {Tcl C API}
	SetErrno         Tcl_GetErrno                       Tcl {Tcl C API}
	AddErrInfo       Tcl_GetErrorLine                   Tcl {Tcl C API}
	FileSystem       Tcl_GetFSDeviceFromStat            Tcl {Tcl C API}
	FileSystem       Tcl_GetFSInodeFromStat             Tcl {Tcl C API}
	Namespace        Tcl_GetGlobalNamespace             Tcl {Tcl C API}
	FileSystem       Tcl_GetGroupIdFromStat             Tcl {Tcl C API}
	Hash             Tcl_GetHashKey                     Tcl {Tcl C API}
	Hash             Tcl_GetHashValue                   Tcl {Tcl C API}
	GetHostName      Tcl_GetHostName                    Tcl {Tcl C API}
	GetIndex         Tcl_GetIndexFromObj                Tcl {Tcl C API}
	GetIndex         Tcl_GetIndexFromObjStruct          Tcl {Tcl C API}
	GetInt           Tcl_GetInt                         Tcl {Tcl C API}
	CrtAlias         Tcl_GetInterpPath                  Tcl {Tcl C API}
	IntObj           Tcl_GetIntForIndex                 Tcl {Tcl C API}
	IntObj           Tcl_GetIntFromObj                  Tcl {Tcl C API}
	FileSystem       Tcl_GetLinkCountFromStat           Tcl {Tcl C API}
	IntObj           Tcl_GetLongFromObj                 Tcl {Tcl C API}
	Alloc            Tcl_GetMemoryInfo                  Tcl {Tcl C API}
	FileSystem       Tcl_GetModeFromStat                Tcl {Tcl C API}
	FileSystem       Tcl_GetModificationTimeFromStat    Tcl {Tcl C API}
	GetTime          Tcl_GetMonotonicTime               Tcl {Tcl C API}
	FindExec         Tcl_GetNameOfExecutable            Tcl {Tcl C API}
	Namespace        Tcl_GetNamespaceUnknownHandler     Tcl {Tcl C API}
	Number           Tcl_GetNumber                      Tcl {Tcl C API}
	Number           Tcl_GetNumberFromObj               Tcl {Tcl C API}
	Class            Tcl_GetObjectAsClass               Tcl {TclOO C API}         
	Class            Tcl_GetObjectCommand               Tcl {TclOO C API}         
	Class            Tcl_GetObjectFromObj               Tcl {TclOO C API}         
	Class            Tcl_GetObjectName                  Tcl {TclOO C API}         
	Class            Tcl_GetObjectNamespace             Tcl {TclOO C API}         
	SetResult        Tcl_GetObjResult                   Tcl {Tcl C API}
	ObjectType       Tcl_GetObjType                     Tcl {Tcl C API}
	GetOpnFl         Tcl_GetOpenFile                    Tcl {Tcl C API}
	CrtAlias         Tcl_GetParent                      Tcl {Tcl C API}
	SplitPath        Tcl_GetPathType                    Tcl {Tcl C API}
	StringObj        Tcl_GetRange                       Tcl {Tcl C API}
	RegExp           Tcl_GetRegExpFromObj               Tcl {Tcl C API}
	AddErrInfo       Tcl_GetReturnOptions               Tcl {Tcl C API}
	OpenFileChnl     Tcl_Gets                           Tcl {Tcl C API}
	Notifier         Tcl_GetServiceMode                 Tcl {Tcl C API}
	FileSystem       Tcl_GetSizeFromStat                Tcl {Tcl C API}
	IntObj           Tcl_GetSizeIntFromObj              Tcl {Tcl C API}
	OpenFileChnl     Tcl_GetsObj                        Tcl {Tcl C API}
	ChnlStack        Tcl_GetStackedChannel              Tcl {Tcl C API}
	Tcl_Main         Tcl_GetStartupScript               Tcl {Tcl C API}
	GetStdChan       Tcl_GetStdChannel                  Tcl {Tcl C API}
	StringObj        Tcl_GetString                      Tcl {Tcl C API}
	StringObj        Tcl_GetStringFromObj               Tcl {Tcl C API}
	SetResult        Tcl_GetStringResult                Tcl {Tcl C API}
	Thread           Tcl_GetThreadData                  Tcl {Tcl C API}
	GetTime          Tcl_GetTime                        Tcl {Tcl C API}
	ChnlStack        Tcl_GetTopChannel                  Tcl {Tcl C API}
	StringObj        Tcl_GetUniChar                     Tcl {Tcl C API}
	StringObj        Tcl_GetUnicode                     Tcl {Tcl C API}
	StringObj        Tcl_GetUnicodeFromObj              Tcl {Tcl C API}
	FileSystem       Tcl_GetUserIdFromStat              Tcl {Tcl C API}
	SetVar           Tcl_GetVar                         Tcl {Tcl C API}
	SetVar           Tcl_GetVar2                        Tcl {Tcl C API}
	SetVar           Tcl_GetVar2Ex                      Tcl {Tcl C API}
	GetVersion       Tcl_GetVersion                     Tcl {Tcl C API}
	IntObj           Tcl_GetWideIntFromObj              Tcl {Tcl C API}
	IntObj           Tcl_GetWideUIntFromObj             Tcl {Tcl C API}
	Eval             Tcl_GlobalEval                     Tcl {Tcl C API}
	Eval             Tcl_GlobalEvalObj                  Tcl {Tcl C API}
	Hash             Tcl_HashStats                      Tcl {Tcl C API}
	ObjectType       Tcl_HasStringRep                   Tcl {Tcl C API}
	CrtAlias         Tcl_HideCommand                    Tcl {Tcl C API}
	Namespace        Tcl_Import                         Tcl {Tcl C API}
	Object           Tcl_IncrRefCount                   Tcl {Tcl C API}
	Init             Tcl_Init                           Tcl {Tcl C API}
	Hash             Tcl_InitCustomHashTable            Tcl {Tcl C API}
	Hash             Tcl_InitHashTable                  Tcl {Tcl C API}
	DumpActiveMemory Tcl_InitMemory                     Tcl {Tcl C API}
	Notifier         Tcl_InitNotifier                   Tcl {Tcl C API}
	Hash             Tcl_InitObjHashTable               Tcl {Tcl C API}
	ObjectType       Tcl_InitStringRep                  Tcl {Tcl C API}
	InitStubs        Tcl_InitStubs                      Tcl {Tcl C API}
	InitSubSyst      Tcl_InitSubsystems                 Tcl {Tcl C API}
	OpenFileChnl     Tcl_InputBlocked                   Tcl {Tcl C API}
	OpenFileChnl     Tcl_InputBuffered                  Tcl {Tcl C API}
	CrtInterp        Tcl_InterpActive                   Tcl {Tcl C API}
	CrtInterp        Tcl_InterpDeleted                  Tcl {Tcl C API}
	Object           Tcl_InvalidateStringRep            Tcl {Tcl C API}
	CrtChannel       Tcl_IsChannelExisting              Tcl {Tcl C API}
	CrtChannel       Tcl_IsChannelRegistered            Tcl {Tcl C API}
	CrtChannel       Tcl_IsChannelShared                Tcl {Tcl C API}
	StringObj        Tcl_IsEmpty                        Tcl {Tcl C API}
	Ensemble         Tcl_IsEnsemble                     Tcl {Tcl C API}
	CrtAlias         Tcl_IsSafe                         Tcl {Tcl C API}
	Object           Tcl_IsShared                       Tcl {Tcl C API}
	OpenFileChnl     Tcl_IsStandardChannel              Tcl {Tcl C API}
	SplitPath        Tcl_JoinPath                       Tcl {Tcl C API}
	Thread           Tcl_JoinThread                     Tcl {Tcl C API}
	Limit            Tcl_LimitAddHandler                Tcl {Tcl C API}
	Limit            Tcl_LimitCheck                     Tcl {Tcl C API}
	Limit            Tcl_LimitExceeded                  Tcl {Tcl C API}
	Limit            Tcl_LimitGetCommands               Tcl {Tcl C API}
	Limit            Tcl_LimitGetGranularity            Tcl {Tcl C API}
	Limit            Tcl_LimitGetTime                   Tcl {Tcl C API}
	Limit            Tcl_LimitReady                     Tcl {Tcl C API}
	Limit            Tcl_LimitRemoveHandler             Tcl {Tcl C API}
	Limit            Tcl_LimitSetCommands               Tcl {Tcl C API}
	Limit            Tcl_LimitSetGranularity            Tcl {Tcl C API}
	Limit            Tcl_LimitSetTime                   Tcl {Tcl C API}
	Limit            Tcl_LimitTypeEnabled               Tcl {Tcl C API}
	Limit            Tcl_LimitTypeExceeded              Tcl {Tcl C API}
	Limit            Tcl_LimitTypeReset                 Tcl {Tcl C API}
	Limit            Tcl_LimitTypeSet                   Tcl {Tcl C API}
	LinkVar          Tcl_LinkArray                      Tcl {Tcl C API}
	LinkVar          Tcl_LinkVar                        Tcl {Tcl C API}
	ListObj          Tcl_ListObjAppendElement           Tcl {Tcl C API}
	ListObj          Tcl_ListObjAppendList              Tcl {Tcl C API}
	ListObj          Tcl_ListObjGetElements             Tcl {Tcl C API}
	ListObj          Tcl_ListObjIndex                   Tcl {Tcl C API}
	ListObj          Tcl_ListObjLength                  Tcl {Tcl C API}
	ListObj          Tcl_ListObjRange                   Tcl {Tcl C API}
	ListObj          Tcl_ListObjRepeat                  Tcl {Tcl C API}
	ListObj          Tcl_ListObjReplace                 Tcl {Tcl C API}
	ListObj          Tcl_ListObjReverse                 Tcl {Tcl C API}
	Load             Tcl_LoadFile                       Tcl {Tcl C API}
	AddErrInfo       Tcl_LogCommandInfo                 Tcl {Tcl C API}
	Tcl_Main         Tcl_Main                           Tcl {Tcl C API}
	Tcl_Main         Tcl_MainEx                         Tcl {Tcl C API}
	Tcl_Main         Tcl_MainExW                        Tcl {Tcl C API}
	OpenFileChnl     Tcl_MakeFileChannel                Tcl {Tcl C API}
	OpenTcp          Tcl_MakeTcpClientChannel           Tcl {Tcl C API}
	TCL_MEM_DEBUG    TCL_MEM_DEBUG                      Tcl {Tcl C API}
	SplitList        Tcl_Merge                          Tcl {Tcl C API}
	Method           Tcl_MethodDeclarerClass            Tcl {TclOO C API}         
	Method           Tcl_MethodDeclarerObject           Tcl {TclOO C API}         
	Method           Tcl_MethodIsPrivate                Tcl {TclOO C API}         
	Method           Tcl_MethodIsPublic                 Tcl {TclOO C API}         
	Method           Tcl_MethodIsType                   Tcl {TclOO C API}         
	Method           Tcl_MethodIsType2                  Tcl {TclOO C API}         
	Method           Tcl_MethodName                     Tcl {TclOO C API}         
	Thread           Tcl_MutexFinalize                  Tcl {Tcl C API}
	Thread           Tcl_MutexLock                      Tcl {Tcl C API}
	Thread           Tcl_MutexUnlock                    Tcl {Tcl C API}
	IntObj           Tcl_NewBignumObj                   Tcl {Tcl C API}
	BoolObj          Tcl_NewBooleanObj                  Tcl {Tcl C API}
	ByteArrObj       Tcl_NewByteArrayObj                Tcl {Tcl C API}
	DictObj          Tcl_NewDictObj                     Tcl {Tcl C API}
	DoubleObj        Tcl_NewDoubleObj                   Tcl {Tcl C API}
	Method           Tcl_NewInstanceMethod              Tcl {TclOO C API}
	Method           Tcl_NewInstanceMethod2             Tcl {TclOO C API}
	IntObj           Tcl_NewIntObj                      Tcl {Tcl C API}
	ListObj          Tcl_NewListObj                     Tcl {Tcl C API}
	IntObj           Tcl_NewLongObj                     Tcl {Tcl C API}
	Method           Tcl_NewMethod                      Tcl {TclOO C API}
	Method           Tcl_NewMethod2                     Tcl {TclOO C API}
	Object           Tcl_NewObj                         Tcl {Tcl C API}
	Class            Tcl_NewObjectInstance              Tcl {TclOO C API}
	StringObj        Tcl_NewStringObj                   Tcl {Tcl C API}
	StringObj        Tcl_NewUnicodeObj                  Tcl {Tcl C API}
	IntObj           Tcl_NewWideIntObj                  Tcl {Tcl C API}
	IntObj           Tcl_NewWideUIntObj                 Tcl {Tcl C API}
	Hash             Tcl_NextHashEntry                  Tcl {Tcl C API}
	CrtChannel       Tcl_NotifyChannel                  Tcl {Tcl C API}
	NRE              Tcl_NRAddCallback                  Tcl {Tcl C API}
	NRE              Tcl_NRCallObjProc                  Tcl {Tcl C API}
	NRE              Tcl_NRCallObjProc2                 Tcl {Tcl C API}
	NRE              Tcl_NRCmdSwap                      Tcl {Tcl C API}
	NRE              Tcl_NRCreateCommand                Tcl {Tcl C API}
	NRE              Tcl_NRCreateCommand2               Tcl {Tcl C API}
	NRE              Tcl_NREvalObj                      Tcl {Tcl C API}
	NRE              Tcl_NREvalObjv                     Tcl {Tcl C API}
	NRE              Tcl_NRExprObj                      Tcl {Tcl C API}
	Utf              Tcl_NumUtfChars                    Tcl {Tcl C API}
	Method           Tcl_ObjectContextInvokeNext        Tcl {TclOO C API}
	Method           Tcl_ObjectContextIsFiltering       Tcl {TclOO C API}
	Method           Tcl_ObjectContextMethod            Tcl {TclOO C API}
	Method           Tcl_ObjectContextObject            Tcl {TclOO C API}
	Method           Tcl_ObjectContextSkippedArgs       Tcl {TclOO C API}
	Class            Tcl_ObjectDeleted                  Tcl {TclOO C API}
	Class            Tcl_ObjectGetMetadata              Tcl {TclOO C API}
	Class            Tcl_ObjectGetMethodNameMapper      Tcl {TclOO C API}
	Class            Tcl_ObjectSetMetadata              Tcl {TclOO C API}
	Class            Tcl_ObjectSetMethodNameMapper      Tcl {TclOO C API}
	SetVar           Tcl_ObjGetVar2                     Tcl {Tcl C API}
	StringObj        Tcl_ObjPrintf                      Tcl {Tcl C API}
	SetVar           Tcl_ObjSetVar2                     Tcl {Tcl C API}
	OOInitStubs      Tcl_OOInitStubs                    Tcl {TclOO C API}
	OpenFileChnl     Tcl_OpenCommandChannel             Tcl {Tcl C API}
	OpenFileChnl     Tcl_OpenFileChannel                Tcl {Tcl C API}
	OpenTcp          Tcl_OpenTcpClient                  Tcl {Tcl C API}
	OpenTcp          Tcl_OpenTcpServer                  Tcl {Tcl C API}
	OpenTcp          Tcl_OpenTcpServerEx                Tcl {Tcl C API}
	OpenFileChnl     Tcl_OutputBuffered                 Tcl {Tcl C API}
	Panic            Tcl_Panic                          Tcl {Tcl C API}
	ParseArgs        Tcl_ParseArgsObjv                  Tcl {Tcl C API}
	ParseCmd         Tcl_ParseBraces                    Tcl {Tcl C API}
	ParseCmd         Tcl_ParseCommand                   Tcl {Tcl C API}
	ParseCmd         Tcl_ParseExpr                      Tcl {Tcl C API}
	ParseCmd         Tcl_ParseQuotedString              Tcl {Tcl C API}
	ParseCmd         Tcl_ParseVar                       Tcl {Tcl C API}
	ParseCmd         Tcl_ParseVarName                   Tcl {Tcl C API}
	PkgRequire       Tcl_PkgPresent                     Tcl {Tcl C API}
	PkgRequire       Tcl_PkgPresentEx                   Tcl {Tcl C API}
	PkgRequire       Tcl_PkgProvide                     Tcl {Tcl C API}
	PkgRequire       Tcl_PkgProvideEx                   Tcl {Tcl C API}
	PkgRequire       Tcl_PkgRequire                     Tcl {Tcl C API}
	PkgRequire       Tcl_PkgRequireEx                   Tcl {Tcl C API}
	PkgRequire       Tcl_PkgRequireProc                 Tcl {Tcl C API}
	AddErrInfo       Tcl_PosixError                     Tcl {Tcl C API}
	Preserve         Tcl_Preserve                       Tcl {Tcl C API}
	PrintDbl         Tcl_PrintDouble                    Tcl {Tcl C API}
	Environment      Tcl_PutEnv                         Tcl {Tcl C API}
	GetTime          Tcl_QueryTimeProc                  Tcl {Tcl C API}
	Notifier         Tcl_QueueEvent                     Tcl {Tcl C API}
	OpenFileChnl     Tcl_Read                           Tcl {Tcl C API}
	OpenFileChnl     Tcl_ReadChars                      Tcl {Tcl C API}
	OpenFileChnl     Tcl_ReadRaw                        Tcl {Tcl C API}
	Alloc            Tcl_Realloc                        Tcl {Tcl C API}
	DetachPids       Tcl_ReapDetachedProcs              Tcl {Tcl C API}
	RecordEval       Tcl_RecordAndEval                  Tcl {Tcl C API}
	RecEvalObj       Tcl_RecordAndEvalObj               Tcl {Tcl C API}
	RegExp           Tcl_RegExpCompile                  Tcl {Tcl C API}
	RegExp           Tcl_RegExpExec                     Tcl {Tcl C API}
	RegExp           Tcl_RegExpExecObj                  Tcl {Tcl C API}
	RegExp           Tcl_RegExpGetInfo                  Tcl {Tcl C API}
	RegExp           Tcl_RegExpMatch                    Tcl {Tcl C API}
	RegExp           Tcl_RegExpMatchObj                 Tcl {Tcl C API}
	RegExp           Tcl_RegExpRange                    Tcl {Tcl C API}
	OpenFileChnl     Tcl_RegisterChannel                Tcl {Tcl C API}
	RegConfig        Tcl_RegisterConfig                 Tcl {Tcl C API}
	ObjectType       Tcl_RegisterObjType                Tcl {Tcl C API}
	Preserve         Tcl_Release                        Tcl {Tcl C API}
	SetResult        Tcl_ResetResult                    Tcl {Tcl C API}
	SaveInterpState  Tcl_RestoreInterpState             Tcl {Tcl C API}
	SaveInterpState  Tcl_SaveInterpState                Tcl {Tcl C API}
	SplitList        Tcl_ScanCountedElement             Tcl {Tcl C API}
	SplitList        Tcl_ScanElement                    Tcl {Tcl C API}
	OpenFileChnl     Tcl_Seek                           Tcl {Tcl C API}
	Notifier         Tcl_ServiceAll                     Tcl {Tcl C API}
	Notifier         Tcl_ServiceEvent                   Tcl {Tcl C API}
	Notifier         Tcl_ServiceModeHook                Tcl {Tcl C API}
	AssocData        Tcl_SetAssocData                   Tcl {Tcl C API}
	IntObj           Tcl_SetBignumObj                   Tcl {Tcl C API}
	BoolObj          Tcl_SetBooleanObj                  Tcl {Tcl C API}
	ByteArrObj       Tcl_SetByteArrayLength             Tcl {Tcl C API}
	ByteArrObj       Tcl_SetByteArrayObj                Tcl {Tcl C API}
	CrtChannel       Tcl_SetChannelBufferSize           Tcl {Tcl C API}
	SetChanErr       Tcl_SetChannelError                Tcl {Tcl C API}
	SetChanErr       Tcl_SetChannelErrorInterp          Tcl {Tcl C API}
	OpenFileChnl     Tcl_SetChannelOption               Tcl {Tcl C API}
	CrtObjCmd        Tcl_SetCommandInfo                 Tcl {Tcl C API}
	CrtObjCmd        Tcl_SetCommandInfoFromToken        Tcl {Tcl C API}
	DoubleObj        Tcl_SetDoubleObj                   Tcl {Tcl C API}
	Encoding         Tcl_SetEncodingSearchPath          Tcl {Tcl C API}
	Ensemble         Tcl_SetEnsembleFlags               Tcl {Tcl C API}
	Ensemble         Tcl_SetEnsembleMappingDict         Tcl {Tcl C API}
	Ensemble         Tcl_SetEnsembleParameterList       Tcl {Tcl C API}
	Ensemble         Tcl_SetEnsembleSubcommandList      Tcl {Tcl C API}
	Ensemble         Tcl_SetEnsembleUnknownHandler      Tcl {Tcl C API}
	SetErrno         Tcl_SetErrno                       Tcl {Tcl C API}
	AddErrInfo       Tcl_SetErrorCode                   Tcl {Tcl C API}
	AddErrInfo       Tcl_SetErrorLine                   Tcl {Tcl C API}
	Exit             Tcl_SetExitProc                    Tcl {Tcl C API}
	Hash             Tcl_SetHashValue                   Tcl {Tcl C API}
	IntObj           Tcl_SetIntObj                      Tcl {Tcl C API}
	ListObj          Tcl_SetListObj                     Tcl {Tcl C API}
	IntObj           Tcl_SetLongObj                     Tcl {Tcl C API}
	Tcl_Main         Tcl_SetMainLoop                    Tcl {Tcl C API}
	Notifier         Tcl_SetMaxBlockTime                Tcl {Tcl C API}
	Namespace        Tcl_SetNamespaceUnknownHandler     Tcl {Tcl C API}
	Notifier         Tcl_SetNotifier                    Tcl {Tcl C API}
	AddErrInfo       Tcl_SetObjErrorCode                Tcl {Tcl C API}
	StringObj        Tcl_SetObjLength                   Tcl {Tcl C API}
	SetResult        Tcl_SetObjResult                   Tcl {Tcl C API}
	Panic            Tcl_SetPanicProc                   Tcl {Tcl C API}
	SetRecLmt        Tcl_SetRecursionLimit              Tcl {Tcl C API}
	SetResult        Tcl_SetResult                      Tcl {Tcl C API}
	AddErrInfo       Tcl_SetReturnOptions               Tcl {Tcl C API}
	Notifier         Tcl_SetServiceMode                 Tcl {Tcl C API}
	Tcl_Main         Tcl_SetStartupScript               Tcl {Tcl C API}
	GetStdChan       Tcl_SetStdChannel                  Tcl {Tcl C API}
	StringObj        Tcl_SetStringObj                   Tcl {Tcl C API}
	Encoding         Tcl_SetSystemEncoding              Tcl {Tcl C API}
	GetTime          Tcl_SetTimeProc                    Tcl {Tcl C API}
	Notifier         Tcl_SetTimer                       Tcl {Tcl C API}
	StringObj        Tcl_SetUnicodeObj                  Tcl {Tcl C API}
	SetVar           Tcl_SetVar                         Tcl {Tcl C API}
	SetVar           Tcl_SetVar2                        Tcl {Tcl C API}
	SetVar           Tcl_SetVar2Ex                      Tcl {Tcl C API}
	IntObj           Tcl_SetWideIntObj                  Tcl {Tcl C API}
	IntObj           Tcl_SetWideUIntObj                 Tcl {Tcl C API}
	Signal           Tcl_SignalId                       Tcl {Tcl C API}
	Signal           Tcl_SignalMsg                      Tcl {Tcl C API}
	Sleep            Tcl_Sleep                          Tcl {Tcl C API}
	SourceRCFile     Tcl_SourceRCFile                   Tcl {Tcl C API}
	CrtChannel       Tcl_SpliceChannel                  Tcl {Tcl C API}
	SplitList        Tcl_SplitList                      Tcl {Tcl C API}
	SplitPath        Tcl_SplitPath                      Tcl {Tcl C API}
	ChnlStack        Tcl_StackChannel                   Tcl {Tcl C API}
	StdChannels      Tcl_StandardChannels               Tcl {Tcl C API}
	Access           Tcl_Stat                           Tcl {Tcl C API}
	StaticLibrary    Tcl_StaticLibrary                  Tcl {Tcl C API}
	StaticLibrary    Tcl_StaticPackage                  Tcl {Tcl C API}
	ObjectType       Tcl_StoreInternalRep               Tcl {Tcl C API}
	StrMatch         Tcl_StringCaseMatch                Tcl {Tcl C API}
	StrMatch         Tcl_StringMatch                    Tcl {Tcl C API}
	SubstObj         Tcl_SubstObj                       Tcl {Tcl C API}
	IntObj           Tcl_TakeBignumFromObj              Tcl {Tcl C API}
	OpenFileChnl     Tcl_Tell                           Tcl {Tcl C API}
	Notifier         Tcl_ThreadAlert                    Tcl {Tcl C API}
	Notifier         Tcl_ThreadQueueEvent               Tcl {Tcl C API}
	TraceCmd         Tcl_TraceCommand                   Tcl {Tcl C API}
	TraceVar         Tcl_TraceVar                       Tcl {Tcl C API}
	TraceVar         Tcl_TraceVar2                      Tcl {Tcl C API}
	SetResult        Tcl_TransferResult                 Tcl {Tcl C API}
	Translate        Tcl_TranslateFileName              Tcl {Tcl C API}
	OpenFileChnl     Tcl_TruncateChannel                Tcl {Tcl C API}
	OpenFileChnl     Tcl_Ungets                         Tcl {Tcl C API}
	Utf              Tcl_UniChar                        Tcl {Tcl C API}
	Utf              Tcl_UniCharAtIndex                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsAlnum                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsAlpha                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsControl               Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsDigit                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsGraph                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsLower                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsPrint                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsPunct                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsSpace                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsUpper                 Tcl {Tcl C API}
	UniCharIsAlpha   Tcl_UniCharIsWordChar              Tcl {Tcl C API}
	Utf              Tcl_UniCharLen                     Tcl {Tcl C API}
	ToUpper          Tcl_UniCharToLower                 Tcl {Tcl C API}
	ToUpper          Tcl_UniCharToTitle                 Tcl {Tcl C API}
	ToUpper          Tcl_UniCharToUpper                 Tcl {Tcl C API}
	Utf              Tcl_UniCharToUtf                   Tcl {Tcl C API}
	Utf              Tcl_UniCharToUtfDString            Tcl {Tcl C API}
	LinkVar          Tcl_UnlinkVar                      Tcl {Tcl C API}
	OpenFileChnl     Tcl_UnregisterChannel              Tcl {Tcl C API}
	SetVar           Tcl_UnsetVar                       Tcl {Tcl C API}
	SetVar           Tcl_UnsetVar2                      Tcl {Tcl C API}
	ChnlStack        Tcl_UnstackChannel                 Tcl {Tcl C API}
	TraceCmd         Tcl_UntraceCommand                 Tcl {Tcl C API}
	TraceVar         Tcl_UntraceVar                     Tcl {Tcl C API}
	TraceVar         Tcl_UntraceVar2                    Tcl {Tcl C API}
	LinkVar          Tcl_UpdateLinkedVar                Tcl {Tcl C API}
	UpVar            Tcl_UpVar                          Tcl {Tcl C API}
	UpVar            Tcl_UpVar2                         Tcl {Tcl C API}
	Utf              Tcl_UtfAtIndex                     Tcl {Tcl C API}
	Utf              Tcl_UtfBackslash                   Tcl {Tcl C API}
	Utf              Tcl_UtfCharComplete                Tcl {Tcl C API}
	Utf              Tcl_UtfFindFirst                   Tcl {Tcl C API}
	Utf              Tcl_UtfFindLast                    Tcl {Tcl C API}
	Utf              Tcl_UtfNcasecmp                    Tcl {Tcl C API}
	Utf              Tcl_UtfNcmp                        Tcl {Tcl C API}
	Utf              Tcl_UtfNext                        Tcl {Tcl C API}
	Utf              Tcl_UtfPrev                        Tcl {Tcl C API}
	Utf              Tcl_UtfToChar16                    Tcl {Tcl C API}
	Utf              Tcl_UtfToChar16DString             Tcl {Tcl C API}
	Encoding         Tcl_UtfToExternal                  Tcl {Tcl C API}
	Encoding         Tcl_UtfToExternalDString           Tcl {Tcl C API}
	Encoding         Tcl_UtfToExternalDStringEx         Tcl {Tcl C API}
	Encoding         Tcl_UtfToExternalEx                Tcl {Tcl C API}
	ToUpper          Tcl_UtfToLower                     Tcl {Tcl C API}
	UnicodeNormalize Tcl_UtfToNormalized                Tcl {Tcl C API}
	UnicodeNormalize Tcl_UtfToNormalizedDString         Tcl {Tcl C API}
	ToUpper          Tcl_UtfToTitle                     Tcl {Tcl C API}
	Utf              Tcl_UtfToUniChar                   Tcl {Tcl C API}
	Utf              Tcl_UtfToUniCharDString            Tcl {Tcl C API}
	ToUpper          Tcl_UtfToUpper                     Tcl {Tcl C API}
	Utf              Tcl_UtfToWChar                     Tcl {Tcl C API}
	Utf              Tcl_UtfToWCharDString              Tcl {Tcl C API}
	DumpActiveMemory Tcl_ValidateAllMemory              Tcl {Tcl C API}
	Eval             Tcl_VarEval                        Tcl {Tcl C API}
	TraceVar         Tcl_VarTraceInfo                   Tcl {Tcl C API}
	TraceVar         Tcl_VarTraceInfo2                  Tcl {Tcl C API}
	Notifier         Tcl_WaitForEvent                   Tcl {Tcl C API}
	DetachPids       Tcl_WaitPid                        Tcl {Tcl C API}
	Utf              Tcl_WCharLen                       Tcl {Tcl C API}
	Utf              Tcl_WCharToUtfDString              Tcl {Tcl C API}
	SetErrno         Tcl_WinConvertError                Tcl {Tcl C API}
	OpenFileChnl     Tcl_Write                          Tcl {Tcl C API}
	OpenFileChnl     Tcl_WriteChars                     Tcl {Tcl C API}
	OpenFileChnl     Tcl_WriteObj                       Tcl {Tcl C API}
	OpenFileChnl     Tcl_WriteRaw                       Tcl {Tcl C API}
	WrongNumArgs     Tcl_WrongNumArgs                   Tcl {Tcl C API}
	TclZlib          Tcl_ZlibAdler32                    Tcl {Tcl C API}
	TclZlib          Tcl_ZlibCRC32                      Tcl {Tcl C API}
	TclZlib          Tcl_ZlibDeflate                    Tcl {Tcl C API}
	TclZlib          Tcl_ZlibInflate                    Tcl {Tcl C API}
	TclZlib          Tcl_ZlibStreamChecksum             Tcl {Tcl C API}
	TclZlib          Tcl_ZlibStreamClose                Tcl {Tcl C API}
	TclZlib          Tcl_ZlibStreamEof                  Tcl {Tcl C API}
	TclZlib          Tcl_ZlibStreamGet                  Tcl {Tcl C API}
	TclZlib          Tcl_ZlibStreamGetCommandName       Tcl {Tcl C API}
	TclZlib          Tcl_ZlibStreamInit                 Tcl {Tcl C API}
	TclZlib          Tcl_ZlibStreamPut                  Tcl {Tcl C API}
	Zipfs            TclZipfs_AppHook                   Tcl {Tcl C API}
	Zipfs            TclZipfs_Mount                     Tcl {Tcl C API}
	Zipfs            TclZipfs_MountBuffer               Tcl {Tcl C API}
	Zipfs            TclZipfs_Unmount                   Tcl {Tcl C API}
} {
	# code here to build the left panel of the webpage for navigation:
	# a page may appear several times (e.g. one entry per C API function documented on it)
	dict set manFiles $group2/$title [list file $file title $title group1 $group1 group2 $group2]
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
		{Tcl C API} - {TclOO C API} {set myFolder TclCAPI}
		{Tk C API}  {set myFolder TkCAPI}
		default {
			if {[string match Tcl* $g]} {set myFolder Tcl}
			if {[string match Tk* $g]}  {set myFolder Tk}
		}
	}
	if {$myFolder eq ""} {return -code error "no folder for file '$myFile'.md"}
	puts "$myFile.md -> $myFile.html"
	exec pandoc -f markdown-tex_math_dollars-smart -t html \
		--lua-filter markdown2html.lua \
		-s -c [file join .. tcl-docs.css] \
		-o [file join .. doc html $myFolder $myFile.html] \
		[file join .. doc markdown $myFolder $myFile.md]
}
