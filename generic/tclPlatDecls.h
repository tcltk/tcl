/*
 * tclPlatDecls.h --
 *
 *	Declarations of platform specific Tcl APIs.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * All rights reserved.
 *
 * RCS: @(#) $Id: tclPlatDecls.h,v 1.30 2008/04/08 14:54:53 das Exp $
 */

#ifndef _TCLPLATDECLS
#define _TCLPLATDECLS

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tcl
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_TCL_STUBS
#      define TCL_STORAGE_CLASS
#   else
#      define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

/*
 *  Pull in the typedef of TCHAR for windows.
 */
#if defined(__CYGWIN__)
    typedef char TCHAR;
#elif defined(__WIN32__) && !defined(_TCHAR_DEFINED)
#   include <tchar.h>
#   ifndef _TCHAR_DEFINED
	/* Borland seems to forget to set this. */
        typedef _TCHAR TCHAR;
#	define _TCHAR_DEFINED
#   endif
#   if defined(_MSC_VER) && defined(__STDC__)
	/* MSVC++ misses this. */
	typedef _TCHAR TCHAR;
#   endif
#endif

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

#ifdef __WIN32__ /* WIN */
/* 0 */
EXTERN TCHAR *		Tcl_WinUtfToTChar(const char *str, int len,
				Tcl_DString *dsPtr);
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
/* 0 */
EXTERN int		Tcl_MacOSXOpenBundleResources(Tcl_Interp *interp,
				const char *bundleName, int hasResourceFile,
				int maxPathLen, char *libraryPath);
#endif /* MACOSX */
#ifdef __WIN32__ /* WIN */
/* 1 */
EXTERN char *		Tcl_WinTCharToUtf(const TCHAR *str, int len,
				Tcl_DString *dsPtr);
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
/* 1 */
EXTERN int		Tcl_MacOSXOpenVersionedBundleResources(
				Tcl_Interp *interp, const char *bundleName,
				const char *bundleVersion,
				int hasResourceFile, int maxPathLen,
				char *libraryPath);
