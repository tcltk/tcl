---
CommandName: abstract
ManualSection: n
Version: 0.3
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - oo::define(n)
 - oo::object(n)
Keywords:
 - abstract class
 - class
 - metaclass
 - object
Copyright:
 - Copyright (c) 2018 Donal K. Fellows
---

# Name

oo::abstract - a class that does not allow direct instances of itself

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[oo::abstract]{.cmd} [method]{.arg} [arg]{.optdot}
:::

# Class hierarchy

**oo::object**    \(-> \fBoo::class\fR        \(-> \fBoo::abstract\fR

# Description

Abstract classes are classes that can contain definitions, but which cannot be directly manufactured; they are intended to only ever be inherited from and instantiated indirectly. The characteristic methods of \fBoo::class\fR (\fBcreate\fR and \fBnew\fR) are not exported by an instance of \fBoo::abstract\fR.

Note that \fBoo::abstract\fR is not itself an instance of \fBoo::abstract\fR.

## Constructor

The \fBoo::abstract\fR class does not define an explicit constructor; this means that it is effectively the same as the constructor of the \fBoo::class\fR class.

## Destructor

The \fBoo::abstract\fR class does not define an explicit destructor; destroying an instance of it is just like destroying an ordinary class (and will destroy all its subclasses).

## Exported methods

The \fBoo::abstract\fR class defines no new exported methods.

## Non-exported methods

The \fBoo::abstract\fR class explicitly states that \fBcreate\fR, \fBcreateWithNamespace\fR, and \fBnew\fR are unexported.

# Examples

This example defines a simple class hierarchy and creates a new instance of it. It then invokes a method of the object before destroying the hierarchy and showing that the destruction is transitive.

```
oo::abstract create fruit {
    method eat {} {
        puts "yummy!"
    }
}
oo::class create banana {
    superclass fruit
    method peel {} {
        puts "skin now off"
    }
}
set b [banana new]
$b peel              \(-> prints 'skin now off'
$b eat               \(-> prints 'yummy!'
set f [fruit new]    \(-> error 'unknown method "new"...'
```

