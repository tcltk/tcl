/*
 * chr.h --
 *
 * 	Regexp package file:  Unichar version of stuff related to the
 * 	nature of a character.
 *
 * Copyright (c) 1998 Henry Spencer.  All rights reserved.
 * 
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., and Sun Microsystems Inc., none of
 * whom are responsible for the results.  The author thanks all of them.
 * 
 * Redistribution and use in source and binary forms -- with or without
 * modification -- are permitted for any purpose, provided that
 * redistributions in source form retain this entire copyright notice and
 * indicate the origin and nature of any modifications. 
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * HENRY SPENCER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) chr.h 1.4 98/01/21 14:32:38
 */

typedef Tcl_UniChar chr;	/* internal character type */
typedef int pchr;		/* what it promotes to */
typedef unsigned uchr;		/* unsigned type big enough to hold a chr */
#define	CHRBITS	(sizeof(Tcl_UniChar) * CHAR_BIT)	/* bits in a chr */
#define	CHR(c)	(UCHAR(c))		/* turn a char literal into a chr literal */
#define	DIGITVAL(c)	((c)-'0')	/* turn a chr digit into its value */

/*
 * char names for the externally-visible functions
 */
#define	compile	re_ucomp
#define	exec	re_uexec
