---
CommandName: object
ManualSection: n
Version: 0.1
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - my(n)
 - oo::class(n)
Keywords:
 - base class
 - class
 - object
 - root class
Copyright:
 - Copyright (c) 2007-2008 Donal K. Fellows
---

# Name

oo::object - root class of the class hierarchy

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[oo::object]{.cmd} [method]{.arg} [arg]{.optdot}
:::

# Class hierarchy

**oo::object**

# Description

The \fBoo::object\fR class is the root class of the object hierarchy; every object is an instance of this class. Since classes are themselves objects, they are instances of this class too. Objects are always referred to by their name, and may be \fBrename\fRd while maintaining their identity.

Instances of objects may be made with either the \fBcreate\fR or \fBnew\fR methods of the \fBoo::object\fR object itself, or by invoking those methods on any of the subclass objects; see \fBoo::class\fR for more details. The configuration of individual objects (i.e., instance-specific methods, mixed-in classes, etc.) may be controlled with the \fBoo::objdefine\fR command.

Each object has a unique namespace associated with it, the instance namespace. This namespace holds all the instance variables of the object, and will be the current namespace whenever a method of the object is invoked (including a method of the class of the object). When the object is destroyed, its instance namespace is deleted. The instance namespace contains the object's \fBmy\fR command, which may be used to invoke non-exported methods of the object or to create a reference to the object for the purpose of invocation which persists across renamings of the object.

## Constructor

The \fBoo::object\fR class does not define an explicit constructor.

## Destructor

The \fBoo::object\fR class does not define an explicit destructor.

## Exported methods

The \fBoo::object\fR class supports the following exported methods:

*obj* \fBdestroy\fR
: This method destroys the object, \fIobj\fR, that it is invoked upon, invoking any destructors on the object's class in the process. It is equivalent to using \fBrename\fR to delete the object command. The result of this method is always the empty string.


## Non-exported methods

The \fBoo::object\fR class supports the following non-exported methods:

*obj* \fBeval\fR ?\fIarg ...\fR?
: This method concatenates the arguments, \fIarg\fR, as if with \fBconcat\fR, and then evaluates the resulting script in the namespace that is uniquely associated with \fIobj\fR, returning the result of the evaluation.
    Note that object-internal commands such as \fBmy\fR and \fBself\fR can be invoked in this context.

*obj* \fBunknown\fR ?\fImethodName\fR? ?\fIarg ...\fR?
: This method is called when an attempt to invoke the method \fImethodName\fR on object \fIobj\fR fails. The arguments that the user supplied to the method are given as \fIarg\fR arguments. If \fImethodName\fR is absent, the object was invoked with no method name at all (or any other arguments). The default implementation (i.e., the one defined by the \fBoo::object\fR class) generates a suitable error, detailing what methods the object supports given whether the object was invoked by its public name or through the \fBmy\fR command.

*obj* \fBvariable\fR ?\fIvarName ...\fR?
: This method arranges for each variable called \fIvarName\fR to be linked from the object \fIobj\fR's unique namespace into the caller's context. Thus, if it is invoked from inside a procedure then the namespace variable in the object is linked to the local variable in the procedure. Each \fIvarName\fR argument must not have any namespace separators in it. The result is the empty string.

*obj* \fBvarname\fR \fIvarName\fR
: This method returns the globally qualified name of the variable \fIvarName\fR in the unique namespace for the object \fIobj\fR.

*obj* \fB<cloned>\fR \fIsourceObjectName\fR
: This method is used by the \fBoo::object\fR command to copy the state of one object to another. It is responsible for copying the procedures and variables of the namespace of the source object (\fIsourceObjectName\fR) to the current object. It does not copy any other types of commands or any traces on the variables; that can be added if desired by overriding this method in a subclass.


# Examples

This example demonstrates basic use of an object.

```
set obj [oo::object new]
$obj foo             \(-> error "unknown method foo"
oo::objdefine $obj method foo {} {
    my variable count
    puts "bar[incr count]"
}
$obj foo             \(-> prints "bar1"
$obj foo             \(-> prints "bar2"
$obj variable count  \(-> error "unknown method variable"
$obj destroy
$obj foo             \(-> error "unknown command obj"
```

