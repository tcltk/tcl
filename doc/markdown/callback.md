---
CommandName: callback
ManualSection: n
Version: 0.3
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - chan(n)
 - fileevent(n)
 - my(n)
 - self(n)
 - socket(n)
 - trace(n)
Keywords:
 - callback
 - object
Copyright:
 - Copyright (c) 2018 Donal K. Fellows
---

# Name

callback, mymethod - generate callbacks to methods

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[callback]{.cmd} [methodName]{.arg} [arg]{.optdot}
[mymethod]{.cmd} [methodName]{.arg} [arg]{.optdot}
:::

# Description

The **callback** command, also called **mymethod** for compatibility with the ooutil and snit packages of Tcllib, and which should only be used from within the context of a call to a method (i.e. inside a method, constructor or destructor body) is used to generate a script fragment that will invoke the method, *methodName*, on the current object (as reported by **self**) when executed. Any additional arguments provided will be provided as leading arguments to the callback. The resulting script fragment shall be a proper list.

Note that it is up to the caller to ensure that the current object is able to handle the call of *methodName*; this command does not check that. *methodName* may refer to any exported or unexported method, but may not refer to a private method as those can only be invoked directly from within methods. If there is no such method present at the point when the callback is invoked, the standard **unknown** method handler will be called.

# Example

This is a simple echo server class. The **callback** command is used in two places, to arrange for the incoming socket connections to be handled by the *Accept* method, and to arrange for the incoming bytes on those connections to be handled by the *Receive* method.

```
oo::class create EchoServer {
    variable server clients
    constructor {port} {
        set server [socket -server [callback Accept] $port]
        set clients {}
    }
    destructor {
        chan close $server
        foreach client [dict keys $clients] {
            chan close $client
        }
    }

    method Accept {channel clientAddress clientPort} {
        dict set clients $channel [dict create \
                address $clientAddress port $clientPort]
        chan event $channel readable [callback Receive $channel]
    }
    method Receive {channel} {
        if {[chan gets $channel line] >= 0} {
            my echo $channel $line
        } else {
            chan close $channel
            dict unset clients $channel
        }
    }

    method echo {channel line} {
        dict with clients $channel {
            chan puts $channel \
                    [format {[%s:%d] %s} $address $port $line]
        }
    }
}
```

