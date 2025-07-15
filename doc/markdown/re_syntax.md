---
CommandName: re_syntax
ManualSection: n
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - RegExp(3)
 - regexp(n)
 - regsub(n)
 - lsearch(n)
 - switch(n)
 - text(n)
Keywords:
 - match
 - regular expression
 - string
Copyright:
 - Copyright (c) 1998 Sun Microsystems, Inc.
 - Copyright (c) 1999 Scriptics Corporation
---

# Name

re_syntax - Syntax of Tcl regular expressions

# Description

A \fIregular expression\fR describes strings of characters. It's a pattern that matches certain strings and does not match others.

# Different flavours of REs

Regular expressions ("RE"s), as defined by POSIX, come in two flavors: \fIextended\fR REs ("ERE"s) and \fIbasic\fR REs ("BRE"s). EREs are roughly those of the traditional \fIegrep\fR, while BREs are roughly those of the traditional \fIed\fR. This implementation adds a third flavor, \fIadvanced\fR REs ("ARE"s), basically EREs with some significant extensions.

This manual page primarily describes AREs. BREs mostly exist for backward compatibility in some old programs; they will be discussed at the end. POSIX EREs are almost an exact subset of AREs. Features of AREs that are not present in EREs will be indicated.

# Regular expression syntax

Tcl regular expressions are implemented using the package written by Henry Spencer, based on the 1003.2 spec and some (not quite all) of the Perl5 extensions (thanks, Henry!). Much of the description of regular expressions below is copied verbatim from his manual entry.

An ARE is one or more \fIbranches\fR, separated by "**|**", matching anything that matches any of the branches.

A branch is zero or more \fIconstraints\fR or \fIquantified atoms\fR, concatenated. It matches a match for the first, followed by a match for the second, etc; an empty branch matches the empty string.

## Quantifiers

A quantified atom is an \fIatom\fR possibly followed by a single \fIquantifier\fR. Without a quantifier, it matches a single match for the atom. The quantifiers, and what a so-quantified atom matches, are:

*****
: a sequence of 0 or more matches of the atom

**+**
: a sequence of 1 or more matches of the atom

**?**
: a sequence of 0 or 1 matches of the atom

**{***m***}**
: a sequence of exactly \fIm\fR matches of the atom

**{***m***,}**
: a sequence of \fIm\fR or more matches of the atom

**{***m***,***n***}**
: a sequence of \fIm\fR through \fIn\fR (inclusive) matches of the atom; \fIm\fR may not exceed \fIn\fR

***?  +?  ??  {***m***}?  {***m***,}?  {***m***,***n***}?**
: *non-greedy* quantifiers, which match the same possibilities, but prefer the smallest number rather than the largest number of matches (see \fBMATCHING\fR)


The forms using \fB{\fR and \fB}\fR are known as \fIbound\fRs. The numbers \fIm\fR and \fIn\fR are unsigned decimal integers with permissible values from 0 to 255 inclusive.

## Atoms

An atom is one of:

**(***re***)**
: matches a match for \fIre\fR (\fIre\fR is any regular expression) with the match noted for possible reporting

**(?:***re***)**
: as previous, but does no reporting (a "non-capturing" set of parentheses)

**()**
: matches an empty string, noted for possible reporting

**(?:)**
: matches an empty string, without reporting

**[***chars***]**
: a \fIbracket expression\fR, matching any one of the \fIchars\fR (see \fBBRACKET EXPRESSIONS\fR for more detail)

**.**
: matches any single character

**\***k*
: matches the non-alphanumeric character \fIk\fR taken as an ordinary character, e.g. \fB\\\fR matches a backslash character

**\***c*
: where \fIc\fR is alphanumeric (possibly followed by other characters), an \fIescape\fR (AREs only), see \fBESCAPES\fR below

