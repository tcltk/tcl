// NaTcl -- JS glue
var tclModule = null;  // our singleton Tcl interp

function printf()
{
    // I like debugging to stderr instead of console.log,
    // because when things go wrong, the JS console is not
    // alays reachable.
    tclModule.evall('printf', arguments.join(' '));
}

function ljoin()
{
    // yeah. 'arguments' is 'similar to an array but not quite'
    // that's why I think JS has no soul.

    return Array.prototype.slice.call(arguments).join("\n");
}

// --- tclDo is the main JS-Tcl trampoline.
//
//  Its job is to pass a Tcl string to [eval] (through naclwrap, see
//  init.nacl), and then take back the result as JS and eval() it.
//  It also detects errors in the latter eval() and pipes them back
//  to [bgerror].

function tclEsc(text) {
  return text.replace(/[][\\$""]/g,'\\$0');
}

function tclDo(s) {
    try {
        //printf("do:"+s);
	t = tclModule.eval("::nacl::wrap {" + s + "}");
	//printf("ret:"+t);
	eval(t);
    } catch(err) {
	//printf("JS-err:"+err);
	setTimeout('tcl("::nacl::bgerror,"'+ err + ',' + t + ')',0);
    }
}

function tcl() {
    var t;
    try {
        //printf.apply(this, arguments);
        t = tclModule.evall.apply(tclModule, arguments);
        //printf("ret:", t);
    } catch (err) {
	//printf("JS-err:", err);
	setTimeout('tcl("::nacl::bgerror,"'+ err + ',' + t + ')',0);
    }

    try {
	eval(t);
    } catch(err) {
	//printf("JS-err:", err);
	setTimeout('tcl("::nacl::bgerror,"'+ err + ',' + t + ')',0);
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
    //printf('tclsource');
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
            script = script + '\n' + scripts[i].innerHTML;
        }
    }
    tclDo(where + " {" + tclEsc(script)+ "}");
}
