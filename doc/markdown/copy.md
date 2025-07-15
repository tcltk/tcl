---
CommandName: copy
ManualSection: n
Version: 0.1
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - oo::class(n)
 - oo::define(n)
 - oo::object(n)
Keywords:
 - clone
 - copy
 - duplication
 - object
Copyright:
 - Copyright (c) 2007 Donal K. Fellows
---

# Name

oo::copy - create copies of objects and classes

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[oo::copy]{.cmd} [sourceObject]{.arg} [targetObject]{.optarg} [targetNamespace]{.optarg}
:::

# Description

The \fBoo::copy\fR command creates a copy of an object or class. It takes the name of the object or class to be copied, \fIsourceObject\fR, and optionally the name of the object or class to create, \fItargetObject\fR, which will be resolved relative to the current namespace if not an absolute qualified name and \fItargetNamespace\fR which is the name of the namespace that will hold the internal state of the object (\fBmy\fR command, etc.); it \fImust not\fR refer to an existing namespace. If either \fItargetObject\fR or \fItargetNamespace\fR is omitted or is given as the empty string, a new name is chosen. Names, unless specified, are chosen with the same algorithm used by the \fBnew\fR method of \fBoo::class\fR. The copied object will be of the same class as the source object, and will have all its per-object methods copied. If it is a class, it will also have all the class methods in the class copied, but it will not have any of its instances copied.

::: {.info -version=""}
After the \fItargetObject\fR has been created and all definitions of its configuration (e.g., methods, filters, mixins) copied, the \fB<cloned>\fR method of \fItargetObject\fR will be invoked, to allow for customization of the created object such as installing related variable traces. The only argument given will be \fIsourceObject\fR. The default implementation of this method (in \fBoo::object\fR) just copies the procedures and variables in the namespace of \fIsourceObject\fR to the namespace of \fItargetObject\fR. If this method call does not return a result that is successful (i.e., an error or other kind of exception) then the \fItargetObject\fR will be deleted and an error returned.
:::

The result of the \fBoo::copy\fR command will be the fully-qualified name of the new object or class.

# Examples

This example creates an object, copies it, modifies the source object, and then demonstrates that the copied object is indeed a copy.

```
oo::object create src
oo::objdefine src method msg {} {puts foo}
oo::copy src dst
oo::objdefine src method msg {} {puts bar}
src msg              \(-> prints "bar"
dst msg              \(-> prints "foo"
```