**{**
: when followed by a character other than a digit, matches the left-brace character "**{**"; when followed by a digit, it is the beginning of a \fIbound\fR (see above)

*x*
: where \fIx\fR is a single character with no other significance, matches that character.


## Constraints

A \fIconstraint\fR matches an empty string when specific conditions are met. A constraint may not be followed by a quantifier. The simple constraints are as follows; some more constraints are described later, under \fBESCAPES\fR.

**^**
: matches at the beginning of the string or a line (according to whether matching is newline-sensitive or not, as described in \fBMATCHING\fR, below).

**$**
: matches at the end of the string or a line (according to whether matching is newline-sensitive or not, as described in \fBMATCHING\fR, below).
    The difference between string and line matching modes is immaterial when the string does not contain a newline character.  The \fB\A\fR and \fB\Z\fR constraint escapes have a similar purpose but are always constraints for the overall string.
    The default newline-sensitivity depends on the command that uses the regular expression, and can be overridden as described in \fBMETASYNTAX\fR, below.

**(?=***re***)**
: *positive lookahead* (AREs only), matches at any point where a substring matching \fIre\fR begins

**(?!***re***)**
: *negative lookahead* (AREs only), matches at any point where no substring matching \fIre\fR begins


The lookahead constraints may not contain back references (see later), and all parentheses within them are considered non-capturing.

An RE may not end with "**\**".

# Bracket expressions

A \fIbracket expression\fR is a list of characters enclosed in "**[\|]**". It normally matches any single character from the list (but see below). If the list begins with "**^**", it matches any single character (but see below) \fInot\fR from the rest of the list.

If two characters in the list are separated by "**-**", this is shorthand for the full \fIrange\fR of characters between those two (inclusive) in the collating sequence, e.g. "**[0-9]**" in Unicode matches any conventional decimal digit. Two ranges may not share an endpoint, so e.g. "**a-c-e**" is illegal. Ranges in Tcl always use the Unicode collating sequence, but other programs may use other collating sequences and this can be a source of incompatibility between programs.

To include a literal \fB]\fR or \fB-\fR in the list, the simplest method is to enclose it in \fB[.\fR and \fB.]\fR to make it a collating element (see below). Alternatively, make it the first character (following a possible "**^**"), or (AREs only) precede it with "**\**". Alternatively, for "**-**", make it the last character, or the second endpoint of a range. To use a literal \fB-\fR as the first endpoint of a range, make it a collating element or (AREs only) precede it with "**\**". With the exception of these, some combinations using \fB[\fR (see next paragraphs), and escapes, all other special characters lose their special significance within a bracket expression.

## Character classes

Within a bracket expression, the name of a \fIcharacter class\fR enclosed in \fB[:\fR and \fB:]\fR stands for the list of all characters (not all collating elements!) belonging to that class. Standard character classes are:

**alpha**
: A letter.

**upper**
: An upper-case letter.

**lower**
: A lower-case letter.

**digit**
: A decimal digit.

**xdigit**
: A hexadecimal digit.

**alnum**
: An alphanumeric (letter or digit).

**print**
: A "printable" (same as graph, except also including space).

**blank**
: A space or tab character.

**space**
: A character producing white space in displayed text.

**punct**
: A punctuation character.

**graph**
: A character with a visible representation (includes both \fBalnum\fR and \fBpunct\fR).

**cntrl**
: A control character.


A locale may provide others. A character class may not be used as an endpoint of a range.

(\fINote:\fR the current Tcl implementation has only one locale, the Unicode locale, which supports exactly the above classes.)

## Bracketed constraints

There are two special cases of bracket expressions: the bracket expressions "**[[:<:]]**" and "**[[:>:]]**" are constraints, matching empty strings at the beginning and end of a word respectively. A word is defined as a sequence of word characters that is neither preceded nor followed by word characters. A word character is an \fIalnum\fR character or an underscore ("**_**"). These special bracket expressions are deprecated; users of AREs should use constraint escapes instead (see below).

## Collating elements

Within a bracket expression, a collating element (a character, a multi-character sequence that collates as if it were a single character, or a collating-sequence name for either) enclosed in \fB[.\fR and \fB.]\fR stands for the sequence of characters of that collating element. The sequence is a single element of the bracket expression's list. A bracket expression in a locale that has multi-character collating elements can thus match more than one character. So (insidiously), a bracket expression that starts with \fB^\fR can match multi-character collating elements even if none of them appear in the bracket expression!

(\fINote:\fR Tcl has no multi-character collating elements. This information is only for illustration.)

For example, assume the collating sequence includes a \fBch\fR multi-character collating element. Then the RE "**[[.ch.]]*c**" (zero or more "**ch**s" followed by "**c**") matches the first five characters of "**chchcc**". Also, the RE "**[^c]b**" matches all of "**chb**" (because "**[^c]**" matches the multi-character "**ch**").

## Equivalence classes

Within a bracket expression, a collating element enclosed in \fB[=\fR and \fB=]\fR is an equivalence class, standing for the sequences of characters of all collating elements equivalent to that one, including itself. (If there are no other equivalent collating elements, the treatment is as if the enclosing delimiters were "**[.**"\& and "**.]**".) For example, if \fBo\fR and \fB\(^o\fR are the members of an equivalence class, then "**[[=o=]]**", "**[[=\(^o=]]**", and "**[o\(^o]**"\& are all synonymous. An equivalence class may not be an endpoint of a range.

(\fINote:\fR Tcl implements only the Unicode locale. It does not define any equivalence classes. The examples above are just illustrations.)

# Escapes

Escapes (AREs only), which begin with a \fB\\fR followed by an alphanumeric character, come in several varieties: character entry, class shorthands, constraint escapes, and back references. A \fB\\fR followed by an alphanumeric character but not constituting a valid escape is illegal in AREs. In EREs, there are no escapes: outside a bracket expression, a \fB\\fR followed by an alphanumeric character merely stands for that character as an ordinary character, and inside a bracket expression, \fB\\fR is an ordinary character. (The latter is the one actual incompatibility between EREs and AREs.)

## Character-entry escapes

Character-entry escapes (AREs only) exist to make it easier to specify non-printing and otherwise inconvenient characters in REs:

**\a**
: alert (bell) character, as in C

**\b**
: backspace, as in C

**\B**
: synonym for \fB\\fR to help reduce backslash doubling in some applications where there are multiple levels of backslash processing

**\c***X*
: (where \fIX\fR is any character) the character whose low-order 5 bits are the same as those of \fIX\fR, and whose other bits are all zero

**\**
: the character whose collating-sequence name is "**ESC**", or failing that, the character with octal value 033

**\f**
: formfeed, as in C

**\n**
: newline, as in C

**\r**
: carriage return, as in C

**\t**
: horizontal tab, as in C

**\u***wxyz*
: (where \fIwxyz\fR is one up to four hexadecimal digits) the Unicode character \fBU+\fR\fIwxyz\fR in the local byte ordering

**\U***stuvwxyz*
: (where \fIstuvwxyz\fR is one up to eight hexadecimal digits) reserved for a Unicode extension up to 21 bits. The digits are parsed until the first non-hexadecimal character is encountered, the maximum of eight hexadecimal digits are reached, or an overflow would occur in the maximum value of \fBU+\fR\fI10ffff\fR.

**\v**
: vertical tab, as in C

**\x***hh*
: (where \fIhh\fR is one or two hexadecimal digits) the character whose hexadecimal value is \fB0x\fR\fIhh\fR.

**\0**
: the character whose value is \fB0\fR

**\***xyz*
: (where \fIxyz\fR is exactly three octal digits, and is not a \fIback reference\fR (see below)) the character whose octal value is \fB0\fR\fIxyz\fR. The first digit must be in the range 0-3, otherwise the two-digit form is assumed.

**\***xy*
: (where \fIxy\fR is exactly two octal digits, and is not a \fIback reference\fR (see below)) the character whose octal value is \fB0\fR\fIxy\fR


Hexadecimal digits are "**0**-\fB9\fR", "**a**-\fBf\fR", and "**A**-\fBF\fR". Octal digits are "**0**-\fB7\fR".

The character-entry escapes are always taken as ordinary characters. For example, \fB\135\fR is \fB]\fR in Unicode, but \fB\135\fR does not terminate a bracket expression. Beware, however, that some applications (e.g., C compilers and the Tcl interpreter if the regular expression is not quoted with braces) interpret such sequences themselves before the regular-expression package gets to see them, which may require doubling (quadrupling, etc.) the "**\**".

## Class-shorthand escapes

Class-shorthand escapes (AREs only) provide shorthands for certain commonly-used character classes:

**\d**
: **[[:digit:]]**

**\s**
: **[[:space:]]**

**\w**
: **[[:alnum:]_\u203F\u2040\u2054\uFE33\uFE34\uFE4D\uFE4E\uFE4F\uFF3F]** (including punctuation connector characters)

**\D**
: **[^[:digit:]]**

**\S**
: **[^[:space:]]**

**\W**
: **[^[:alnum:]_\u203F\u2040\u2054\uFE33\uFE34\uFE4D\uFE4E\uFE4F\uFF3F]** (including punctuation connector characters)


Within bracket expressions, "**\d**", "**\s**", and "**\w**"\& lose their outer brackets, and "**\D**", "**\S**", and "**\W**"\& are illegal. (So, for example, "**[a-c\d]**" is equivalent to "**[a-c[:digit:]]**". Also, "**[a-c\D]**", which is equivalent to "**[a-c^[:digit:]]**", is illegal.)

## Constraint escapes

A constraint escape (AREs only) is a constraint, matching the empty string if specific conditions are met, written as an escape:

**\A**
: matches only at the beginning of the string (see \fBMATCHING\fR, below, for how this differs from "**^**")

**\m**
: matches only at the beginning of a word

**\M**
: matches only at the end of a word

**\y**
: matches only at the beginning or end of a word

**\Y**
: matches only at a point that is not the beginning or end of a word

**\Z**
: matches only at the end of the string (see \fBMATCHING\fR, below, for how this differs from "**$**")

**\***m*
: (where \fIm\fR is a nonzero digit) a \fIback reference\fR, see below

**\***mnn*
: (where \fIm\fR is a nonzero digit, and \fInn\fR is some more digits, and the decimal value \fImnn\fR is not greater than the number of closing capturing parentheses seen so far) a \fIback reference\fR, see below


A word is defined as in the specification of "**[[:<:]]**" and "**[[:>:]]**" above. Constraint escapes are illegal within bracket expressions.

## Back references

A back reference (AREs only) matches the same string matched by the parenthesized subexpression specified by the number, so that (e.g.) "**([bc])\1**" matches "**bb**" or "**cc**" but not "**bc**". The subexpression must entirely precede the back reference in the RE. Subexpressions are numbered in the order of their leading parentheses. Non-capturing parentheses do not define subexpressions.

There is an inherent historical ambiguity between octal character-entry escapes and back references, which is resolved by heuristics, as hinted at above. A leading zero always indicates an octal escape. A single non-zero digit, not followed by another digit, is always taken as a back reference. A multi-digit sequence not starting with a zero is taken as a back reference if it comes after a suitable subexpression (i.e. the number is in the legal range for a back reference), and otherwise is taken as octal.

# Metasyntax

In addition to the main syntax described above, there are some special forms and miscellaneous syntactic facilities available.

Normally the flavor of RE being used is specified by application-dependent means. However, this can be overridden by a \fIdirector\fR. If an RE of any flavor begins with "*****:**", the rest of the RE is an ARE. If an RE of any flavor begins with "*****=**", the rest of the RE is taken to be a literal string, with all characters considered ordinary characters.

An ARE may begin with \fIembedded options\fR: a sequence \fB(?\fR\fIxyz\fR\fB)\fR (where \fIxyz\fR is one or more alphabetic characters) specifies options affecting the rest of the RE. These supplement, and can override, any options specified by the application. The available option letters are:

**b**
: rest of RE is a BRE

**c**
: case-sensitive matching (usual default)

**e**
: rest of RE is an ERE

**i**
: case-insensitive matching (see \fBMATCHING\fR, below)

**m**
: historical synonym for \fBn\fR

**n**
: newline-sensitive matching (see \fBMATCHING\fR, below)

**p**
: partial newline-sensitive matching (see \fBMATCHING\fR, below)

**q**
: rest of RE is a literal ("quoted") string, all ordinary characters

**s**
: non-newline-sensitive matching (usual default)

**t**
: tight syntax (usual default; see below)

**w**
: inverse partial newline-sensitive ("weird") matching (see \fBMATCHING\fR, below)

**x**
: expanded syntax (see below)


Embedded options take effect at the \fB)\fR terminating the sequence. They are available only at the start of an ARE, and may not be used later within it.

