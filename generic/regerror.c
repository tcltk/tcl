/*
 * regerror - error-code expansion
 */

#include "regguts.h"

/* unknown-error explanation */
static char unk[] = "*** unknown regex error code 0x%x ***";

/* struct to map among codes, code names, and explanations */
static struct rerr {
	int code;
	char *name;
	char *explain;
} rerrs[] = {
	/* the actual table is built from regex.h */
#	include "regerrs.h"
	{ -1,	"",	"oops" },	/* explanation special-cased in code */
};

/*
 - regerror - the interface to error numbers
 */
/* ARGSUSED */
size_t				/* actual space needed (including NUL) */
regerror(errcode, preg, errbuf, errbuf_size)
int errcode;			/* error code, or REG_ATOI or REG_ITOA */
CONST regex_t *preg;		/* associated regex_t (unused at present) */
char *errbuf;			/* result buffer (unless errbuf_size==0) */
size_t errbuf_size;		/* available space in errbuf, can be 0 */
{
	struct rerr *r;
	char *msg;
	char convbuf[sizeof(unk)+50];	/* 50 = plenty for int */
	size_t len;
	int icode;

	switch (errcode) {
	case REG_ATOI:		/* convert name to number */
		for (r = rerrs; r->code >= 0; r++)
			if (strcmp(r->name, errbuf) == 0)
				break;
		sprintf(convbuf, "%d", r->code);	/* -1 for unknown */
		msg = convbuf;
		break;
	case REG_ITOA:		/* convert number to name */
		icode = atoi(errbuf);	/* not our problem if this fails */
		for (r = rerrs; r->code >= 0; r++)
			if (r->code == icode)
				break;
		if (r->code >= 0)
			msg = r->name;
		else {			/* unknown; tell him the number */
			sprintf(convbuf, "REG_%u", (unsigned)icode);
			msg = convbuf;
		}
		break;
	default:		/* a real, normal error code */
		for (r = rerrs; r->code >= 0; r++)
			if (r->code == errcode)
				break;
		if (r->code >= 0)
			msg = r->explain;
		else {			/* unknown; say so */
			sprintf(convbuf, unk, errcode);
			msg = convbuf;
		}
		break;
	}

	len = strlen(msg) + 1;		/* space needed, including NUL */
	if (errbuf_size > 0) {
		if (errbuf_size > len)
			strcpy(errbuf, msg);
		else {			/* truncate to fit */
			strncpy(errbuf, msg, errbuf_size-1);
			errbuf[errbuf_size-1] = '\0';
		}
	}

	return len;
}
