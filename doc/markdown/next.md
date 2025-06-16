---
CommandName: next
ManualSection: n
Version: 0.1
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - oo::class(n)
 - oo::define(n)
 - oo::object(n)
 - self(n)
Keywords:
 - call
 - method
 - method chain
Copyright:
 - Copyright (c) 2007 Donal K. Fellows
---

# Name

next, nextto - invoke superclass method implementations

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::oo]{.lit}

[next]{.cmd} [arg]{.optdot}
[nextto]{.cmd} [class]{.arg} [arg]{.optdot}

:::

# Description

The **next** command is used to call implementations of a method by a class, superclass or mixin that are overridden by the current method. It can only be used from within a method. It is also used within filters to indicate the point where a filter calls the actual implementation (the filter may decide to not go along the chain, and may process the results of going along the chain of methods as it chooses). The result of the **next** command is the result of the next method in the method chain; if there are no further methods in the method chain, the result of **next** will be an error. The arguments, *arg*, to **next** are the arguments to pass to the next method in the chain.

The **nextto** command is the same as the **next** command, except that it takes an additional *class* argument that identifies a class whose implementation of the current method chain (see **info object** **call**) should be used; the method implementation selected will be the one provided by the given class, and it must refer to an existing non-filter invocation that lies further along the chain than the current implementation.

# The method chain

When a method of an object is invoked, things happen in several stages:

1. The structure of the object, its class, superclasses, filters, and mixins, are examined to build a *method chain*, which contains a list of method implementations to invoke.

2. The first method implementation on the chain is invoked.

3. If that method implementation invokes the **next** command, the next method implementation is invoked (with its arguments being those that were passed to **next**).

4. The result from the overall method call is the result from the outermost method implementation; inner method implementations return their results through **next**.

5. The method chain is cached for future use.


## Method search order

When constructing the method chain, method implementations are searched for in the following order:

1. In the classes mixed into the object, in class traversal order. The list of mixins is checked in natural order.

2. In the classes mixed into the classes of the object, with sources of mixing in being searched in class traversal order. Within each class, the list of mixins is processed in natural order.

3. In the object itself.

4. In the object's class.

5. In the superclasses of the class, following each superclass in a depth-first fashion in the natural order of the superclass list.


Any particular method implementation always comes as *late* in the resulting list of implementations as possible; this means that if some class, A, is both mixed into a class, B, and is also a superclass of B, the instances of B will always treat A as a superclass from the perspective of inheritance. This is true even when the multiple inheritance is processed indirectly.

## Filters

When an object has a list of filter names set upon it, or is an instance of a class (or has mixed in a class) that has a list of filter names set upon it, before every invocation of any method the filters are processed. Filter implementations are found in class traversal order, as are the lists of filter names (each of which is traversed in natural list order). Explicitly invoking a method used as a filter will cause that method to be invoked twice, once as a filter and once as a normal method.

Each filter should decide for itself whether to permit the execution to go forward to the proper implementation of the method (which it does by invoking the **next** command as filters are inserted into the front of the method call chain) and is responsible for returning the result of **next**.

Filters are invoked when processing an invocation of the **unknown** method because of a failure to locate a method implementation, but *not* when invoking either constructors or destructors. (Note however that the **destroy** method is a conventional method, and filters are invoked as normal when it is called.)

# Examples

This example demonstrates how to use the **next** command to call the (super)class's implementation of a method. The script:

```
oo::class create theSuperclass {
    method example {args} {
        puts "in the superclass, args = $args"
    }
}

oo::class create theSubclass {
    superclass theSuperclass
    method example {args} {
        puts "before chaining from subclass, args = $args"
        next a {*}$args b
        next pureSynthesis
        puts "after chaining from subclass"
    }
}

theSubclass create obj
oo::objdefine obj method example args {
    puts "per-object method, args = $args"
    next x {*}$args y
    next
}
obj example 1 2 3
```

prints the following:

```
per-object method, args = 1 2 3
before chaining from subclass, args = x 1 2 3 y
in the superclass, args = a x 1 2 3 y b
in the superclass, args = pureSynthesis
after chaining from subclass
before chaining from subclass, args =
in the superclass, args = a b
in the superclass, args = pureSynthesis
after chaining from subclass
```

This example demonstrates how to build a simple cache class that applies memoization to all the method calls of the objects it is mixed into, and shows how it can make a difference to computation times:

```
oo::class create cache {
    filter Memoize
    method Memoize args {
        # Do not filter the core method implementations
        if {[lindex [self target] 0] eq "::oo::object"} {
            return [next {*}$args]
        }

        # Check if the value is already in the cache
        my variable ValueCache
        set key [self target],$args
        if {[info exist ValueCache($key)]} {
            return $ValueCache($key)
        }

        # Compute value, insert into cache, and return it
        return [set ValueCache($key) [next {*}$args]]
    }

    method flushCache {} {
        my variable ValueCache
        unset ValueCache
        # Skip the caching
        return -level 2 ""
    }
}

oo::object create demo
oo::objdefine demo {
    mixin cache

    method compute {a b c} {
        after 3000 ;# Simulate deep thought
        return [expr {$a + $b * $c}]
    }

    method compute2 {a b c} {
        after 3000 ;# Simulate deep thought
        return [expr {$a * $b + $c}]
    }
}

puts [demo compute  1 2 3]      \(-> prints "7" after delay
puts [demo compute2 4 5 6]      \(-> prints "26" after delay
puts [demo compute  1 2 3]      \(-> prints "7" instantly
puts [demo compute2 4 5 6]      \(-> prints "26" instantly
puts [demo compute  4 5 6]      \(-> prints "34" after delay
puts [demo compute  4 5 6]      \(-> prints "34" instantly
puts [demo compute  1 2 3]      \(-> prints "7" instantly
demo flushCache
puts [demo compute  1 2 3]      \(-> prints "7" after delay
```