In addition to the usual (\fItight\fR) RE syntax, in which all characters are significant, there is an \fIexpanded\fR syntax, available in all flavors of RE with the \fB-expanded\fR switch, or in AREs with the embedded x option. In the expanded syntax, white-space characters are ignored and all characters between a \fB#\fR and the following newline (or the end of the RE) are ignored, permitting paragraphing and commenting a complex RE. There are three exceptions to that basic rule:

- a white-space character or "**#**" preceded by "**\**" is retained

- white space or "**#**" within a bracket expression is retained

- white space and comments are illegal within multi-character symbols like the ARE "**(?:**" or the BRE "**\(**"


Expanded-syntax white-space characters are blank, tab, newline, and any character that belongs to the \fIspace\fR character class.

Finally, in an ARE, outside bracket expressions, the sequence "**(?#***ttt***)**" (where \fIttt\fR is any text not containing a "**)**") is a comment, completely ignored. Again, this is not allowed between the characters of multi-character symbols like "**(?:**". Such comments are more a historical artifact than a useful facility, and their use is deprecated; use the expanded syntax instead.

*None* of these metasyntax extensions is available if the application (or an initial "*****=**" director) has specified that the user's input be treated as a literal string rather than as an RE.

# Matching

In the event that an RE could match more than one substring of a given string, the RE matches the one starting earliest in the string. If the RE could match more than one substring starting at that point, its choice is determined by its \fIpreference\fR: either the longest substring, or the shortest.

