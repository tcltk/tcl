// NaTcl -- JS glue
var tclModule = null;  // our singleton Tcl interp

function printf()
{
    // I like debugging to stderr instead of console.log,
    // because when things go wrong, the JS console is not
    // alays reachable.
    tclModule.postMessage('eval:printf '+Array.prototype.slice.call(arguments).join(' '));
}

// --- tclDo is the main JS-Tcl trampoline.
//
//  Its job is to pass a Tcl string to [eval] (through naclwrap, see
//  init.nacl), and then take back the result as JS and eval() it.
//  It also detects errors in the latter eval() and pipes them back
//  to [bgerror].

function tclEsc(text) {
    return text.replace(/([\[\]\\$""{}])/g,'\\$1');
}

function tclDo(s) {
    //console.log("TclDo: "+s);
    tclModule.postMessage("eval:::nacl::wrap {" + s + "}");
}
function tclDoCoro(s) {
    tclModule.postMessage("eval:coroutine ::main_coro ::nacl::wrap \"" + tclEsc(s) + "\"");
}
function tcl()
{
	tclDo(Array.prototype.slice.call(arguments).join(" "));
}

// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
    try {
		
		t = message_event.data;
		//console.log("JSdo:"+t);
		window.eval(t);
    } catch(err) {
		//printf("JS-err:"+err);
		alert("ERROR:"+err);
    }
}

function serialT (thing) {
    alert("serialT " + thing.type + " " + thing.toString());
    var result = '{';
    switch (thing.type) {

    case 'string':
        return tclEsc(thing);
        break;

    case 'array':
        for (var i=0; i < thing.length(); i++) {
            result = result + " " + serialT(thing[i]);
        }
        result = result + '}';
        break;

    case 'object':
        for (var prop in thing) {
            result = result + " " + serialT(prop) + " " + serialT(thing[prop]);
        }
        result = result + '}';
        break;

    case 'function':
        result = thing.toString();
        break;

    default:
        return undefined;
        break;
    }
    return result;
}

function callback(to,js) {
    alert("callback " + to + " " + js);
    try {
	tclDo(to + " " + serialT(eval(js)));
    } catch (err) {
        alert("JS-error " + err);
	//printf("JS-err:", err);
	setTimeout('tcl("::nacl::bgerror,"'+ err + ',' + js + ')',0);
    }
}

// --- tclsource starts an XHR, and calls the given 'tcb' (Tcl
// --- Callback) on completion. A catchable Tcl-level error is raised
// --- in case of not-200. Used by [source].
function tclsource(url,tcb) {
    //printf('tclsource:'+url);
    xs = new XMLHttpRequest();
    xs.open("GET",url,true);
    xs.send(null);
    xs.onreadystatechange = function() {
	//printf("XHR-source:"+xs.readyState);
	if (xs.readyState==4)
        {
            if (xs.status==200) {
                tclDo(tcb+" {"+xs.responseText+"}");
            } else {
                tclDo(tcb+" {error \"Can't source -- "+xs.statusText+"\"}");
            }
        }
    };
}

// traverse - apply function to subtree of dom returning object
function traverse(fn)
{
    var result;
    var target;
    if (arguments.length() > 1) {
        target = arguments[1];
    } else {
        target = document;
    }

    result[target]=fn.apply(target);

    for (var i=0; i<target.childNodes.length; i++) {
        var child = target.childNodes[i];
        var traversal = traverse(fn, child);
        for (var prop in traversal) {
            result[prop] = traversal[prop];
        }
    }

    return result;
}

// ---------- GUI and standard NaCl-loading machinery --------

function moduleDidLoad() {
    tclModule = document.getElementById('tcl');
	tclModule.addEventListener('message', handleMessage, false);
    // tcl('lappend', '::JS', "alert('ARGV:[join $::argv]')");
    tcl('eval', 'coroutine', '::main_coro', 'source', '[dict get $::argv source]');
}

// runTclScripts - collect all <script> elements with type text/tcl
// Pass them to the Tcl header given in 'where' (or uplevel)
function runTclScripts(where) {
    if (where == undefined) {
        where = "uplevel #0";
    }

    var scripts = document.getElementsByTagName('script');
    var script = "";
    for (var i = 0; i < scripts.length; i++) {
        if (scripts[i].getAttribute('type') == 'text/tcl') {
            if (scripts[i].hasAttribute('src')) {
                script = script + '\nsource ' + scripts[i].getAttribute('src');
            }
            var s = scripts[i].innerHTML;
            if (s != "") {
                script = script + '\n' + s;
            }
        }
    }
    tclDo(where + " {" + tclEsc(script)+ "}");
}
