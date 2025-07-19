---
CommandName: configurable
ManualSection: n
Version: 0.4
TclPart: TclOO
TclDescription: TclOO Commands
Links:
 - info(n)
 - oo::class(n)
 - oo::define(n)
Keywords:
 - class
 - object
 - properties
 - configuration
Copyright:
 - Copyright (c) 2019 Donal K. Fellows
---

# Name

oo::configurable, configure, property - class that makes configurable classes and objects, and supports making configurable properties

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [TclOO]{.lit}

[oo::configurable]{.cmd} [create]{.sub} [class]{.arg} [definitionScript]{.optarg}

[oo::define]{.cmd} [class]{.arg} [{]{.lit}
[property]{.cmd} [propName]{.arg} [propName]{.optdot}
[property]{.cmd} [propName]{.arg} [option]{.arg} [value]{.arg} [option value]{.optdot} [propName]{.optarg} [option value]{.optarg} [option value]{.optdot}
[}]{.cmd}

[oo::objdefine]{.cmd} [object]{.arg} [{]{.lit}
[property]{.cmd} [propName]{.arg} [propName]{.optdot}
[property]{.cmd} [propName]{.arg} [option]{.arg} [value]{.arg} [option value]{.optdot} [propName]{.optarg} [option value]{.optarg} [option value]{.optdot}
[}]{.cmd}

[objectName]{.ins} [configure]{.sub}
[objectName]{.ins} [configure]{.sub} [-prop]{.arg}
[objectName]{.ins} [configure]{.sub} [-prop]{.arg} [value]{.arg} [-prop value]{.optdot}
:::

# Class hierarchy