#endif /* MACOSX */
/* Slot 2 is reserved */
/* Slot 3 is reserved */
/* Slot 4 is reserved */
/* Slot 5 is reserved */
/* Slot 6 is reserved */
/* Slot 7 is reserved */
/* Slot 8 is reserved */
/* Slot 9 is reserved */
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
/* Slot 13 is reserved */
/* Slot 14 is reserved */
/* Slot 15 is reserved */
/* Slot 16 is reserved */
/* Slot 17 is reserved */
/* Slot 18 is reserved */
/* Slot 19 is reserved */
/* Slot 20 is reserved */
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
/* Slot 24 is reserved */
/* Slot 25 is reserved */
/* Slot 26 is reserved */
/* Slot 27 is reserved */
/* Slot 28 is reserved */
/* Slot 29 is reserved */
/* Slot 30 is reserved */
/* Slot 31 is reserved */
/* Slot 32 is reserved */
/* Slot 33 is reserved */
/* Slot 34 is reserved */
/* Slot 35 is reserved */
/* Slot 36 is reserved */
/* Slot 37 is reserved */
/* Slot 38 is reserved */
/* Slot 39 is reserved */
/* Slot 40 is reserved */
/* Slot 41 is reserved */
/* Slot 42 is reserved */
/* Slot 43 is reserved */
/* Slot 44 is reserved */
/* Slot 45 is reserved */
/* Slot 46 is reserved */
/* Slot 47 is reserved */
/* Slot 48 is reserved */
/* Slot 49 is reserved */
/* Slot 50 is reserved */
/* Slot 51 is reserved */
/* Slot 52 is reserved */
/* Slot 53 is reserved */
/* Slot 54 is reserved */
/* Slot 55 is reserved */
/* Slot 56 is reserved */
/* Slot 57 is reserved */
/* Slot 58 is reserved */
/* Slot 59 is reserved */
/* Slot 60 is reserved */
/* Slot 61 is reserved */
/* Slot 62 is reserved */
/* Slot 63 is reserved */
/* Slot 64 is reserved */
/* Slot 65 is reserved */
/* Slot 66 is reserved */
/* Slot 67 is reserved */
/* Slot 68 is reserved */
/* Slot 69 is reserved */
/* Slot 70 is reserved */
/* Slot 71 is reserved */
/* Slot 72 is reserved */
/* Slot 73 is reserved */
/* Slot 74 is reserved */
/* Slot 75 is reserved */
/* Slot 76 is reserved */
/* Slot 77 is reserved */
/* Slot 78 is reserved */
/* Slot 79 is reserved */
/* Slot 80 is reserved */
/* Slot 81 is reserved */
/* Slot 82 is reserved */
/* Slot 83 is reserved */
/* Slot 84 is reserved */
/* Slot 85 is reserved */
/* Slot 86 is reserved */
/* Slot 87 is reserved */
/* Slot 88 is reserved */
/* Slot 89 is reserved */
/* Slot 90 is reserved */
/* Slot 91 is reserved */
/* Slot 92 is reserved */
/* Slot 93 is reserved */
/* Slot 94 is reserved */
/* Slot 95 is reserved */
/* Slot 96 is reserved */
/* Slot 97 is reserved */
/* Slot 98 is reserved */
/* Slot 99 is reserved */
/* Slot 100 is reserved */
/* Slot 101 is reserved */
/* Slot 102 is reserved */
/* Slot 103 is reserved */
/* Slot 104 is reserved */
/* Slot 105 is reserved */
/* Slot 106 is reserved */
/* Slot 107 is reserved */
/* Slot 108 is reserved */
/* Slot 109 is reserved */
/* Slot 110 is reserved */
/* Slot 111 is reserved */
/* Slot 112 is reserved */
/* Slot 113 is reserved */
/* Slot 114 is reserved */
/* Slot 115 is reserved */
/* Slot 116 is reserved */
/* Slot 117 is reserved */
/* Slot 118 is reserved */
/* Slot 119 is reserved */
/* Slot 120 is reserved */
/* Slot 121 is reserved */
/* Slot 122 is reserved */
/* Slot 123 is reserved */
/* Slot 124 is reserved */
/* Slot 125 is reserved */
/* Slot 126 is reserved */
/* Slot 127 is reserved */
/* Slot 128 is reserved */
/* Slot 129 is reserved */
/* Slot 130 is reserved */
/* Slot 131 is reserved */
/* Slot 132 is reserved */
/* Slot 133 is reserved */
/* Slot 134 is reserved */
/* Slot 135 is reserved */
/* Slot 136 is reserved */
/* Slot 137 is reserved */
/* Slot 138 is reserved */
/* Slot 139 is reserved */
/* Slot 140 is reserved */
/* Slot 141 is reserved */
/* Slot 142 is reserved */
/* Slot 143 is reserved */
/* Slot 144 is reserved */
/* Slot 145 is reserved */
/* Slot 146 is reserved */
/* Slot 147 is reserved */
/* Slot 148 is reserved */
/* Slot 149 is reserved */
/* Slot 150 is reserved */
/* Slot 151 is reserved */
/* Slot 152 is reserved */
/* Slot 153 is reserved */
/* Slot 154 is reserved */
/* Slot 155 is reserved */
/* Slot 156 is reserved */
/* Slot 157 is reserved */
/* Slot 158 is reserved */
/* Slot 159 is reserved */
/* Slot 160 is reserved */
/* Slot 161 is reserved */
/* Slot 162 is reserved */
/* Slot 163 is reserved */
/* Slot 164 is reserved */
/* Slot 165 is reserved */
/* Slot 166 is reserved */
/* Slot 167 is reserved */
/* Slot 168 is reserved */
/* Slot 169 is reserved */
/* Slot 170 is reserved */
/* Slot 171 is reserved */
/* Slot 172 is reserved */
/* Slot 173 is reserved */
/* Slot 174 is reserved */
/* Slot 175 is reserved */
/* Slot 176 is reserved */
/* Slot 177 is reserved */
/* Slot 178 is reserved */
/* Slot 179 is reserved */
/* Slot 180 is reserved */
/* Slot 181 is reserved */
/* Slot 182 is reserved */
/* Slot 183 is reserved */
/* Slot 184 is reserved */
/* Slot 185 is reserved */
/* Slot 186 is reserved */
/* Slot 187 is reserved */
/* Slot 188 is reserved */
/* Slot 189 is reserved */
/* Slot 190 is reserved */
/* Slot 191 is reserved */
/* Slot 192 is reserved */
/* Slot 193 is reserved */
/* Slot 194 is reserved */
/* Slot 195 is reserved */
/* Slot 196 is reserved */
/* Slot 197 is reserved */
/* Slot 198 is reserved */
/* Slot 199 is reserved */
/* Slot 200 is reserved */
/* Slot 201 is reserved */
/* Slot 202 is reserved */
/* Slot 203 is reserved */
/* Slot 204 is reserved */
/* Slot 205 is reserved */
/* Slot 206 is reserved */
/* Slot 207 is reserved */
/* Slot 208 is reserved */
/* Slot 209 is reserved */
/* Slot 210 is reserved */
/* Slot 211 is reserved */
/* Slot 212 is reserved */
/* Slot 213 is reserved */
/* Slot 214 is reserved */
/* Slot 215 is reserved */
/* Slot 216 is reserved */
/* Slot 217 is reserved */
/* Slot 218 is reserved */
/* Slot 219 is reserved */
/* Slot 220 is reserved */
/* Slot 221 is reserved */
/* Slot 222 is reserved */
/* Slot 223 is reserved */
/* Slot 224 is reserved */
/* Slot 225 is reserved */
/* Slot 226 is reserved */
/* Slot 227 is reserved */
/* Slot 228 is reserved */
/* Slot 229 is reserved */
/* Slot 230 is reserved */
/* Slot 231 is reserved */
/* Slot 232 is reserved */
/* Slot 233 is reserved */
/* Slot 234 is reserved */
/* Slot 235 is reserved */
/* Slot 236 is reserved */
/* Slot 237 is reserved */
/* Slot 238 is reserved */
/* Slot 239 is reserved */
/* Slot 240 is reserved */
/* Slot 241 is reserved */
/* Slot 242 is reserved */
/* Slot 243 is reserved */
/* Slot 244 is reserved */
/* Slot 245 is reserved */
/* Slot 246 is reserved */
/* Slot 247 is reserved */
/* Slot 248 is reserved */
/* Slot 249 is reserved */
/* Slot 250 is reserved */
/* Slot 251 is reserved */
/* Slot 252 is reserved */
/* Slot 253 is reserved */
/* Slot 254 is reserved */
/* Slot 255 is reserved */
/* Slot 256 is reserved */
/* Slot 257 is reserved */
/* Slot 258 is reserved */
/* Slot 259 is reserved */
/* Slot 260 is reserved */
/* Slot 261 is reserved */
/* Slot 262 is reserved */
/* Slot 263 is reserved */
/* Slot 264 is reserved */
/* Slot 265 is reserved */
/* Slot 266 is reserved */
/* Slot 267 is reserved */
/* Slot 268 is reserved */
/* Slot 269 is reserved */
/* Slot 270 is reserved */
/* Slot 271 is reserved */
/* Slot 272 is reserved */
/* Slot 273 is reserved */
/* Slot 274 is reserved */
/* Slot 275 is reserved */
/* Slot 276 is reserved */
/* Slot 277 is reserved */
/* Slot 278 is reserved */
/* Slot 279 is reserved */
/* Slot 280 is reserved */
/* Slot 281 is reserved */
/* Slot 282 is reserved */
/* Slot 283 is reserved */
/* Slot 284 is reserved */
/* Slot 285 is reserved */
/* Slot 286 is reserved */
/* Slot 287 is reserved */
/* Slot 288 is reserved */
/* Slot 289 is reserved */
/* Slot 290 is reserved */
/* Slot 291 is reserved */
/* Slot 292 is reserved */
/* Slot 293 is reserved */
/* Slot 294 is reserved */
/* Slot 295 is reserved */
/* Slot 296 is reserved */
/* Slot 297 is reserved */
/* Slot 298 is reserved */
/* Slot 299 is reserved */
/* Slot 300 is reserved */
/* Slot 301 is reserved */
/* Slot 302 is reserved */
/* Slot 303 is reserved */
/* Slot 304 is reserved */
/* Slot 305 is reserved */
/* Slot 306 is reserved */
/* Slot 307 is reserved */
/* Slot 308 is reserved */
/* Slot 309 is reserved */
/* Slot 310 is reserved */
/* Slot 311 is reserved */
/* Slot 312 is reserved */
/* Slot 313 is reserved */
/* Slot 314 is reserved */
/* Slot 315 is reserved */
/* Slot 316 is reserved */
/* Slot 317 is reserved */
/* Slot 318 is reserved */
/* Slot 319 is reserved */
/* Slot 320 is reserved */
/* Slot 321 is reserved */
/* Slot 322 is reserved */
/* Slot 323 is reserved */
/* Slot 324 is reserved */
/* Slot 325 is reserved */
/* Slot 326 is reserved */
/* Slot 327 is reserved */
/* Slot 328 is reserved */
/* Slot 329 is reserved */
/* Slot 330 is reserved */
/* Slot 331 is reserved */
/* Slot 332 is reserved */
/* Slot 333 is reserved */
/* Slot 334 is reserved */
/* Slot 335 is reserved */
/* Slot 336 is reserved */
/* Slot 337 is reserved */
/* Slot 338 is reserved */
/* Slot 339 is reserved */
/* Slot 340 is reserved */
/* Slot 341 is reserved */
/* Slot 342 is reserved */
/* Slot 343 is reserved */
/* Slot 344 is reserved */
/* Slot 345 is reserved */
/* Slot 346 is reserved */
/* Slot 347 is reserved */
/* Slot 348 is reserved */
/* Slot 349 is reserved */
/* Slot 350 is reserved */
/* Slot 351 is reserved */
/* Slot 352 is reserved */
/* Slot 353 is reserved */
/* Slot 354 is reserved */
/* Slot 355 is reserved */
/* Slot 356 is reserved */
/* Slot 357 is reserved */
/* Slot 358 is reserved */
/* Slot 359 is reserved */
/* Slot 360 is reserved */
/* Slot 361 is reserved */
/* Slot 362 is reserved */
/* Slot 363 is reserved */
/* Slot 364 is reserved */
/* Slot 365 is reserved */
/* Slot 366 is reserved */
/* Slot 367 is reserved */
/* Slot 368 is reserved */
/* Slot 369 is reserved */
/* Slot 370 is reserved */
/* Slot 371 is reserved */
/* Slot 372 is reserved */
/* Slot 373 is reserved */
/* Slot 374 is reserved */
/* Slot 375 is reserved */
/* Slot 376 is reserved */
/* Slot 377 is reserved */
/* Slot 378 is reserved */
/* Slot 379 is reserved */
/* Slot 380 is reserved */
/* Slot 381 is reserved */
/* Slot 382 is reserved */
/* Slot 383 is reserved */
/* Slot 384 is reserved */
/* Slot 385 is reserved */
/* Slot 386 is reserved */
/* Slot 387 is reserved */
/* Slot 388 is reserved */
/* Slot 389 is reserved */
/* Slot 390 is reserved */
/* Slot 391 is reserved */
/* Slot 392 is reserved */
/* Slot 393 is reserved */
/* Slot 394 is reserved */
/* Slot 395 is reserved */
/* Slot 396 is reserved */
/* Slot 397 is reserved */
/* Slot 398 is reserved */
/* Slot 399 is reserved */
/* Slot 400 is reserved */
/* Slot 401 is reserved */
/* Slot 402 is reserved */
/* Slot 403 is reserved */
/* Slot 404 is reserved */
/* Slot 405 is reserved */
/* Slot 406 is reserved */
/* Slot 407 is reserved */
/* Slot 408 is reserved */
/* Slot 409 is reserved */
/* Slot 410 is reserved */
/* Slot 411 is reserved */
/* Slot 412 is reserved */
/* Slot 413 is reserved */
/* Slot 414 is reserved */
/* Slot 415 is reserved */
/* Slot 416 is reserved */
/* Slot 417 is reserved */
/* Slot 418 is reserved */
/* Slot 419 is reserved */
/* Slot 420 is reserved */
/* Slot 421 is reserved */
/* Slot 422 is reserved */
/* Slot 423 is reserved */
/* Slot 424 is reserved */
/* Slot 425 is reserved */
/* Slot 426 is reserved */
/* Slot 427 is reserved */
/* Slot 428 is reserved */
/* Slot 429 is reserved */
/* Slot 430 is reserved */
/* Slot 431 is reserved */
/* Slot 432 is reserved */
/* Slot 433 is reserved */
/* Slot 434 is reserved */
/* Slot 435 is reserved */
/* Slot 436 is reserved */
/* Slot 437 is reserved */
/* Slot 438 is reserved */
/* Slot 439 is reserved */
/* Slot 440 is reserved */
/* Slot 441 is reserved */
/* Slot 442 is reserved */
/* Slot 443 is reserved */
/* Slot 444 is reserved */
/* Slot 445 is reserved */
/* Slot 446 is reserved */
/* Slot 447 is reserved */
/* Slot 448 is reserved */
/* Slot 449 is reserved */
/* Slot 450 is reserved */
/* Slot 451 is reserved */
/* Slot 452 is reserved */
/* Slot 453 is reserved */
/* Slot 454 is reserved */
/* Slot 455 is reserved */
/* Slot 456 is reserved */
/* Slot 457 is reserved */
/* Slot 458 is reserved */
/* Slot 459 is reserved */
/* Slot 460 is reserved */
/* Slot 461 is reserved */
/* Slot 462 is reserved */
/* Slot 463 is reserved */
/* Slot 464 is reserved */
/* Slot 465 is reserved */
/* Slot 466 is reserved */
/* Slot 467 is reserved */
/* Slot 468 is reserved */
/* Slot 469 is reserved */
/* Slot 470 is reserved */
/* Slot 471 is reserved */
/* Slot 472 is reserved */
/* Slot 473 is reserved */
/* Slot 474 is reserved */
/* Slot 475 is reserved */
/* Slot 476 is reserved */
/* Slot 477 is reserved */
/* Slot 478 is reserved */
/* Slot 479 is reserved */
/* Slot 480 is reserved */
/* Slot 481 is reserved */
/* Slot 482 is reserved */
/* Slot 483 is reserved */
/* Slot 484 is reserved */
/* Slot 485 is reserved */
/* Slot 486 is reserved */
/* Slot 487 is reserved */
/* Slot 488 is reserved */
/* Slot 489 is reserved */
/* Slot 490 is reserved */
/* Slot 491 is reserved */
/* Slot 492 is reserved */
/* Slot 493 is reserved */
/* Slot 494 is reserved */
/* Slot 495 is reserved */
/* Slot 496 is reserved */
/* Slot 497 is reserved */
/* Slot 498 is reserved */
/* Slot 499 is reserved */
/* Slot 500 is reserved */
/* Slot 501 is reserved */
/* Slot 502 is reserved */
/* Slot 503 is reserved */
/* Slot 504 is reserved */
/* Slot 505 is reserved */
/* Slot 506 is reserved */
/* Slot 507 is reserved */
/* Slot 508 is reserved */
/* Slot 509 is reserved */
/* Slot 510 is reserved */
/* Slot 511 is reserved */
/* Slot 512 is reserved */
/* Slot 513 is reserved */
/* Slot 514 is reserved */
/* Slot 515 is reserved */
/* Slot 516 is reserved */
/* Slot 517 is reserved */
/* Slot 518 is reserved */
/* Slot 519 is reserved */
/* Slot 520 is reserved */
/* Slot 521 is reserved */
/* Slot 522 is reserved */
/* Slot 523 is reserved */
/* Slot 524 is reserved */
/* Slot 525 is reserved */
/* Slot 526 is reserved */
/* Slot 527 is reserved */
/* Slot 528 is reserved */
/* Slot 529 is reserved */
/* Slot 530 is reserved */
/* Slot 531 is reserved */
/* Slot 532 is reserved */
/* Slot 533 is reserved */
/* Slot 534 is reserved */
/* Slot 535 is reserved */
/* Slot 536 is reserved */
/* Slot 537 is reserved */
/* Slot 538 is reserved */
/* Slot 539 is reserved */
/* Slot 540 is reserved */
/* Slot 541 is reserved */
/* Slot 542 is reserved */
/* Slot 543 is reserved */
/* Slot 544 is reserved */
/* Slot 545 is reserved */
/* Slot 546 is reserved */
/* Slot 547 is reserved */
/* Slot 548 is reserved */
/* Slot 549 is reserved */
/* Slot 550 is reserved */
/* Slot 551 is reserved */
/* Slot 552 is reserved */
/* Slot 553 is reserved */
/* Slot 554 is reserved */
/* Slot 555 is reserved */
/* Slot 556 is reserved */
/* Slot 557 is reserved */
/* Slot 558 is reserved */
/* Slot 559 is reserved */
/* Slot 560 is reserved */
/* Slot 561 is reserved */
/* Slot 562 is reserved */
/* Slot 563 is reserved */
/* Slot 564 is reserved */
/* Slot 565 is reserved */
/* Slot 566 is reserved */
/* Slot 567 is reserved */
/* Slot 568 is reserved */
/* Slot 569 is reserved */
/* Slot 570 is reserved */
/* Slot 571 is reserved */
/* Slot 572 is reserved */
/* Slot 573 is reserved */
/* Slot 574 is reserved */
/* Slot 575 is reserved */
/* Slot 576 is reserved */
/* Slot 577 is reserved */
/* Slot 578 is reserved */
/* Slot 579 is reserved */
/* Slot 580 is reserved */
/* Slot 581 is reserved */
/* Slot 582 is reserved */
/* Slot 583 is reserved */
/* Slot 584 is reserved */
/* Slot 585 is reserved */
/* Slot 586 is reserved */
/* Slot 587 is reserved */
/* Slot 588 is reserved */
/* Slot 589 is reserved */
/* Slot 590 is reserved */
/* Slot 591 is reserved */
/* Slot 592 is reserved */
/* Slot 593 is reserved */
/* Slot 594 is reserved */
/* Slot 595 is reserved */
/* Slot 596 is reserved */
/* Slot 597 is reserved */
/* Slot 598 is reserved */
/* Slot 599 is reserved */
/* Slot 600 is reserved */
/* Slot 601 is reserved */
/* Slot 602 is reserved */
/* Slot 603 is reserved */
/* 604 */
EXTERN int		Tcl_ParseArgsObjv(Tcl_Interp *interp,
				const Tcl_ArgvInfo *argTable, int *objcPtr,
				Tcl_Obj *const *objv, Tcl_Obj ***remObjv);