Most atoms, and all constraints, have no preference. A parenthesized RE has the same preference (possibly none) as the RE. A quantified atom with quantifier \fB{\fR\fIm\fR\fB}\fR or \fB{\fR\fIm\fR\fB}?\fR has the same preference (possibly none) as the atom itself. A quantified atom with other normal quantifiers (including \fB{\fR\fIm\fR\fB,\fR\fIn\fR\fB}\fR with \fIm\fR equal to \fIn\fR) prefers longest match. A quantified atom with other non-greedy quantifiers (including \fB{\fR\fIm\fR\fB,\fR\fIn\fR\fB}?\fR with \fIm\fR equal to \fIn\fR) prefers shortest match. A branch has the same preference as the first quantified atom in it which has a preference. An RE consisting of two or more branches connected by the \fB|\fR operator prefers longest match.

Subject to the constraints imposed by the rules for matching the whole RE, subexpressions also match the longest or shortest possible substrings, based on their preferences, with subexpressions starting earlier in the RE taking priority over ones starting later. Note that outer subexpressions thus take priority over their component subexpressions.

The quantifiers \fB{1,1}\fR and \fB{1,1}?\fR can be used to force longest and shortest preference, respectively, on a subexpression or a whole RE.

**NOTE:** This means that you can usually make a RE be non-greedy overall by putting \fB{1,1}?\fR after one of the first non-constraint atoms or parenthesized sub-expressions in it. \fIIt pays to experiment\fR with the placing of this non-greediness override on a suitable range of input texts when you are writing a RE if you are using this level of complexity.

