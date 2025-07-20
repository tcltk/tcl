---
CommandName: my
ManualSection: n
Version: 0.1
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - next(n)
 - oo::object(n)
 - self(n)
Keywords:
 - method
 - method visibility
 - object
 - private method
 - public method
Copyright:
 - Copyright (c) 2007 Donal K. Fellows
---

# Name

my, myclass - invoke any method of current object or its class

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[my]{.cmd} [methodName]{.arg} [arg]{.optdot}
[myclass]{.cmd} [methodName]{.arg} [arg]{.optdot}
:::

# Description

The **my** command is used to allow methods of objects to invoke methods of the object (or its class),

::: {.info version="TIP478"}
and the **myclass** command is used to allow methods of objects to invoke methods of the current class of the object *as an object*.
:::

In particular, the set of valid values for *methodName* is the set of all methods supported by an object and its superclasses, including those that are not exported

::: {.info version="TIP500"}
and private methods of the object or class when used within another method defined by that object or class.
:::

The object upon which the method is invoked via **my** is the one that owns the namespace that the **my** command is contained in initially (**NB:** the link remains if the command is renamed), which is the currently invoked object by default.

::: {.info version="TIP478"}
Similarly, the object on which the method is invoked via **myclass** is the object that is the current class of the object that owns the namespace that the **myclass** command is contained in initially. As with **my**, the link remains even if the command is renamed into another namespace, and defaults to being the manufacturing class of the current object.
:::

Each object has its own **my** and **myclass** commands, contained in its instance namespace.

# Examples

This example shows basic use of **my** to use the **variables** method of the **oo::object** class, which is not publicly visible by default:

```
oo::class create c {
    method count {} {
        my variable counter
        puts [incr counter]
    }
}

c create o
o count              \(-> prints "1"
o count              \(-> prints "2"
o count              \(-> prints "3"
```

This example shows how you can use **my** to make callbacks to private methods from outside the object (from a **trace**), using **namespace code** to enter the correct context. (See the **callback** command for the recommended way of doing this.)

```
oo::class create HasCallback {
    method makeCallback {} {
        return [namespace code {
            my Callback
        }]
    }

    method Callback {args} {
        puts "callback: $args"
    }
}

set o [HasCallback new]
trace add variable xyz write [$o makeCallback]
set xyz "called"     \(-> prints "callback: xyz {} write"
```

::: {.info version="TIP478"}
This example shows how to access a private method of a class from an instance of that class. (See the **classmethod** declaration in **oo::define** for a higher level interface for doing this.)
:::

```
oo::class create CountedSteps {
    self {
        variable count
        method Count {} {
            return [incr count]
        }
    }
    method advanceTwice {} {
        puts "in [self] step A: [myclass Count]"
        puts "in [self] step B: [myclass Count]"
    }
}

CountedSteps create x
CountedSteps create y
x advanceTwice       \(-> prints "in ::x step A: 1"
                     \(-> prints "in ::x step B: 2"
y advanceTwice       \(-> prints "in ::y step A: 3"
                     \(-> prints "in ::y step B: 4"
x advanceTwice       \(-> prints "in ::x step A: 5"
                     \(-> prints "in ::x step B: 6"
y advanceTwice       \(-> prints "in ::y step A: 7"
                     \(-> prints "in ::y step B: 8"
```