typedef struct TclPlatStubs {
    int magic;
    const struct TclPlatStubHooks *hooks;

#if !defined(__WIN32__) && !defined(MAC_OSX_TCL) /* UNIX */
    void (*reserved0)(void);
#endif /* UNIX */
#ifdef __WIN32__ /* WIN */
    TCHAR * (*tcl_WinUtfToTChar) (const char *str, int len, Tcl_DString *dsPtr); /* 0 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    int (*tcl_MacOSXOpenBundleResources) (Tcl_Interp *interp, const char *bundleName, int hasResourceFile, int maxPathLen, char *libraryPath); /* 0 */
#endif /* MACOSX */
#if !defined(__WIN32__) && !defined(MAC_OSX_TCL) /* UNIX */
    void (*reserved1)(void);
#endif /* UNIX */
#ifdef __WIN32__ /* WIN */
    char * (*tcl_WinTCharToUtf) (const TCHAR *str, int len, Tcl_DString *dsPtr); /* 1 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
    int (*tcl_MacOSXOpenVersionedBundleResources) (Tcl_Interp *interp, const char *bundleName, const char *bundleVersion, int hasResourceFile, int maxPathLen, char *libraryPath); /* 1 */
#endif /* MACOSX */
    void (*reserved2)(void);
    void (*reserved3)(void);
    void (*reserved4)(void);
    void (*reserved5)(void);
    void (*reserved6)(void);
    void (*reserved7)(void);
    void (*reserved8)(void);
    void (*reserved9)(void);
    void (*reserved10)(void);
    void (*reserved11)(void);
    void (*reserved12)(void);
    void (*reserved13)(void);
    void (*reserved14)(void);
    void (*reserved15)(void);
    void (*reserved16)(void);
    void (*reserved17)(void);
    void (*reserved18)(void);
    void (*reserved19)(void);
    void (*reserved20)(void);
    void (*reserved21)(void);
    void (*reserved22)(void);
    void (*reserved23)(void);
    void (*reserved24)(void);
    void (*reserved25)(void);
    void (*reserved26)(void);
    void (*reserved27)(void);
    void (*reserved28)(void);
    void (*reserved29)(void);
    void (*reserved30)(void);
    void (*reserved31)(void);
    void (*reserved32)(void);
    void (*reserved33)(void);
    void (*reserved34)(void);
    void (*reserved35)(void);
    void (*reserved36)(void);
    void (*reserved37)(void);
    void (*reserved38)(void);
    void (*reserved39)(void);
    void (*reserved40)(void);
    void (*reserved41)(void);
    void (*reserved42)(void);
    void (*reserved43)(void);
    void (*reserved44)(void);
    void (*reserved45)(void);
    void (*reserved46)(void);
    void (*reserved47)(void);
    void (*reserved48)(void);
    void (*reserved49)(void);
    void (*reserved50)(void);
    void (*reserved51)(void);
    void (*reserved52)(void);
    void (*reserved53)(void);
    void (*reserved54)(void);
    void (*reserved55)(void);
    void (*reserved56)(void);
    void (*reserved57)(void);
    void (*reserved58)(void);
    void (*reserved59)(void);
    void (*reserved60)(void);
    void (*reserved61)(void);
    void (*reserved62)(void);
    void (*reserved63)(void);
    void (*reserved64)(void);
    void (*reserved65)(void);
    void (*reserved66)(void);
    void (*reserved67)(void);
    void (*reserved68)(void);
    void (*reserved69)(void);
    void (*reserved70)(void);
    void (*reserved71)(void);
    void (*reserved72)(void);
    void (*reserved73)(void);
    void (*reserved74)(void);
    void (*reserved75)(void);
    void (*reserved76)(void);
    void (*reserved77)(void);
    void (*reserved78)(void);
    void (*reserved79)(void);
    void (*reserved80)(void);
    void (*reserved81)(void);
    void (*reserved82)(void);
    void (*reserved83)(void);
    void (*reserved84)(void);
    void (*reserved85)(void);
    void (*reserved86)(void);
    void (*reserved87)(void);
    void (*reserved88)(void);
    void (*reserved89)(void);
    void (*reserved90)(void);
    void (*reserved91)(void);
    void (*reserved92)(void);
    void (*reserved93)(void);
    void (*reserved94)(void);
    void (*reserved95)(void);
    void (*reserved96)(void);
    void (*reserved97)(void);
    void (*reserved98)(void);
    void (*reserved99)(void);
    void (*reserved100)(void);
    void (*reserved101)(void);
    void (*reserved102)(void);
    void (*reserved103)(void);
    void (*reserved104)(void);
    void (*reserved105)(void);
    void (*reserved106)(void);
    void (*reserved107)(void);
    void (*reserved108)(void);
    void (*reserved109)(void);
    void (*reserved110)(void);
    void (*reserved111)(void);
    void (*reserved112)(void);
    void (*reserved113)(void);
    void (*reserved114)(void);
    void (*reserved115)(void);
    void (*reserved116)(void);
    void (*reserved117)(void);
    void (*reserved118)(void);
    void (*reserved119)(void);
    void (*reserved120)(void);
    void (*reserved121)(void);
    void (*reserved122)(void);
    void (*reserved123)(void);
    void (*reserved124)(void);
    void (*reserved125)(void);
    void (*reserved126)(void);
    void (*reserved127)(void);
    void (*reserved128)(void);
    void (*reserved129)(void);
    void (*reserved130)(void);
    void (*reserved131)(void);
    void (*reserved132)(void);
    void (*reserved133)(void);
    void (*reserved134)(void);
    void (*reserved135)(void);
    void (*reserved136)(void);
    void (*reserved137)(void);
    void (*reserved138)(void);
    void (*reserved139)(void);
    void (*reserved140)(void);
    void (*reserved141)(void);
    void (*reserved142)(void);
    void (*reserved143)(void);
    void (*reserved144)(void);
    void (*reserved145)(void);
    void (*reserved146)(void);
    void (*reserved147)(void);
    void (*reserved148)(void);
    void (*reserved149)(void);
    void (*reserved150)(void);
    void (*reserved151)(void);
    void (*reserved152)(void);
    void (*reserved153)(void);
    void (*reserved154)(void);
    void (*reserved155)(void);
    void (*reserved156)(void);
    void (*reserved157)(void);
    void (*reserved158)(void);
    void (*reserved159)(void);
    void (*reserved160)(void);
    void (*reserved161)(void);
    void (*reserved162)(void);
    void (*reserved163)(void);
    void (*reserved164)(void);
    void (*reserved165)(void);
    void (*reserved166)(void);
    void (*reserved167)(void);
    void (*reserved168)(void);
    void (*reserved169)(void);
    void (*reserved170)(void);
    void (*reserved171)(void);
    void (*reserved172)(void);
    void (*reserved173)(void);
    void (*reserved174)(void);
    void (*reserved175)(void);
    void (*reserved176)(void);
    void (*reserved177)(void);
    void (*reserved178)(void);
    void (*reserved179)(void);
    void (*reserved180)(void);
    void (*reserved181)(void);
    void (*reserved182)(void);
    void (*reserved183)(void);
    void (*reserved184)(void);
    void (*reserved185)(void);
    void (*reserved186)(void);
    void (*reserved187)(void);
    void (*reserved188)(void);
    void (*reserved189)(void);
    void (*reserved190)(void);
    void (*reserved191)(void);
    void (*reserved192)(void);
    void (*reserved193)(void);
    void (*reserved194)(void);
    void (*reserved195)(void);
    void (*reserved196)(void);
    void (*reserved197)(void);
    void (*reserved198)(void);
    void (*reserved199)(void);
    void (*reserved200)(void);
    void (*reserved201)(void);
    void (*reserved202)(void);
    void (*reserved203)(void);
    void (*reserved204)(void);
    void (*reserved205)(void);
    void (*reserved206)(void);
    void (*reserved207)(void);
    void (*reserved208)(void);
    void (*reserved209)(void);
    void (*reserved210)(void);
    void (*reserved211)(void);
    void (*reserved212)(void);
    void (*reserved213)(void);
    void (*reserved214)(void);
    void (*reserved215)(void);
    void (*reserved216)(void);
    void (*reserved217)(void);
    void (*reserved218)(void);
    void (*reserved219)(void);
    void (*reserved220)(void);
    void (*reserved221)(void);
    void (*reserved222)(void);
    void (*reserved223)(void);
    void (*reserved224)(void);
    void (*reserved225)(void);
    void (*reserved226)(void);
    void (*reserved227)(void);
    void (*reserved228)(void);
    void (*reserved229)(void);
    void (*reserved230)(void);
    void (*reserved231)(void);
    void (*reserved232)(void);
    void (*reserved233)(void);
    void (*reserved234)(void);
    void (*reserved235)(void);
    void (*reserved236)(void);
    void (*reserved237)(void);
    void (*reserved238)(void);
    void (*reserved239)(void);
    void (*reserved240)(void);
    void (*reserved241)(void);
    void (*reserved242)(void);
    void (*reserved243)(void);
    void (*reserved244)(void);
    void (*reserved245)(void);
    void (*reserved246)(void);
    void (*reserved247)(void);
    void (*reserved248)(void);
    void (*reserved249)(void);
    void (*reserved250)(void);
    void (*reserved251)(void);
    void (*reserved252)(void);
    void (*reserved253)(void);
    void (*reserved254)(void);
    void (*reserved255)(void);
    void (*reserved256)(void);
    void (*reserved257)(void);
    void (*reserved258)(void);
    void (*reserved259)(void);
    void (*reserved260)(void);
    void (*reserved261)(void);
    void (*reserved262)(void);
    void (*reserved263)(void);
    void (*reserved264)(void);
    void (*reserved265)(void);
    void (*reserved266)(void);
    void (*reserved267)(void);
    void (*reserved268)(void);
    void (*reserved269)(void);
    void (*reserved270)(void);
    void (*reserved271)(void);
    void (*reserved272)(void);
    void (*reserved273)(void);
    void (*reserved274)(void);
    void (*reserved275)(void);
    void (*reserved276)(void);
    void (*reserved277)(void);
    void (*reserved278)(void);
    void (*reserved279)(void);
    void (*reserved280)(void);
    void (*reserved281)(void);
    void (*reserved282)(void);
    void (*reserved283)(void);
    void (*reserved284)(void);
    void (*reserved285)(void);
    void (*reserved286)(void);
    void (*reserved287)(void);
    void (*reserved288)(void);
    void (*reserved289)(void);
    void (*reserved290)(void);
    void (*reserved291)(void);
    void (*reserved292)(void);
    void (*reserved293)(void);
    void (*reserved294)(void);
    void (*reserved295)(void);
    void (*reserved296)(void);
    void (*reserved297)(void);
    void (*reserved298)(void);
    void (*reserved299)(void);
    void (*reserved300)(void);
    void (*reserved301)(void);
    void (*reserved302)(void);
    void (*reserved303)(void);
    void (*reserved304)(void);
    void (*reserved305)(void);
    void (*reserved306)(void);
    void (*reserved307)(void);
    void (*reserved308)(void);
    void (*reserved309)(void);
    void (*reserved310)(void);
    void (*reserved311)(void);
    void (*reserved312)(void);
    void (*reserved313)(void);
    void (*reserved314)(void);
    void (*reserved315)(void);
    void (*reserved316)(void);
    void (*reserved317)(void);
    void (*reserved318)(void);
    void (*reserved319)(void);
    void (*reserved320)(void);
    void (*reserved321)(void);
    void (*reserved322)(void);
    void (*reserved323)(void);
    void (*reserved324)(void);
    void (*reserved325)(void);
    void (*reserved326)(void);
    void (*reserved327)(void);
    void (*reserved328)(void);
    void (*reserved329)(void);
    void (*reserved330)(void);
    void (*reserved331)(void);
    void (*reserved332)(void);
    void (*reserved333)(void);
    void (*reserved334)(void);
    void (*reserved335)(void);
    void (*reserved336)(void);
    void (*reserved337)(void);
    void (*reserved338)(void);
    void (*reserved339)(void);
    void (*reserved340)(void);
    void (*reserved341)(void);
    void (*reserved342)(void);
    void (*reserved343)(void);
    void (*reserved344)(void);
    void (*reserved345)(void);
    void (*reserved346)(void);
    void (*reserved347)(void);
    void (*reserved348)(void);
    void (*reserved349)(void);
    void (*reserved350)(void);
    void (*reserved351)(void);
    void (*reserved352)(void);
    void (*reserved353)(void);
    void (*reserved354)(void);
    void (*reserved355)(void);
    void (*reserved356)(void);
    void (*reserved357)(void);
    void (*reserved358)(void);
    void (*reserved359)(void);
    void (*reserved360)(void);
    void (*reserved361)(void);
    void (*reserved362)(void);
    void (*reserved363)(void);
    void (*reserved364)(void);
    void (*reserved365)(void);
    void (*reserved366)(void);
    void (*reserved367)(void);
    void (*reserved368)(void);
    void (*reserved369)(void);
    void (*reserved370)(void);
    void (*reserved371)(void);
    void (*reserved372)(void);
    void (*reserved373)(void);
    void (*reserved374)(void);
    void (*reserved375)(void);
    void (*reserved376)(void);
    void (*reserved377)(void);
    void (*reserved378)(void);
    void (*reserved379)(void);
    void (*reserved380)(void);
    void (*reserved381)(void);
    void (*reserved382)(void);
    void (*reserved383)(void);
    void (*reserved384)(void);
    void (*reserved385)(void);
    void (*reserved386)(void);
    void (*reserved387)(void);
    void (*reserved388)(void);
    void (*reserved389)(void);
    void (*reserved390)(void);
    void (*reserved391)(void);
    void (*reserved392)(void);
    void (*reserved393)(void);
    void (*reserved394)(void);
    void (*reserved395)(void);
    void (*reserved396)(void);
    void (*reserved397)(void);
    void (*reserved398)(void);
    void (*reserved399)(void);
    void (*reserved400)(void);
    void (*reserved401)(void);
    void (*reserved402)(void);
    void (*reserved403)(void);
    void (*reserved404)(void);
    void (*reserved405)(void);
    void (*reserved406)(void);
    void (*reserved407)(void);
    void (*reserved408)(void);
    void (*reserved409)(void);
    void (*reserved410)(void);
    void (*reserved411)(void);
    void (*reserved412)(void);
    void (*reserved413)(void);
    void (*reserved414)(void);
    void (*reserved415)(void);
    void (*reserved416)(void);
    void (*reserved417)(void);
    void (*reserved418)(void);
    void (*reserved419)(void);
    void (*reserved420)(void);
    void (*reserved421)(void);
    void (*reserved422)(void);
    void (*reserved423)(void);
    void (*reserved424)(void);
    void (*reserved425)(void);
    void (*reserved426)(void);
    void (*reserved427)(void);
    void (*reserved428)(void);
    void (*reserved429)(void);
    void (*reserved430)(void);
    void (*reserved431)(void);
    void (*reserved432)(void);
    void (*reserved433)(void);
    void (*reserved434)(void);
    void (*reserved435)(void);
    void (*reserved436)(void);
    void (*reserved437)(void);
    void (*reserved438)(void);
    void (*reserved439)(void);
    void (*reserved440)(void);
    void (*reserved441)(void);
    void (*reserved442)(void);
    void (*reserved443)(void);
    void (*reserved444)(void);
    void (*reserved445)(void);
    void (*reserved446)(void);
    void (*reserved447)(void);
    void (*reserved448)(void);
    void (*reserved449)(void);
    void (*reserved450)(void);
    void (*reserved451)(void);
    void (*reserved452)(void);
    void (*reserved453)(void);
    void (*reserved454)(void);
    void (*reserved455)(void);
    void (*reserved456)(void);
    void (*reserved457)(void);
    void (*reserved458)(void);
    void (*reserved459)(void);
    void (*reserved460)(void);
    void (*reserved461)(void);
    void (*reserved462)(void);
    void (*reserved463)(void);
    void (*reserved464)(void);
    void (*reserved465)(void);
    void (*reserved466)(void);
    void (*reserved467)(void);
    void (*reserved468)(void);
    void (*reserved469)(void);
    void (*reserved470)(void);
    void (*reserved471)(void);
    void (*reserved472)(void);
    void (*reserved473)(void);
    void (*reserved474)(void);
    void (*reserved475)(void);
    void (*reserved476)(void);
    void (*reserved477)(void);
    void (*reserved478)(void);
    void (*reserved479)(void);
    void (*reserved480)(void);
    void (*reserved481)(void);
    void (*reserved482)(void);
    void (*reserved483)(void);
    void (*reserved484)(void);
    void (*reserved485)(void);
    void (*reserved486)(void);
    void (*reserved487)(void);
    void (*reserved488)(void);
    void (*reserved489)(void);
    void (*reserved490)(void);
    void (*reserved491)(void);
    void (*reserved492)(void);
    void (*reserved493)(void);
    void (*reserved494)(void);
    void (*reserved495)(void);
    void (*reserved496)(void);
    void (*reserved497)(void);
    void (*reserved498)(void);
    void (*reserved499)(void);
    void (*reserved500)(void);
    void (*reserved501)(void);
    void (*reserved502)(void);
    void (*reserved503)(void);
    void (*reserved504)(void);
    void (*reserved505)(void);
    void (*reserved506)(void);
    void (*reserved507)(void);
    void (*reserved508)(void);
    void (*reserved509)(void);
    void (*reserved510)(void);
    void (*reserved511)(void);
    void (*reserved512)(void);
    void (*reserved513)(void);
    void (*reserved514)(void);
    void (*reserved515)(void);
    void (*reserved516)(void);
    void (*reserved517)(void);
    void (*reserved518)(void);
    void (*reserved519)(void);
    void (*reserved520)(void);
    void (*reserved521)(void);
    void (*reserved522)(void);
    void (*reserved523)(void);
    void (*reserved524)(void);
    void (*reserved525)(void);
    void (*reserved526)(void);
    void (*reserved527)(void);
    void (*reserved528)(void);
    void (*reserved529)(void);
    void (*reserved530)(void);
    void (*reserved531)(void);
    void (*reserved532)(void);
    void (*reserved533)(void);
    void (*reserved534)(void);
    void (*reserved535)(void);
    void (*reserved536)(void);
    void (*reserved537)(void);
    void (*reserved538)(void);
    void (*reserved539)(void);
    void (*reserved540)(void);
    void (*reserved541)(void);
    void (*reserved542)(void);
    void (*reserved543)(void);
    void (*reserved544)(void);
    void (*reserved545)(void);
    void (*reserved546)(void);
    void (*reserved547)(void);
    void (*reserved548)(void);
    void (*reserved549)(void);
    void (*reserved550)(void);
    void (*reserved551)(void);
    void (*reserved552)(void);
    void (*reserved553)(void);
    void (*reserved554)(void);
    void (*reserved555)(void);
    void (*reserved556)(void);
    void (*reserved557)(void);
    void (*reserved558)(void);
    void (*reserved559)(void);
    void (*reserved560)(void);
    void (*reserved561)(void);
    void (*reserved562)(void);
    void (*reserved563)(void);
    void (*reserved564)(void);
    void (*reserved565)(void);
    void (*reserved566)(void);
    void (*reserved567)(void);
    void (*reserved568)(void);
    void (*reserved569)(void);
    void (*reserved570)(void);
    void (*reserved571)(void);
    void (*reserved572)(void);
    void (*reserved573)(void);
    void (*reserved574)(void);
    void (*reserved575)(void);
    void (*reserved576)(void);
    void (*reserved577)(void);
    void (*reserved578)(void);
    void (*reserved579)(void);
    void (*reserved580)(void);
    void (*reserved581)(void);
    void (*reserved582)(void);
    void (*reserved583)(void);
    void (*reserved584)(void);
    void (*reserved585)(void);
    void (*reserved586)(void);
    void (*reserved587)(void);
    void (*reserved588)(void);
    void (*reserved589)(void);
    void (*reserved590)(void);
    void (*reserved591)(void);
    void (*reserved592)(void);
    void (*reserved593)(void);
    void (*reserved594)(void);
    void (*reserved595)(void);
    void (*reserved596)(void);
    void (*reserved597)(void);
    void (*reserved598)(void);
    void (*reserved599)(void);
    void (*reserved600)(void);
    void (*reserved601)(void);
    void (*reserved602)(void);
    void (*reserved603)(void);
    int (*tcl_ParseArgsObjv) (Tcl_Interp *interp, const Tcl_ArgvInfo *argTable, int *objcPtr, Tcl_Obj *const *objv, Tcl_Obj ***remObjv); /* 604 */
} TclPlatStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern const TclPlatStubs *tclPlatStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_TCL_STUBS)