For example, this regular expression is non-greedy, and will match the shortest substring possible given that "**abc**" will be matched as early as possible (the quantifier does not change that):

```
ab{1,1}?c.*x.*cba
```

The atom "**a**" has no greediness preference, we explicitly give one for "**b**", and the remaining quantifiers are overridden to be non-greedy by the preceding non-greedy quantifier.

Match lengths are measured in characters, not collating elements. An empty string is considered longer than no match at all. For example, "**bb***" matches the three middle characters of "**abbbc**", "**(week|wee)(night|knights)**" matches all ten characters of "**weeknights**", when "**(.*).***" is matched against "**abc**" the parenthesized subexpression matches all three characters, and when "**(a*)***" is matched against "**bc**" both the whole RE and the parenthesized subexpression match an empty string.

If case-independent matching is specified, the effect is much as if all case distinctions had vanished from the alphabet. When an alphabetic that exists in multiple cases appears as an ordinary character outside a bracket expression, it is effectively transformed into a bracket expression containing both cases, so that \fBx\fR becomes "**[xX]**". When it appears inside a bracket expression, all case counterparts of it are added to the bracket expression, so that "**[x]**" becomes "**[xX]**" and "**[^x]**" becomes "**[^xX]**".

If newline-sensitive matching is specified, \fB.\fR and bracket expressions using \fB^\fR will never match the newline character (so that matches will never cross newlines unless the RE explicitly arranges it) and \fB^\fR and \fB$\fR will match the empty string after and before a newline respectively, in addition to matching at beginning and end of string respectively. ARE \fB\A\fR and \fB\Z\fR continue to match beginning or end of string \fIonly\fR.