**oo::object**    \(-> **oo::class**        \(-> **oo::configurable**  **oo::object**    \(-> **oo::class**        \(-> **oo::configurablesupport::configurable**

# Description

Configurable objects are objects that support being configured with a **configure** method. Each of the configurable entities of the object is known as a property of the object. Properties may be defined on classes or instances; when configuring an object, any of the properties defined by its classes (direct or indirect) or by the instance itself may be configured.

The **oo::configurable** metaclass installs basic support for making configurable objects into a class. This consists of making a **property** definition command available in definition scripts for the class and instances (e.g., from the class's constructor, within **oo::define** and within **oo::objdefine**) and making a **configure** method available within the instances.

## Configure method

The behavior of the **configure** method is modelled after the **fconfigure**/**chan configure** command.

If passed no additional arguments, the **configure** method returns an alphabetically sorted dictionary of all *readable* and *read-write* properties and their current values.

If passed a single additional argument, that argument to the **configure** method must be the name of a property to read (or an unambiguous prefix thereof); its value is returned.

Otherwise, if passed an even number of arguments then each pair of arguments specifies a property name (or an unambiguous prefix thereof) and the value to set it to. The properties will be set in the order specified, including duplicates. If the setting of any property fails, the overall **configure** method fails, the preceding pairs (if any) will continue to have been applied, and the succeeding pairs (if any) will be not applied. On success, the result of the **configure** method in this mode operation will be an empty string.

## Property definitions

When a class has been manufactured by the **oo::configurable** metaclass (or one of its subclasses), it gains an extra definition, **property**. The **property** definition defines one or more properties that will be exposed by the class's instances.

The **property** command takes the name of a property to define first, *without a leading hyphen*, followed by a number of option-value pairs that modify the basic behavior of the property. This can then be followed by an arbitrary number of other property definitions. The supported options are:

**-get** *getterScript*
: This defines the implementation of how to read from the property; the *getterScript* will become the body of a method (taking no arguments) defined on the class, if the kind of the property is such that the property can be read from. The method will be named **<ReadProp-***propertyName***>**, and will default to being a simple read of the instance variable with the same name as the property (e.g., "**property** xyz" will result in a method "<ReadProp-xyz>" being created).

**-kind** *propertyKind*
: This defines what sort of property is being created. The *propertyKind* must be exactly one of **readable**, **writable**, or **readwrite** (which is the default) which will make the property read-only, write-only or read-write, respectively.  Read-only properties can only ever be read from, write-only properties can only ever be written to, and read-write properties can be both read and written.
    Note that write-only properties are not particularly discoverable as they are never reported by the **configure** method other than by error messages when attempting to write to a property that does not exist.

**-set** *setterScript*
: This defines the implementation of how to write to the property; the *setterScript* will become the body of a method taking a single argument, *value*, defined on the class, if the kind of the property is such that the property can be written to. The method will be named **<WriteProp-***propertyName***>**, and will default to being a simple write of the instance variable with the same name as the property (e.g., "**property** xyz" will result in a method "<WriteProp-xyz>" being created).


Instances of the class that was created by **oo::configurable** will also support **property** definitions; the semantics will be exactly as above except that the properties will be defined on the instance alone.

Note that the property implementation methods that **property** defines should not be private, as this makes them inaccessible from the implementation of **configure** (by design; the property configuration mechanism is intended for use mainly from outside a class, whereas a class may access variables directly). The variables accessed by the default implementations of the properties *may* be private, if so declared.

# Advanced usage

The configurable class system is comprised of several pieces. The **oo::configurable** metaclass works by mixing in a class and setting definition namespaces during object creation that provide the other bits and pieces of machinery. The key pieces of the implementation are enumerated here so that they can be used by other code:

**oo::configuresupport::configurable**
: This is a class that provides the implementation of the **configure** method (described above in **CONFIGURE METHOD**).

**oo::configuresupport::configurableclass**
: This is a namespace that contains the definition dialect that provides the **property** declaration for use in classes (i.e., via **oo::define**, and class constructors under normal circumstances), as described above in **PROPERTY DEFINITIONS**. It **namespace export**s its **property** command so that it may be used easily in user definition dialects.

**oo::configuresupport::configurableobject**
: This is a namespace that contains the definition dialect that provides the **property** declaration for use in instance objects (i.e., via **oo::objdefine**, and the **self** declaration in **oo::define**), as described above in **PROPERTY DEFINITIONS**. It **namespace export**s its **property** command so that it may be used easily in user definition dialects.


The underlying property discovery mechanism relies on four slots (see **oo::define** for what that implies) that list the properties that can be configured. These slots do not themselves impose any semantics on what the slots mean other than that they have unique names, no important order, can be inherited and discovered on classes and instances.

These slots, and their intended semantics, are:

**oo::configuresupport::readableproperties**
: The set of properties of a class (not including those from its superclasses) that may be read from when configuring an instance of the class. This slot can also be read with the **info class properties** command.

**oo::configuresupport::writableproperties**
: The set of properties of a class (not including those from its superclasses) that may be written to when configuring an instance of the class. This slot can also be read with the **info class properties** command.

**oo::configuresupport::objreadableproperties**
: The set of properties of an object instance (not including those from its classes) that may be read from when configuring the object. This slot can also be read with the **info object properties** command.

**oo::configuresupport::objwritableproperties**
: The set of properties of an object instance (not including those from its classes) that may be written to when configuring the object. This slot can also be read with the **info object properties** command.


Note that though these are slots, they are *not* in the standard **oo::define** or **oo::objdefine** namespaces; in order to use them inside a definition script, they need to be referred to by full name. This is because they are intended to be building bricks of configurable property system, and not directly used by normal user code.

## Implementation note

The implementation of the **configure** method uses **info object properties** with the **-all** option to discover what properties it may manipulate.

# Examples

Here we create a simple configurable class and demonstrate how it can be configured:

```
oo::configurable create Point {
    property x y
    constructor args {
        my configure -x 0 -y 0 {*}$args
    }
    variable x y
    method print {} {
        puts "x=$x, y=$y"
    }
}

set pt [Point new -x 27]
$pt print;   # x=27, y=0
$pt configure -y 42
$pt print;   # x=27, y=42
puts "distance from origin: [expr {
    hypot([$pt configure -x], [$pt configure -y])
}]";         # distance from origin: 49.92995093127971
puts [$pt configure]
             # -x 27 -y 42
```

Such a configurable class can be extended by subclassing, though the subclass needs to also be created by **oo::configurable** if it will use the **property** definition:

```
oo::configurable create Point3D {
    superclass Point
    property z
    constructor args {
        next -z 0 {*}$args
    }
}

set pt2 [Point3D new -x 2 -y 3 -z 4]
puts [$pt2 configure]
             # -x 2 -y 3 -z 4
```

Once you have a configurable class, you can also add instance properties to it. (The backing variables for all properties start unset.) Note below that we are using an unambiguous prefix of a property name when setting it; this is supported for all properties though full names are normally recommended because subclasses will not make an unambiguous prefix become ambiguous in that case.

```
oo::objdefine $pt {
    property color
}
$pt configure -c bisque
puts [$pt configure]
             # -color bisque -x 27 -y 42
```

You can also do derived properties by making them read-only and supplying a script that computes them.

```
oo::configurable create PointMk2 {
    property x y
    property distance -kind readable -get {
        return [expr {hypot($x, $y)}]
    }
    variable x y
    constructor args {
        my configure -x 0 -y 0 {*}$args
    }
}

set pt3 [PointMk2 new -x 3 -y 4]
puts [$pt3 configure -distance]
             # 5.0
$pt3 configure -distance 10
             # ERROR: bad property "-distance": must be -x or -y
```

Setters are used to validate the type of a property:

```
oo::configurable create PointMk3 {
    property x -set {
        if {![string is double -strict $value]} {
            error "-x property must be a number"
        }
        set x $value
    }
    property y -set {
        if {![string is double -strict $value]} {
            error "-y property must be a number"
        }
        set y $value
    }
    variable x y
    constructor args {
        my configure -x 0 -y 0 {*}$args
    }
}

set pt4 [PointMk3 new]
puts [$pt4 configure]
             # -x 0 -y 0
$pt4 configure -x 3 -y 4
puts [$pt4 configure]
             # -x 3 -y 4
$pt4 configure -x "obviously not a number"
             # ERROR: -x property must be a number
```

