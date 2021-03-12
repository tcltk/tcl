# Design of Tcl string values for Tcl 9

## DRAFT WORK IN PROGRESS. NOT (YET) NORMATIVE

All Tcl values are strings, but what is a string?

The aim of this document is to spell out what the answer to that question
has been, what it should be, and how we get from one to the other.

## Fundamentals

A ***string*** is a sequence of zero or more symbols, each ***symbol***
a member of a symbol set known as an ***alphabet***.  For example, the
Tcl string **cat** is the sequence of three symbols **c**, **a**, **t**.

Each symbol in the Tcl alphabet is associated with a non-negative integer,
known as the ***code*** of that symbol.  Distinct symbols have distinct
codes.  Schemes that associate symbols with code values are known
as ***character sets***.  For example, in Tcl's character set, the
symbols **c**, **a**, and **t** are associated with code
values 99, 97, and 116 respectively.

There is an ordering imposed on the alphabet by the numeric order of
the associated code values.  This ordering allows symbols to be compared
and sorted.  The natural extension of that ordering to sequences
establishes one well defined way to compare, order and sort string values.

The number of symbols in a string's symbol sequence is the ***length***
of that string.  

A symbol within a string can be identified by its place in the sequence,
counting with an ***index*** that starts at 0.  A string of length *N*
has a symbol at each of the index values 0 through *N*-1.  We can
represent the symbol found at index *i* within string *s* with the
notation *s*[*i*].

Given a string *s* of length *N*, every pair of an index
value *i*, 0 <= *i* <= *N*, and a length *L*, 0 <= *L* <= *N* - *i*,
defines a substring of *s* of length *L* built from the sequence
of symbols *s*[*i*], ..., *s*[*i* + *L* - 1].

Given a string *s* of length *M* and a string *t* of length *N*, the
concatenation of *s* and *t* is the string *u* of length *L* = *M* + *N*,
with *u*[0] = *s*[0], ... *u*[*M* - 1] = *s*[*M* - 1],
*u*[*M*] = *t*[0], ... *u*[*L* - 1] = *t*[*N* - 1].

In all of these descriptions, the string values act in ways with a
direct analog to C arrays of integer values.

To be able to create and store an arbitrary string, a program needs
at a minimum the primitive ability to create and empty string, to create
a string of length 1 containing each symbol in the alphabet, and the
ability to concatenate arbitrary strings.  Other important primitives
are the ability to index into a string, take a substring from a string,
and compare symbols and strings.  

## Tcl strings as the universal value set



## Tcl alphabet versions


## Representations