If partial newline-sensitive matching is specified, this affects \fB.\fR and bracket expressions as with newline-sensitive matching, but not \fB^\fR and \fB$\fR.

If inverse partial newline-sensitive matching is specified, this affects \fB^\fR and \fB$\fR as with newline-sensitive matching, but not \fB.\fR and bracket expressions. This is not very useful but is provided for symmetry.

# Limits and compatibility

No particular limit is imposed on the length of REs. Programs intended to be highly portable should not employ REs longer than 256 bytes, as a POSIX-compliant implementation can refuse to accept such REs.

The only feature of AREs that is actually incompatible with POSIX EREs is that \fB\\fR does not lose its special significance inside bracket expressions. All other ARE features use syntax which is illegal or has undefined or unspecified effects in POSIX EREs; the \fB***\fR syntax of directors likewise is outside the POSIX syntax for both BREs and EREs.

Many of the ARE extensions are borrowed from Perl, but some have been changed to clean them up, and a few Perl extensions are not present. Incompatibilities of note include "**\b**", "**\B**", the lack of special treatment for a trailing newline, the addition of complemented bracket expressions to the things affected by newline-sensitive matching, the restrictions on parentheses and back references in lookahead constraints, and the longest/shortest-match (rather than first-match) matching semantics.

The matching rules for REs containing both normal and non-greedy quantifiers have changed since early beta-test versions of this package. (The new rules are much simpler and cleaner, but do not work as hard at guessing the user's real intentions.)

Henry Spencer's original 1986 \fIregexp\fR package, still in widespread use (e.g., in pre-8.1 releases of Tcl), implemented an early version of today's EREs. There are four incompatibilities between \fIregexp\fR's near-EREs ("RREs" for short) and AREs. In roughly increasing order of significance:

- In AREs, \fB\\fR followed by an alphanumeric character is either an escape or an error, while in RREs, it was just another way of writing the alphanumeric. This should not be a problem because there was no reason to write such a sequence in RREs.

- **{** followed by a digit in an ARE is the beginning of a bound, while in RREs, \fB{\fR was always an ordinary character. Such sequences should be rare, and will often result in an error because following characters will not look like a valid bound.

- In AREs, \fB\\fR remains a special character within "**[\|]**", so a literal \fB\\fR within \fB[\|]\fR must be written "**\\**". \fB\\\fR also gives a literal \fB\\fR within \fB[\|]\fR in RREs, but only truly paranoid programmers routinely doubled the backslash.

- AREs report the longest/shortest match for the RE, rather than the first found in a specified search order. This may affect some RREs which were written in the expectation that the first match would be reported. (The careful crafting of RREs to optimize the search order for fast matching is obsolete (AREs examine all possible matches in parallel, and their performance is largely insensitive to their complexity) but cases where the search order was exploited to deliberately find a match which was \fInot\fR the longest/shortest will need rewriting.)


# Basic regular expressions

BREs differ from EREs in several respects. "**|**", "**+**", and \fB?\fR are ordinary characters and there is no equivalent for their functionality. The delimiters for bounds are \fB\{\fR and "**\}**", with \fB{\fR and \fB}\fR by themselves ordinary characters. The parentheses for nested subexpressions are \fB\(\fR and "**\)**", with \fB(\fR and \fB)\fR by themselves ordinary characters. \fB^\fR is an ordinary character except at the beginning of the RE or the beginning of a parenthesized subexpression, \fB$\fR is an ordinary character except at the end of the RE or the end of a parenthesized subexpression, and \fB*\fR is an ordinary character if it appears at the beginning of the RE or the beginning of a parenthesized subexpression (after a possible leading "**^**"). Finally, single-digit back references are available, and \fB\<\fR and \fB\>\fR are synonyms for "**[[:<:]]**" and "**[[:>:]]**" respectively; no other escapes are available.

