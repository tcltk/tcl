---
CommandName: singleton
ManualSection: n
Version: 0.3
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - oo::class(n)
Keywords:
 - class
 - metaclass
 - object
 - single instance
Copyright:
 - Copyright (c) 2018 Donal K. Fellows
---

# Name

oo::singleton - a class that does only allows one instance of itself

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[oo::singleton]{.cmd} [method]{.arg} [arg]{.optdot}
:::

# Class hierarchy

**oo::object**    \(-> **oo::class**        \(-> **oo::singleton**

# Description

Singleton classes are classes that only permit at most one instance of themselves to exist. They unexport the **create** and **createWithNamespace** methods entirely, and override the **new** method so that it only makes a new instance if there is no existing instance.  It is not recommended to inherit from a singleton class; singleton-ness is *not* inherited. It is not recommended that a singleton class's constructor take any arguments.

Instances have their** destroy** method overridden with a method that always returns an error in order to discourage destruction of the object, but destruction remains possible if strictly necessary (e.g., by destroying the class or using **rename** to delete it). They also have a (non-exported) **<cloned>** method defined on them that similarly always returns errors to make attempts to use the singleton instance with **oo::copy** fail.

## Constructor

The **oo::singleton** class does not define an explicit constructor; this means that it is effectively the same as the constructor of the **oo::class** class.

## Destructor

The **oo::singleton** class does not define an explicit destructor; destroying an instance of it is just like destroying an ordinary class (and will destroy the singleton object).

## Exported methods

*cls* **new** ?*arg ...*?
: This returns the current instance of the singleton class, if one exists, and creates a new instance only if there is no existing instance. The additional arguments, *arg ...*, are only used if a new instance is actually manufactured; that construction is via the **oo::class** class's **new** method.
    This is an override of the behaviour of a superclass's method with an identical call signature to the superclass's implementation.


## Non-exported methods

The **oo::singleton** class explicitly states that **create** and **createWithNamespace** are unexported; callers should not assume that they have control over either the name or the namespace name of the singleton instance.

# Example

This example demonstrates that there is only one instance even though the **new** method is called three times.

```
oo::singleton create Highlander {
    method say {} {
        puts "there can be only one"
    }
}

set h1 [Highlander new]
set h2 [Highlander new]
if {$h1 eq $h2} {
    puts "equal objects"    \(-> prints "equal objects"
}
set h3 [Highlander new]
if {$h1 eq $h3} {
    puts "equal objects"    \(-> prints "equal objects"
}
```

Note that the name of the instance of the singleton is not guaranteed to be anything in particular.