/*
 * Inline function declarations:
 */

#ifdef __WIN32__ /* WIN */
#define Tcl_WinUtfToTChar \
	(tclPlatStubsPtr->tcl_WinUtfToTChar) /* 0 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
#define Tcl_MacOSXOpenBundleResources \
	(tclPlatStubsPtr->tcl_MacOSXOpenBundleResources) /* 0 */
#endif /* MACOSX */
#ifdef __WIN32__ /* WIN */
#define Tcl_WinTCharToUtf \
	(tclPlatStubsPtr->tcl_WinTCharToUtf) /* 1 */
#endif /* WIN */
#ifdef MAC_OSX_TCL /* MACOSX */
#define Tcl_MacOSXOpenVersionedBundleResources \
	(tclPlatStubsPtr->tcl_MacOSXOpenVersionedBundleResources) /* 1 */
#endif /* MACOSX */
/* Slot 2 is reserved */
/* Slot 3 is reserved */
/* Slot 4 is reserved */
/* Slot 5 is reserved */
/* Slot 6 is reserved */
/* Slot 7 is reserved */
/* Slot 8 is reserved */
/* Slot 9 is reserved */
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
/* Slot 13 is reserved */
/* Slot 14 is reserved */
/* Slot 15 is reserved */
/* Slot 16 is reserved */
/* Slot 17 is reserved */
/* Slot 18 is reserved */
/* Slot 19 is reserved */
/* Slot 20 is reserved */
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
/* Slot 24 is reserved */
/* Slot 25 is reserved */
/* Slot 26 is reserved */
/* Slot 27 is reserved */
/* Slot 28 is reserved */
/* Slot 29 is reserved */
/* Slot 30 is reserved */
/* Slot 31 is reserved */
/* Slot 32 is reserved */
/* Slot 33 is reserved */
/* Slot 34 is reserved */
/* Slot 35 is reserved */
/* Slot 36 is reserved */
/* Slot 37 is reserved */
/* Slot 38 is reserved */
/* Slot 39 is reserved */
/* Slot 40 is reserved */
/* Slot 41 is reserved */
/* Slot 42 is reserved */
/* Slot 43 is reserved */
/* Slot 44 is reserved */
/* Slot 45 is reserved */
/* Slot 46 is reserved */
/* Slot 47 is reserved */
/* Slot 48 is reserved */
/* Slot 49 is reserved */
/* Slot 50 is reserved */
/* Slot 51 is reserved */
/* Slot 52 is reserved */
/* Slot 53 is reserved */
/* Slot 54 is reserved */
/* Slot 55 is reserved */
/* Slot 56 is reserved */
/* Slot 57 is reserved */
/* Slot 58 is reserved */
/* Slot 59 is reserved */
/* Slot 60 is reserved */
/* Slot 61 is reserved */
/* Slot 62 is reserved */
/* Slot 63 is reserved */
/* Slot 64 is reserved */
/* Slot 65 is reserved */
/* Slot 66 is reserved */
/* Slot 67 is reserved */
/* Slot 68 is reserved */
/* Slot 69 is reserved */
/* Slot 70 is reserved */
/* Slot 71 is reserved */
/* Slot 72 is reserved */
/* Slot 73 is reserved */
/* Slot 74 is reserved */
/* Slot 75 is reserved */
/* Slot 76 is reserved */
/* Slot 77 is reserved */
/* Slot 78 is reserved */
/* Slot 79 is reserved */
/* Slot 80 is reserved */
/* Slot 81 is reserved */
/* Slot 82 is reserved */
/* Slot 83 is reserved */
/* Slot 84 is reserved */
/* Slot 85 is reserved */
/* Slot 86 is reserved */
/* Slot 87 is reserved */
/* Slot 88 is reserved */
/* Slot 89 is reserved */
/* Slot 90 is reserved */
/* Slot 91 is reserved */
/* Slot 92 is reserved */
/* Slot 93 is reserved */
/* Slot 94 is reserved */
/* Slot 95 is reserved */
/* Slot 96 is reserved */
/* Slot 97 is reserved */
/* Slot 98 is reserved */
/* Slot 99 is reserved */
/* Slot 100 is reserved */
/* Slot 101 is reserved */
/* Slot 102 is reserved */
/* Slot 103 is reserved */
/* Slot 104 is reserved */
/* Slot 105 is reserved */
/* Slot 106 is reserved */
/* Slot 107 is reserved */
/* Slot 108 is reserved */
/* Slot 109 is reserved */
/* Slot 110 is reserved */
/* Slot 111 is reserved */
/* Slot 112 is reserved */
/* Slot 113 is reserved */
/* Slot 114 is reserved */
/* Slot 115 is reserved */
/* Slot 116 is reserved */
/* Slot 117 is reserved */
/* Slot 118 is reserved */
/* Slot 119 is reserved */
/* Slot 120 is reserved */
/* Slot 121 is reserved */
/* Slot 122 is reserved */
/* Slot 123 is reserved */
/* Slot 124 is reserved */
/* Slot 125 is reserved */
/* Slot 126 is reserved */
/* Slot 127 is reserved */
/* Slot 128 is reserved */
/* Slot 129 is reserved */
/* Slot 130 is reserved */
/* Slot 131 is reserved */
/* Slot 132 is reserved */
/* Slot 133 is reserved */
/* Slot 134 is reserved */
/* Slot 135 is reserved */
/* Slot 136 is reserved */
/* Slot 137 is reserved */
/* Slot 138 is reserved */
/* Slot 139 is reserved */
/* Slot 140 is reserved */
/* Slot 141 is reserved */
/* Slot 142 is reserved */
/* Slot 143 is reserved */
/* Slot 144 is reserved */
/* Slot 145 is reserved */
/* Slot 146 is reserved */
/* Slot 147 is reserved */
/* Slot 148 is reserved */
/* Slot 149 is reserved */
/* Slot 150 is reserved */
/* Slot 151 is reserved */
/* Slot 152 is reserved */
/* Slot 153 is reserved */
/* Slot 154 is reserved */
/* Slot 155 is reserved */
/* Slot 156 is reserved */
/* Slot 157 is reserved */
/* Slot 158 is reserved */
/* Slot 159 is reserved */
/* Slot 160 is reserved */
/* Slot 161 is reserved */
/* Slot 162 is reserved */
/* Slot 163 is reserved */
/* Slot 164 is reserved */
/* Slot 165 is reserved */
/* Slot 166 is reserved */
/* Slot 167 is reserved */
/* Slot 168 is reserved */
/* Slot 169 is reserved */
/* Slot 170 is reserved */
/* Slot 171 is reserved */
/* Slot 172 is reserved */
/* Slot 173 is reserved */
/* Slot 174 is reserved */
/* Slot 175 is reserved */
/* Slot 176 is reserved */
/* Slot 177 is reserved */
/* Slot 178 is reserved */
/* Slot 179 is reserved */
/* Slot 180 is reserved */
/* Slot 181 is reserved */
/* Slot 182 is reserved */
/* Slot 183 is reserved */
/* Slot 184 is reserved */
/* Slot 185 is reserved */
/* Slot 186 is reserved */
/* Slot 187 is reserved */
/* Slot 188 is reserved */
/* Slot 189 is reserved */
/* Slot 190 is reserved */
/* Slot 191 is reserved */
/* Slot 192 is reserved */
/* Slot 193 is reserved */
/* Slot 194 is reserved */
/* Slot 195 is reserved */
/* Slot 196 is reserved */
/* Slot 197 is reserved */
/* Slot 198 is reserved */
/* Slot 199 is reserved */
/* Slot 200 is reserved */
/* Slot 201 is reserved */
/* Slot 202 is reserved */
/* Slot 203 is reserved */
/* Slot 204 is reserved */
/* Slot 205 is reserved */
/* Slot 206 is reserved */
/* Slot 207 is reserved */
/* Slot 208 is reserved */
/* Slot 209 is reserved */
/* Slot 210 is reserved */
/* Slot 211 is reserved */
/* Slot 212 is reserved */
/* Slot 213 is reserved */
/* Slot 214 is reserved */
/* Slot 215 is reserved */
/* Slot 216 is reserved */
/* Slot 217 is reserved */
/* Slot 218 is reserved */
/* Slot 219 is reserved */
/* Slot 220 is reserved */
/* Slot 221 is reserved */
/* Slot 222 is reserved */
/* Slot 223 is reserved */
/* Slot 224 is reserved */
/* Slot 225 is reserved */
/* Slot 226 is reserved */
/* Slot 227 is reserved */
/* Slot 228 is reserved */
/* Slot 229 is reserved */
/* Slot 230 is reserved */
/* Slot 231 is reserved */
/* Slot 232 is reserved */
/* Slot 233 is reserved */
/* Slot 234 is reserved */
/* Slot 235 is reserved */
/* Slot 236 is reserved */
/* Slot 237 is reserved */
/* Slot 238 is reserved */
/* Slot 239 is reserved */
/* Slot 240 is reserved */
/* Slot 241 is reserved */
/* Slot 242 is reserved */
/* Slot 243 is reserved */
/* Slot 244 is reserved */
/* Slot 245 is reserved */
/* Slot 246 is reserved */
/* Slot 247 is reserved */
/* Slot 248 is reserved */
/* Slot 249 is reserved */
/* Slot 250 is reserved */
/* Slot 251 is reserved */
/* Slot 252 is reserved */
/* Slot 253 is reserved */
/* Slot 254 is reserved */
/* Slot 255 is reserved */
/* Slot 256 is reserved */
/* Slot 257 is reserved */
/* Slot 258 is reserved */
/* Slot 259 is reserved */
/* Slot 260 is reserved */
/* Slot 261 is reserved */
/* Slot 262 is reserved */
/* Slot 263 is reserved */
/* Slot 264 is reserved */
/* Slot 265 is reserved */
/* Slot 266 is reserved */
/* Slot 267 is reserved */
/* Slot 268 is reserved */
/* Slot 269 is reserved */
/* Slot 270 is reserved */
/* Slot 271 is reserved */
/* Slot 272 is reserved */
/* Slot 273 is reserved */
/* Slot 274 is reserved */
/* Slot 275 is reserved */
/* Slot 276 is reserved */
/* Slot 277 is reserved */
/* Slot 278 is reserved */
/* Slot 279 is reserved */
/* Slot 280 is reserved */
/* Slot 281 is reserved */
/* Slot 282 is reserved */
/* Slot 283 is reserved */
/* Slot 284 is reserved */
/* Slot 285 is reserved */
/* Slot 286 is reserved */
/* Slot 287 is reserved */
/* Slot 288 is reserved */
/* Slot 289 is reserved */
/* Slot 290 is reserved */
/* Slot 291 is reserved */
/* Slot 292 is reserved */
/* Slot 293 is reserved */
/* Slot 294 is reserved */
/* Slot 295 is reserved */
/* Slot 296 is reserved */
/* Slot 297 is reserved */
/* Slot 298 is reserved */
/* Slot 299 is reserved */
/* Slot 300 is reserved */
/* Slot 301 is reserved */
/* Slot 302 is reserved */
/* Slot 303 is reserved */
/* Slot 304 is reserved */
/* Slot 305 is reserved */
/* Slot 306 is reserved */
/* Slot 307 is reserved */
/* Slot 308 is reserved */
/* Slot 309 is reserved */
/* Slot 310 is reserved */
/* Slot 311 is reserved */
/* Slot 312 is reserved */
/* Slot 313 is reserved */
/* Slot 314 is reserved */
/* Slot 315 is reserved */
/* Slot 316 is reserved */
/* Slot 317 is reserved */
/* Slot 318 is reserved */
/* Slot 319 is reserved */
/* Slot 320 is reserved */
/* Slot 321 is reserved */
/* Slot 322 is reserved */
/* Slot 323 is reserved */
/* Slot 324 is reserved */
/* Slot 325 is reserved */
/* Slot 326 is reserved */
/* Slot 327 is reserved */
/* Slot 328 is reserved */
/* Slot 329 is reserved */
/* Slot 330 is reserved */
/* Slot 331 is reserved */
/* Slot 332 is reserved */
/* Slot 333 is reserved */
/* Slot 334 is reserved */
/* Slot 335 is reserved */
/* Slot 336 is reserved */
/* Slot 337 is reserved */
/* Slot 338 is reserved */
/* Slot 339 is reserved */
/* Slot 340 is reserved */
/* Slot 341 is reserved */
/* Slot 342 is reserved */
/* Slot 343 is reserved */
/* Slot 344 is reserved */
/* Slot 345 is reserved */
/* Slot 346 is reserved */
/* Slot 347 is reserved */
/* Slot 348 is reserved */
/* Slot 349 is reserved */
/* Slot 350 is reserved */
/* Slot 351 is reserved */
/* Slot 352 is reserved */
/* Slot 353 is reserved */
/* Slot 354 is reserved */
/* Slot 355 is reserved */
/* Slot 356 is reserved */
/* Slot 357 is reserved */
/* Slot 358 is reserved */
/* Slot 359 is reserved */
/* Slot 360 is reserved */
/* Slot 361 is reserved */
/* Slot 362 is reserved */
/* Slot 363 is reserved */
/* Slot 364 is reserved */
/* Slot 365 is reserved */
/* Slot 366 is reserved */
/* Slot 367 is reserved */
/* Slot 368 is reserved */
/* Slot 369 is reserved */
/* Slot 370 is reserved */
/* Slot 371 is reserved */
/* Slot 372 is reserved */
/* Slot 373 is reserved */
/* Slot 374 is reserved */
/* Slot 375 is reserved */
/* Slot 376 is reserved */
/* Slot 377 is reserved */
/* Slot 378 is reserved */
/* Slot 379 is reserved */
/* Slot 380 is reserved */
/* Slot 381 is reserved */
/* Slot 382 is reserved */
/* Slot 383 is reserved */
/* Slot 384 is reserved */
/* Slot 385 is reserved */
/* Slot 386 is reserved */
/* Slot 387 is reserved */
/* Slot 388 is reserved */
/* Slot 389 is reserved */
/* Slot 390 is reserved */
/* Slot 391 is reserved */
/* Slot 392 is reserved */
/* Slot 393 is reserved */
/* Slot 394 is reserved */
/* Slot 395 is reserved */
/* Slot 396 is reserved */
/* Slot 397 is reserved */
/* Slot 398 is reserved */
/* Slot 399 is reserved */
/* Slot 400 is reserved */
/* Slot 401 is reserved */
/* Slot 402 is reserved */
/* Slot 403 is reserved */
/* Slot 404 is reserved */
/* Slot 405 is reserved */
/* Slot 406 is reserved */
/* Slot 407 is reserved */
/* Slot 408 is reserved */
/* Slot 409 is reserved */
/* Slot 410 is reserved */
/* Slot 411 is reserved */
/* Slot 412 is reserved */
/* Slot 413 is reserved */
/* Slot 414 is reserved */
/* Slot 415 is reserved */
/* Slot 416 is reserved */
/* Slot 417 is reserved */
/* Slot 418 is reserved */
/* Slot 419 is reserved */
/* Slot 420 is reserved */
/* Slot 421 is reserved */
/* Slot 422 is reserved */
/* Slot 423 is reserved */
/* Slot 424 is reserved */
/* Slot 425 is reserved */
/* Slot 426 is reserved */
/* Slot 427 is reserved */
/* Slot 428 is reserved */
/* Slot 429 is reserved */
/* Slot 430 is reserved */
/* Slot 431 is reserved */
/* Slot 432 is reserved */
/* Slot 433 is reserved */
/* Slot 434 is reserved */
/* Slot 435 is reserved */
/* Slot 436 is reserved */
/* Slot 437 is reserved */
/* Slot 438 is reserved */
/* Slot 439 is reserved */
/* Slot 440 is reserved */
/* Slot 441 is reserved */
/* Slot 442 is reserved */
/* Slot 443 is reserved */
/* Slot 444 is reserved */
/* Slot 445 is reserved */
/* Slot 446 is reserved */
/* Slot 447 is reserved */
/* Slot 448 is reserved */
/* Slot 449 is reserved */
/* Slot 450 is reserved */
/* Slot 451 is reserved */
/* Slot 452 is reserved */
/* Slot 453 is reserved */
/* Slot 454 is reserved */
/* Slot 455 is reserved */
/* Slot 456 is reserved */
/* Slot 457 is reserved */
/* Slot 458 is reserved */
/* Slot 459 is reserved */
/* Slot 460 is reserved */
/* Slot 461 is reserved */
/* Slot 462 is reserved */
/* Slot 463 is reserved */
/* Slot 464 is reserved */
/* Slot 465 is reserved */
/* Slot 466 is reserved */
/* Slot 467 is reserved */
/* Slot 468 is reserved */
/* Slot 469 is reserved */
/* Slot 470 is reserved */
/* Slot 471 is reserved */
/* Slot 472 is reserved */
/* Slot 473 is reserved */
/* Slot 474 is reserved */
/* Slot 475 is reserved */
/* Slot 476 is reserved */
/* Slot 477 is reserved */
/* Slot 478 is reserved */
/* Slot 479 is reserved */
/* Slot 480 is reserved */
/* Slot 481 is reserved */
/* Slot 482 is reserved */
/* Slot 483 is reserved */
/* Slot 484 is reserved */
/* Slot 485 is reserved */
/* Slot 486 is reserved */
/* Slot 487 is reserved */
/* Slot 488 is reserved */
/* Slot 489 is reserved */
/* Slot 490 is reserved */
/* Slot 491 is reserved */
/* Slot 492 is reserved */
/* Slot 493 is reserved */
/* Slot 494 is reserved */
/* Slot 495 is reserved */
/* Slot 496 is reserved */
/* Slot 497 is reserved */
/* Slot 498 is reserved */
/* Slot 499 is reserved */
/* Slot 500 is reserved */
/* Slot 501 is reserved */
/* Slot 502 is reserved */
/* Slot 503 is reserved */
/* Slot 504 is reserved */
/* Slot 505 is reserved */
/* Slot 506 is reserved */
/* Slot 507 is reserved */
/* Slot 508 is reserved */
/* Slot 509 is reserved */
/* Slot 510 is reserved */
/* Slot 511 is reserved */
/* Slot 512 is reserved */
/* Slot 513 is reserved */
/* Slot 514 is reserved */
/* Slot 515 is reserved */
/* Slot 516 is reserved */
/* Slot 517 is reserved */
/* Slot 518 is reserved */
/* Slot 519 is reserved */
/* Slot 520 is reserved */
/* Slot 521 is reserved */
/* Slot 522 is reserved */
/* Slot 523 is reserved */
/* Slot 524 is reserved */
/* Slot 525 is reserved */
/* Slot 526 is reserved */
/* Slot 527 is reserved */
/* Slot 528 is reserved */
/* Slot 529 is reserved */
/* Slot 530 is reserved */
/* Slot 531 is reserved */
/* Slot 532 is reserved */
/* Slot 533 is reserved */
/* Slot 534 is reserved */
/* Slot 535 is reserved */
/* Slot 536 is reserved */
/* Slot 537 is reserved */
/* Slot 538 is reserved */
/* Slot 539 is reserved */
/* Slot 540 is reserved */
/* Slot 541 is reserved */
/* Slot 542 is reserved */
/* Slot 543 is reserved */
/* Slot 544 is reserved */
/* Slot 545 is reserved */
/* Slot 546 is reserved */
/* Slot 547 is reserved */
/* Slot 548 is reserved */
/* Slot 549 is reserved */
/* Slot 550 is reserved */
/* Slot 551 is reserved */
/* Slot 552 is reserved */
/* Slot 553 is reserved */
/* Slot 554 is reserved */
/* Slot 555 is reserved */
/* Slot 556 is reserved */
/* Slot 557 is reserved */
/* Slot 558 is reserved */
/* Slot 559 is reserved */
/* Slot 560 is reserved */
/* Slot 561 is reserved */
/* Slot 562 is reserved */
/* Slot 563 is reserved */
/* Slot 564 is reserved */
/* Slot 565 is reserved */
/* Slot 566 is reserved */
/* Slot 567 is reserved */
/* Slot 568 is reserved */
/* Slot 569 is reserved */
/* Slot 570 is reserved */
/* Slot 571 is reserved */
/* Slot 572 is reserved */
/* Slot 573 is reserved */
/* Slot 574 is reserved */
/* Slot 575 is reserved */
/* Slot 576 is reserved */
/* Slot 577 is reserved */
/* Slot 578 is reserved */
/* Slot 579 is reserved */
/* Slot 580 is reserved */
/* Slot 581 is reserved */
/* Slot 582 is reserved */
/* Slot 583 is reserved */
/* Slot 584 is reserved */
/* Slot 585 is reserved */
/* Slot 586 is reserved */
/* Slot 587 is reserved */
/* Slot 588 is reserved */
/* Slot 589 is reserved */
/* Slot 590 is reserved */
/* Slot 591 is reserved */
/* Slot 592 is reserved */
/* Slot 593 is reserved */
/* Slot 594 is reserved */
/* Slot 595 is reserved */
/* Slot 596 is reserved */
/* Slot 597 is reserved */
/* Slot 598 is reserved */
/* Slot 599 is reserved */
/* Slot 600 is reserved */
/* Slot 601 is reserved */
/* Slot 602 is reserved */
/* Slot 603 is reserved */
#define Tcl_ParseArgsObjv \
	(tclPlatStubsPtr->tcl_ParseArgsObjv) /* 604 */

#endif /* defined(USE_TCL_STUBS) */

/* !END!: Do not edit above this line. */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCLPLATDECLS */


