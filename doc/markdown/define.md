---
CommandName: define
ManualSection: n
Version: 0.3
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - next(n)
 - oo::class(n)
 - oo::object(n)
Keywords:
 - class
 - definition
 - method
 - object
 - slot
Copyright:
 - Copyright (c) 2007-2018 Donal K. Fellows
---

# Name

oo::define, oo::objdefine, oo::Slot - define and configure classes and objects

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[oo::define]{.cmd} [class]{.arg} [defScript]{.arg}
[oo::define]{.cmd} [class]{.arg} [subcommand]{.arg} [arg]{.arg} [arg]{.optdot}
[oo::objdefine]{.cmd} [object]{.arg} [defScript]{.arg}
[oo::objdefine]{.cmd} [object]{.arg} [subcommand]{.arg} [arg]{.arg} [arg]{.optdot}

[oo::Slot]{.cmd} [arg...]{.arg}
:::

# Class hierarchy

**oo::object**    \(-> \fBoo::Slot\fR

# Description

The \fBoo::define\fR command is used to control the configuration of classes, and the \fBoo::objdefine\fR command is used to control the configuration of objects (including classes as instance objects), with the configuration being applied to the entity named in the \fIclass\fR or the \fIobject\fR argument. Configuring a class also updates the configuration of all subclasses of the class and all objects that are instances of that class or which mix it in (as modified by any per-instance configuration). The way in which the configuration is done is controlled by either the \fIdefScript\fR argument or by the \fIsubcommand\fR and following \fIarg\fR arguments; when the second is present, it is exactly as if all the arguments from \fIsubcommand\fR onwards are made into a list and that list is used as the \fIdefScript\fR argument.

Note that the constructor for \fBoo::class\fR will call \fBoo::define\fR on the script argument that it is provided. This is a convenient way to create and define a class in one step.

# Configuring classes

The following commands are supported in the \fIdefScript\fR for \fBoo::define\fR, each of which may also be used in the \fIsubcommand\fR form:

**classmethod** \fIname\fR ?\fIargList bodyScrip\fR?
: This creates a class method, or (if \fIargList\fR and \fIbodyScript\fR are omitted) promotes an existing method on the class object to be a class method. The \fIname\fR, \fIargList\fR and \fIbodyScript\fR arguments are as in the \fBmethod\fR definition, below.
    Class methods can be called on either the class itself or on the instances of that class. When they are called, the current object (see the \fBsel\fR and \fBmy\fR commands) is the class on which they are called or the class of the instance on which they are called, depending on whether they are called on the class or an instance of the class, respectively. If called on a subclass or instance of the subclass, the current object is the subclass.
    In a private definition context, the methods as invoked on classes are \fInot\fR private, but the methods as invoked on instances of classes are private.

**constructor** \fIargList bodyScript\fR
: This creates or updates the constructor for a class. The formal arguments to the constructor (defined using the same format as for the Tcl \fBproc\fR command) will be \fIargList\fR, and the body of the constructor will be \fIbodyScript\fR. When the body of the constructor is evaluated, the current namespace of the constructor will be a namespace that is unique to the object being constructed. Within the constructor, the \fBnext\fR command should be used to call the superclasses' constructors. If \fIbodyScript\fR is the empty string, the constructor will be deleted.
    Classes do not need to have a constructor defined. If none is specified, the superclass's constructor will be used instead.

**destructor** \fIbodyScript\fR
: This creates or updates the destructor for a class. Destructors take no arguments, and the body of the destructor will be \fIbodyScript\fR. The destructor is called when objects of the class are deleted, and when called will have the object's unique namespace as the current namespace. Destructors should use the \fBnext\fR command to call the superclasses' destructors. Note that destructors are not called in all situations (e.g. if the interpreter is destroyed). If \fIbodyScript\fR is the empty string, the destructor will be deleted. Note that errors during the evaluation of a destructor \fIare not returned\fR to the code that causes the destruction of an object. Instead, they are passed to the currently-defined \fBbgerror\fR handler.

**export** \fIname\fR ?\fIname ...\fR?
: This arranges for each of the named methods, \fIname\fR, to be exported (i.e. usable outside an instance through the instance object's command) by the class being defined. Note that the methods themselves may be actually defined by a superclass; subclass exports override superclass visibility, and may in turn be overridden by instances.

**forward** \fIname cmdName\fR ?\fIarg ...\fR?
: This creates or updates a forwarded method called \fIname\fR. The method is defined be forwarded to the command called \fIcmdName\fR, with additional arguments, \fIarg\fR etc., added before those arguments specified by the caller of the method. The \fIcmdName\fR will always be resolved using the rules of the invoking objects' namespaces, i.e., when \fIcmdName\fR is not fully-qualified, the command will be searched for in each object's namespace, using the instances' namespace's path, or by looking in the global namespace. The method will be exported if \fIname\fR starts with a lower-case letter, and non-exported otherwise.
    If in a private definition context (see the \fBprivate\fR definition command, below), this command creates private forwarded methods.

**initialise** \fIscript\fR
: ...see next...

**initialize** \fIscript\fR
: This evaluates \fIscript\fR in a context which supports local variables and where the current namespace is the instance namespace of the class object itself. This is useful for setting up, e.g., class-scoped variables.

**method** \fIname\fR ?\fIoption\fR? \fIargList bodyScript\fR
: This creates or updates a method that is implemented as a procedure-like script. The name of the method is \fIname\fR, the formal arguments to the method (defined using the same format as for the Tcl \fBproc\fR command) will be \fIargList\fR, and the body of the method will be \fIbodyScript\fR. When the body of the method is evaluated, the current namespace of the method will be a namespace that is unique to the current object. The method will be exported if \fIname\fR starts with a lower-case letter, and non-exported otherwise; this behavior can be overridden via \fBexport\fR and \fBunexport\fR or by specifying \fB-export\fR, \fB-private\fR or \fB-unexport\fR in the optional parameter \fIoption\fR.
    If in a private definition context (see the \fBprivate\fR definition command, below) or if the \fB-private\fR flag is given for \fIoption\fR, this command creates private procedure-like methods.

**private** \fIcmd arg...\fR
: ...see next...

**private** \fIscript\fR
: This evaluates the \fIscript\fR (or the list of command and arguments given by \fIcmd\fR and \fIarg\fRs) in a context where the definitions made on the current class will be private definitions.
    The following class definition commands are affected by \fBprivate\fR: \fBforward\fR, \fBmethod\fR, \fBself\fR, and \fBvariable\fR. Nesting \fBprivate\fR inside \fBprivate\fR has no cumulative effect; the innermost definition context is just a private definition context. All other definition commands have no difference in behavior when used in a private definition context.

**self** \fIsubcommand arg ...\fR
: ...see next...

**self** \fIscript\fR
: ...see next...

**self**
: This command is equivalent to calling \fBoo::objdefine\fR on the class being defined (see \fBCONFIGURING OBJECTS\fR below for a description of the supported values of \fIsubcommand\fR). It follows the same general pattern of argument handling as the \fBoo::define\fR and \fBoo::objdefine\fR commands, and "**oo::define** \fIcls\fR \fBself\fR \fIsubcommand ...\fR" operates identically to "**oo::objdefine** \fIcls subcommand ...\fR".
    If no arguments at all are used, this gives the name of the class currently being configured. If in a private definition context (see the \fBprivate\fR definition command, below), the definitions on the class object will also be made in a private definition context.

**superclass** ?\fI-slotOperation\fR? ?\fIclassName ...\fR?
: This slot (see \fBSLOTTED DEFINITIONS\fR below) allows the alteration of the superclasses of the class being defined. Each \fIclassName\fR argument names one class that is to be a superclass of the defined class. Note that objects must not be changed from being classes to being non-classes or vice-versa, that an empty parent class is equivalent to \fBoo::object\fR, and that the parent classes of \fBoo::object\fR and \fBoo::class\fR may not be modified. By default, this slot works by replacement.

**unexport** \fIname\fR ?\fIname ...\fR?
: This arranges for each of the named methods, \fIname\fR, to be not exported (i.e. not usable outside the instance through the instance object's command, but instead just through the \fBmy\fR command visible in each object's context) by the class being defined. Note that the methods themselves may be actually defined by a superclass; subclass unexports override superclass visibility, and may be overridden by instance unexports.

**variable** ?\fI-slotOperation\fR? ?\fIname ...\fR?
: This slot (see \fBSLOTTED DEFINITIONS\fR below) arranges for each of the named variables to be automatically made available in the methods, constructor and destructor declared by the class being defined. Each variable name must not have any namespace separators and must not look like an array access. All variables will be actually present in the namespace of the instance object on which the method is executed. Note that the variable lists declared by a superclass or subclass are completely disjoint, as are variable lists declared by instances; the list of variable names is just for methods (and constructors and destructors) declared by this class. By default, this slot works by appending.
    If in a private definition context (see the \fBprivate\fR definition command, below), this slot manipulates the list of private variable bindings for this class. In a private variable binding, the name of the variable within the instance object is different to the name given in the definition; the name used in the definition is the name that you use to access the variable within the methods of this class, and the name of the variable in the instance namespace has a unique prefix that makes accidental use from other classes extremely unlikely.


## Advanced class configuration options

The following definitions are also supported, but are not required in simple programs:

**definitionnamespace** ?\fIkind\fR? \fInamespaceName\fR
: This allows control over what namespace will be used by the \fBoo::define\fR and \fBoo::objdefine\fR commands to look up the definition commands they use. When any object has a definition operation applied to it, \fIthe class that it is an instance of\fR (and its superclasses and mixins) is consulted for what definition namespace to use. \fBoo::define\fR gets the class definition namespace, and \fB::oo::objdefine\fR gets the instance definition namespace, but both otherwise use the identical lookup operation.
    This sets the definition namespace of kind \fIkind\fR provided by the current class to \fInamespaceName\fR. The \fInamespaceName\fR must refer to a currently existing namespace, or must be the empty string (to stop the current class from having such a namespace connected). The \fIkind\fR, if supplied, must be either \fB-class\fR (the default) or \fB-instance\fR to specify the whether the namespace for use with \fBoo::define\fR or \fBoo::objdefine\fR respectively is being set.
    The class \fBoo::object\fR has its instance namespace locked to \fB::oo::objdefine\fR, and the class \fBoo::class\fR has its class namespace locked to \fB::oo::define\fR. A consequence of this is that effective use of this feature for classes requires the definition of a metaclass.

**deletemethod** \fIname\fR ?\fIname ...\fR?
: This deletes each of the methods called \fIname\fR from a class. The methods must have previously existed in that class. Does not affect the superclasses of the class, nor does it affect the subclasses or instances of the class (except when they have a call chain through the class being modified) or the class object itself.

**filter** ?\fI-slotOperation\fR? ?\fImethodName ...\fR?
: This slot (see \fBSLOTTED DEFINITIONS\fR below) sets or updates the list of method names that are used to guard whether method call to instances of the class may be called and what the method's results are. Each \fImethodName\fR names a single filtering method (which may be exposed or not exposed); it is not an error for a non-existent method to be named since they may be defined by subclasses. By default, this slot works by appending.

**mixin** ?\fI-slotOperation\fR? ?\fIclassName ...\fR?
: This slot (see \fBSLOTTED DEFINITIONS\fR below) sets or updates the list of additional classes that are to be mixed into all the instances of the class being defined. Each \fIclassName\fR argument names a single class that is to be mixed in. By default, this slot works by replacement.

**renamemethod** \fIfromName toName\fR
: This renames the method called \fIfromName\fR in a class to \fItoName\fR. The method must have previously existed in the class, and \fItoName\fR must not previously refer to a method in that class. Does not affect the superclasses of the class, nor does it affect the subclasses or instances of the class (except when they have a call chain through the class being modified), or the class object itself. Does not change the export status of the method; if it was exported before, it will be afterwards.


# Configuring objects

The following commands are supported in the \fIdefScript\fR for \fBoo::objdefine\fR, each of which may also be used in the \fIsubcommand\fR form:

**export** \fIname\fR ?\fIname ...\fR?
: This arranges for each of the named methods, \fIname\fR, to be exported (i.e. usable outside the object through the object's command) by the object being defined. Note that the methods themselves may be actually defined by a class or superclass; object exports override class visibility.

**forward** \fIname cmdName\fR ?\fIarg ...\fR?
: This creates or updates a forwarded object method called \fIname\fR. The method is defined be forwarded to the command called \fIcmdName\fR, with additional arguments, \fIarg\fR etc., added before those arguments specified by the caller of the method. Forwarded methods should be deleted using the \fBmethod\fR subcommand. The method will be exported if \fIname\fR starts with a lower-case letter, and non-exported otherwise.
    If in a private definition context (see the \fBprivate\fR definition command, below), this command creates private forwarded methods.

**method** \fIname\fR ?\fIoption\fR? \fIargList bodyScript\fR
: This creates, updates or deletes an object method. The name of the method is \fIname\fR, the formal arguments to the method (defined using the same format as for the Tcl \fBproc\fR command) will be \fIargList\fR, and the body of the method will be \fIbodyScript\fR. When the body of the method is evaluated, the current namespace of the method will be a namespace that is unique to the object. The method will be exported if \fIname\fR starts with a lower-case letter, and non-exported otherwise; this can be overridden by specifying \fB-export\fR, \fB-private\fR or \fB-unexport\fR in the optional parameter \fIoption\fR, or via the \fBexport\fR and \fBunexport\fR definitions.
    If in a private definition context (see the \fBprivate\fR definition command, below) or if the \fB-private\fR flag is given for \fIoption\fR, this command creates private procedure-like methods.

**mixin** ?\fI-slotOperation\fR? ?\fIclassName ...\fR?
: This slot (see \fBSLOTTED DEFINITIONS\fR below) sets or updates a per-object list of additional classes that are to be mixed into the object. Each argument, \fIclassName\fR, names a single class that is to be mixed in. By default, this slot works by replacement.

**private** \fIcmd arg...\fR
: ...see next...

**private** \fIscript\fR
: This evaluates the \fIscript\fR (or the list of command and arguments given by \fIcmd\fR and \fIarg\fRs) in a context where the definitions made on the current object will be private definitions.
    The following class definition commands are affected by \fBprivate\fR: \fBforward\fR, \fBmethod\fR, and \fBvariable\fR. Nesting \fBprivate\fR inside \fBprivate\fR has no cumulative effect; the innermost definition context is just a private definition context. All other definition commands have no difference in behavior when used in a private definition context.

**unexport** \fIname\fR ?\fIname ...\fR?
: This arranges for each of the named methods, \fIname\fR, to be not exported (i.e. not usable outside the object through the object's command, but instead just through the \fBmy\fR command visible in the object's context) by the object being defined. Note that the methods themselves may be actually defined by a class; instance unexports override class visibility.

**variable** ?\fI-slotOperation\fR? ?\fIname ...\fR?
: This slot (see \fBSLOTTED DEFINITIONS\fR below) arranges for each of the named variables to be automatically made available in the methods declared by the object being defined.  Each variable name must not have any namespace separators and must not look like an array access. All variables will be actually present in the namespace of the object on which the method is executed. Note that the variable lists declared by the classes and mixins of which the object is an instance are completely disjoint; the list of variable names is just for methods declared by this object. By default, this slot works by appending.
    If in a private definition context (see the \fBprivate\fR definition command, below), this slot manipulates the list of private variable bindings for this object.  In a private variable binding, the name of the variable within the instance object is different to the name given in the definition; the name used in the definition is the name that you use to access the variable within the methods of this instance object, and the name of the variable in the instance namespace has a unique prefix that makes accidental use from superclass methods extremely unlikely.


## Advanced object configuration options

The following definitions are also supported, but are not required in simple programs:

**class** \fIclassName\fR
: This allows the class of an object to be changed after creation. Note that the class's constructors are not called when this is done, and so the object may well be in an inconsistent state unless additional configuration work is done.

**deletemethod** \fIname\fR ?\fIname ...\fR
: This deletes each of the methods called \fIname\fR from an object. The methods must have previously existed in that object (e.g., because it was created through \fBoo::objdefine method\fR). Does not affect the classes that the object is an instance of, or remove the exposure of those class-provided methods in the instance of that class.

**filter** ?\fI-slotOperation\fR? ?\fImethodName ...\fR?
: This slot (see \fBSLOTTED DEFINITIONS\fR below) sets or updates the list of method names that are used to guard whether a method call to the object may be called and what the method's results are. Each \fImethodName\fR names a single filtering method (which may be exposed or not exposed); it is not an error for a non-existent method to be named. Note that the actual list of filters also depends on the filters set upon any classes that the object is an instance of. By default, this slot works by appending.

**renamemethod** \fIfromName toName\fR
: This renames the method called \fIfromName\fR in an object to \fItoName\fR. The method must have previously existed in the object, and \fItoName\fR must not previously refer to a method in that object. Does not affect the classes that the object is an instance of and cannot rename in an instance object the methods provided by those classes (though a \fBoo::objdefine forward\fRed method may provide an equivalent capability). Does not change the export status of the method; if it was exported before, it will be afterwards.

**self** 
: This gives the name of the object currently being configured.


# Private methods

::: {.info -version="TIP500"}
When a class or instance has a private method, that private method can only be invoked from within methods of that class or instance. Other callers of the object's methods \fIcannot\fR invoke private methods, it is as if the private methods do not exist. However, a private method of a class \fIcan\fR be invoked from the class's methods when those methods are being used on another instance object; this means that a class can use them to coordinate behaviour between several instances of itself without interfering with how other classes (especially either subclasses or superclasses) interact. Private methods precede all mixed in classes in the method call order (as reported by \fBself call\fR).
:::

# Slotted definitions

Some of the configurable definitions of a class or object are \fIslotted definitions\fR. This means that the configuration is implemented by a slot object, that is an instance of the class \fBoo::Slot\fR, which manages a list of values (class names, variable names, etc.) that comprises the contents of the slot.

The \fBoo::Slot\fR class defines six operations (as methods) that may be done on the slot:

*slot* \fB-append\fR ?\fImember ...\fR?
: This appends the given \fImember\fR elements to the slot definition.

*slot* \fB-appendifnew\fR ?\fImember ...\fR?
: This appends the given \fImember\fR elements to the slot definition if they do not already exist.

*slot* \fB-clear\fR
: This sets the slot definition to the empty list.

*slot* \fB-prepend\fR ?\fImember ...\fR?
: This prepends the given \fImember\fR elements to the slot definition.

*slot* \fB-remove\fR ?\fImember ...\fR?
: This removes the given \fImember\fR elements from the slot definition.

*slot* \fB-set\fR ?\fImember ...\fR?
: This replaces the slot definition with the given \fImember\fR elements.


A consequence of this is that any use of a slot's default operation where the first member argument begins with a hyphen will be an error. One of the above operations should be used explicitly in those circumstances.

You only need to make an instance of \fBoo::Slot\fR if you are definining your own slot that behaves like a standard slot.

## Slot implementation

Internally, slot objects also define a method \fB--default-operation\fR which is forwarded to the default operation of the slot (thus, for the class "**variable**" slot, this is forwarded to "**my -append**"), and these methods which provide the implementation interface:

*slot* \fBGet\fR
: Returns a list that is the current contents of the slot, but does not modify the slot. This method must always be called from a stack frame created by a call to \fBoo::define\fR or \fBoo::objdefine\fR. This method \fIshould not\fR return an error unless it is called from outside a definition context or with the wrong number of arguments.
    The elements of the list should be fully resolved, if that is a meaningful concept to the slot.

*slot* \fBResolve\fR \fIslotElement\fR
: Returns \fIslotElement\fR with a resolution operation applied to it, but does not modify the slot. For slots of simple strings, this is an operation that does nothing, whereas for slots of classes, this maps a class name to its fully-qualified class name.  This method must always be called from a stack frame created by a call to \fBoo::define\fR or \fBoo::objdefine\fR.  This method \fIshould not\fR return an error unless it is called from outside a definition context or with the wrong number of arguments; unresolvable arguments should be returned as is (as not all slot operations strictly require that values are resolvable to work).
    Implementations \fIshould not\fR enforce uniqueness and ordering constraints in this method; that is the responsibility of the \fBSet\fR method.

*slot* \fBResolve\fR \fIelement\fR
: This converts an element of the slotted collection into its resolved form; for a simple value, it could just return the value, but for a slot that contains references to commands or classes it should convert those into their fully-qualified forms (so they can be compared with \fBstring equals\fR): that could be done by forwarding to \fBnamespace which\fR or similar.

*slot* \fBSet\fR \fIelementList\fR
: Sets the contents of the slot to the list \fIelementList\fR and returns the empty string. This method must always be called from a stack frame created by a call to \fBoo::define\fR or \fBoo::objdefine\fR. This method may return an error if it rejects the change to the slot contents (e.g., because of invalid values) as well as if it is called from outside a definition context or with the wrong number of arguments.
    This method \fImay\fR reorder and filter the elements if this is necessary in order to satisfy the underlying constraints of the slot. (For example, slots of classes enforce a uniqueness constraint that places each element in the earliest location in the slot that it can.)


The implementation of these methods is slot-dependent (and responsible for accessing the correct part of the class or object definition). Slots also have an unknown method handler to tie all these pieces together, and they hide their \fBdestroy\fR method so that it is not invoked inadvertently. It is \fIrecommended\fR that any user changes to the slot mechanism itself be restricted to defining new operations whose names start with a hyphen.

Note that slot instances are not expected to contain the storage for the slot they manage; that will be in or attached to the class or object that they manage. Those instances should provide their own implementations of the \fBGet\fR and \fBSet\fR methods (and optionally \fBResolve\fR; that defaults to a do-nothing pass-through).

::: {.info -version="TIP516"}
Most slot operations will initially \fBResolve\fR their argument list, combine it with the results of the \fBGet\fR method, and then \fBSet\fR the result. Some operations omit one or both of the first two steps; omitting the third would result in an idempotent read-only operation (but the standard mechanism for reading from slots is via \fBinfo class\fR and \fBinfo object\fR).
:::

# Examples

This example demonstrates how to use both forms of the \fBoo::define\fR and \fBoo::objdefine\fR commands (they work in the same way), as well as illustrating four of their subcommands.

```
oo::class create c
c create o
oo::define c method foo {} {
    puts "world"
}
oo::objdefine o {
    method bar {} {
        my Foo "hello "
        my foo
    }
    forward Foo ::puts -nonewline
    unexport foo
}
o bar                \(-> prints "hello world"
o foo                \(-> error "unknown method foo"
o Foo Bar            \(-> error "unknown method Foo"
oo::objdefine o renamemethod bar lollipop
o lollipop           \(-> prints "hello world"
```

This example shows how additional classes can be mixed into an object. It also shows how \fBmixin\fR is a slot that supports appending:

```
oo::object create inst
inst m1              \(-> error "unknown method m1"
inst m2              \(-> error "unknown method m2"

oo::class create A {
    method m1 {} {
        puts "red brick"
    }
}
oo::objdefine inst {
    mixin A
}
inst m1              \(-> prints "red brick"
inst m2              \(-> error "unknown method m2"

oo::class create B {
    method m2 {} {
        puts "blue brick"
    }
}
oo::objdefine inst {
    mixin -append B
}
inst m1              \(-> prints "red brick"
inst m2              \(-> prints "blue brick"
```

::: {.info -version="TIP478"}
This example shows how to create and use class variables. It is a class that counts how many instances of itself have been made.
:::

```
oo::class create Counted
oo::define Counted {
    initialise {
        variable count 0
    }

    variable number
    constructor {} {
        classvariable count
        set number [incr count]
    }

    method report {} {
        classvariable count
        puts "This is instance $number of $count"
    }
}

set a [Counted new]
set b [Counted new]
$a report
        \(-> This is instance 1 of 2
set c [Counted new]
$b report
        \(-> This is instance 2 of 3
$c report
        \(-> This is instance 3 of 3
```

This example demonstrates how to use class methods. (Note that the constructor for \fBoo::class\fR calls \fBoo::define\fR on the class.)

```
oo::class create DBTable {
    classmethod find {description} {
        puts "DB: locate row from [self] matching $description"
        return [my new]
    }
    classmethod insert {description} {
        puts "DB: create row in [self] matching $description"
        return [my new]
    }
    method update {description} {
        puts "DB: update row [self] with $description"
    }
    method delete {} {
        puts "DB: delete row [self]"
        my destroy; # Just delete the object, not the DB row
    }
}

oo::class create Users {
    superclass DBTable
}
oo::class create Groups {
    superclass DBTable
}

set u1 [Users insert "username=abc"]
        \(-> DB: create row from ::Users matching username=abc
set u2 [Users insert "username=def"]
        \(-> DB: create row from ::Users matching username=def
$u2 update "group=NULL"
        \(-> DB: update row ::oo::Obj124 with group=NULL
$u1 delete
        \(-> DB: delete row ::oo::Obj123
set g [Group find "groupname=webadmins"]
        \(-> DB: locate row ::Group with groupname=webadmins
$g update "emailaddress=admins"
        \(-> DB: update row ::oo::Obj125 with emailaddress=admins
```

::: {.info -version="TIP524"}
This example shows how to make a custom definition for a class. Note that it explicitly includes delegation to the existing definition commands via \fBnamespace path\fR.
:::

```
namespace eval myDefinitions {
    # Delegate to existing definitions where not overridden
    namespace path ::oo::define

    # A custom type of method
    proc exprmethod {name arguments body} {
        tailcall method $name $arguments [list expr $body]
    }

    # A custom way of building a constructor
    proc parameters args {
        uplevel 1 [list variable {*}$args]
        set body [join [lmap a $args {
            string map [list VAR $a] {
                set [my varname VAR] [expr {double($VAR)}]
            }
        }] ";"]
        tailcall constructor $args $body
    }
}

# Bind the namespace into a (very simple) metaclass for use
oo::class create exprclass {
    superclass oo::class
    definitionnamespace myDefinitions
}

# Use the custom definitions
exprclass create quadratic {
    parameters a b c
    exprmethod evaluate {x} {
        ($a * $x**2) + ($b * $x) + $c
    }
}

# Showing the resulting class and object in action
quadratic create quad 1 2 3
for {set x 0} {$x <= 4} {incr x} {
    puts [format "quad(%d) = %.2f" $x [quad evaluate $x]]
}
        \(-> quad(0) = 3.00
        \(-> quad(1) = 6.00
        \(-> quad(2) = 11.00
        \(-> quad(3) = 18.00
        \(-> quad(4) = 27.00
```

