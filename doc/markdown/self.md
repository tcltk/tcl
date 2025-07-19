---
CommandName: self
ManualSection: n
Version: 0.1
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - info(n)
 - next(n)
Keywords:
 - call
 - introspection
 - object
Copyright:
 - Copyright (c) 2007 Donal K. Fellows
---

# Name

self - method call internal introspection

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[self]{.cmd} [subcommand]{.optarg}
:::

# Description

The **self** command, which should only be used from within the context of a call to a method (i.e. inside a method, constructor or destructor body) is used to allow the method to discover information about how it was called. It takes an argument, *subcommand*, that tells it what sort of information is actually desired; if omitted the result will be the same as if **self object** was invoked. The supported subcommands are:

**self call**
: This returns a two-element list describing the method implementations used to implement the current call chain. The first element is the same as would be reported by **info object** **call** for the current method (except that this also reports useful values from within constructors and destructors, whose names are reported as **<constructor>** and **<destructor>** respectively, and for private methods, which are described as being **private** instead of being a **method**), and the second element is an index into the first element's list that indicates which actual implementation is currently executing (the first implementation to execute is always at index 0).

**self caller**
: When the method was invoked from inside another object method, this subcommand returns a three element list describing the containing object and method. The first element describes the declaring object or class of the method, the second element is the name of the object on which the containing method was invoked, and the third element is the name of the method (with the strings **<constructor>** and **<destructor>** indicating constructors and destructors respectively).

**self class**
: This returns the name of the class that the current method was defined within. Note that this will change as the chain of method implementations is traversed with **next**, and that if the method was defined on an object then this will fail.
    If you want the class of the current object, you need to use this other construct:

    ```
    info object class [self object]
    ```

**self filter**
: When invoked inside a filter, this subcommand returns a three element list describing the filter. The first element gives the name of the object or class that declared the filter (note that this may be different from the object or class that provided the implementation of the filter), the second element is either **object** or **class** depending on whether the declaring entity was an object or class, and the third element is the name of the filter.

**self method**
: This returns the name of the current method (with the strings **<constructor>** and **<destructor>** indicating constructors and destructors respectively).

**self namespace**
: This returns the name of the unique namespace of the object that the method was invoked upon.

**self next**
: When invoked from a method that is not at the end of a call chain (i.e. where the **next** command will invoke an actual method implementation), this subcommand returns a two element list describing the next element in the method call chain; the first element is the name of the class or object that declares the next part of the call chain, and the second element is the name of the method (with the strings **<constructor>** and **<destructor>** indicating constructors and destructors respectively). If invoked from a method that is at the end of a call chain, this subcommand returns the empty string.

**self object**
: This returns the name of the object that the method was invoked upon.

**self target**
: When invoked inside a filter implementation, this subcommand returns a two element list describing the method being filtered. The first element will be the name of the declarer of the method, and the second element will be the actual name of the method.


# Examples

This example shows basic use of **self** to provide information about the current object:

```
oo::class create c {
    method foo {} {
        puts "this is the [self] object"
    }
}
c create a
c create b
a foo                \(-> prints "this is the ::a object"
b foo                \(-> prints "this is the ::b object"
```

This demonstrates what a method call chain looks like, and how traversing along it changes the index into it:

```
oo::class create c {
    method x {} {
        puts "Cls: [self call]"
    }
}
c create a
oo::objdefine a {
    method x {} {
        puts "Obj: [self call]"
        next
        puts "Obj: [self call]"
    }
}
a x     \(-> Obj: {{method x object method} {method x ::c method}} 0
        \(-> Cls: {{method x object method} {method x ::c method}} 1
        \(-> Obj: {{method x object method} {method x ::c method}} 0
```

