/* $OpenBSD: dsdt.c,v 1.130 2008/06/14 21:40:16 jordan Exp $ */
/*
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/bus.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#endif

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#ifdef SMALL_KERNEL
#undef ACPI_DEBUG
#endif

#define opsize(opcode) (((opcode) & 0xFF00) ? 2 : 1)

#define AML_FIELD_RESERVED	0x00
#define AML_FIELD_ATTRIB	0x01

#define AML_REVISION		0x01
#define AML_INTSTRLEN		16
#define AML_NAMESEG_LEN		4

void			aml_copyvalue(struct aml_value *, struct aml_value *);

void			aml_setvalue(struct aml_scope *, struct aml_value *,
			    struct aml_value *, int64_t);
void			aml_freevalue(struct aml_value *);
struct aml_value	*aml_allocvalue(int, int64_t, const void *);
struct aml_value	*_aml_setvalue(struct aml_value *, int, int64_t,
			    const void *);

u_int64_t		aml_convradix(u_int64_t, int, int);
int64_t			aml_evalexpr(int64_t, int64_t, int);
int			aml_lsb(u_int64_t);
int			aml_msb(u_int64_t);

int			aml_tstbit(const u_int8_t *, int);
void			aml_setbit(u_int8_t *, int, int);

void		aml_xaddref(struct aml_value *, const char *);
void		aml_xdelref(struct aml_value **, const char *);

void			aml_bufcpy(void *, int, const void *, int, int);
int			aml_evalinteger(struct acpi_softc *, struct aml_node *,
			    const char *, int, struct aml_value *, int64_t *);

int			aml_pc(uint8_t *);

struct aml_value	*aml_parseop(struct aml_scope *, struct aml_value *,int);
struct aml_value	*aml_parsetarget(struct aml_scope *, struct aml_value *,
			    struct aml_value **);
struct aml_value	*aml_parseterm(struct aml_scope *, struct aml_value *);

struct aml_value	*aml_evaltarget(struct aml_scope *scope,
			    struct aml_value *res);
int			aml_evalterm(struct aml_scope *scope,
			    struct aml_value *raw, struct aml_value *dst);
void			aml_gasio(struct acpi_softc *, int, uint64_t, uint64_t,
			    int, int, int, void *, int);

struct aml_opcode	*aml_findopcode(int);

#define acpi_os_malloc(sz) _acpi_os_malloc(sz, __FUNCTION__, __LINE__)
#define acpi_os_free(ptr)  _acpi_os_free(ptr, __FUNCTION__, __LINE__)

void			*_acpi_os_malloc(size_t, const char *, int);
void			_acpi_os_free(void *, const char *, int);
void			acpi_sleep(int);
void			acpi_stall(int);

uint8_t *aml_xparsename(uint8_t *pos, struct aml_node *node, 
    void (*fn)(struct aml_node *, int, uint8_t *, void *), void *arg);
void ns_xcreate(struct aml_node *node, int n, uint8_t *pos, void *arg);
void ns_xdis(struct aml_node *node, int n, uint8_t *pos, void *arg);
void ns_xsearch(struct aml_node *node, int n, uint8_t *pos, void *arg);

struct aml_value	*aml_callosi(struct aml_scope *, struct aml_value *);

const char		*aml_getname(const char *);
int64_t			aml_hextoint(const char *);
void			aml_dump(int, u_int8_t *);
void			_aml_die(const char *fn, int line, const char *fmt, ...);
#define aml_die(x...)	_aml_die(__FUNCTION__, __LINE__, x)

/*
 * @@@: Global variables
 */
int			aml_intlen = 64;
struct aml_node		aml_root;
struct aml_value	*aml_global_lock;
struct acpi_softc	*dsdt_softc;

/* Perfect Hash key */
#define HASH_OFF		6904
#define HASH_SIZE		179
#define HASH_KEY(k)		(((k) ^ HASH_OFF) % HASH_SIZE)

/*
 * XXX this array should be sorted, and then aml_findopcode() should
 * do a binary search
 */
struct aml_opcode **aml_ophash;
struct aml_opcode aml_table[] = {
	/* Simple types */
	{ AMLOP_ZERO,		"Zero",		"c",	},
	{ AMLOP_ONE,		"One",		"c",	},
	{ AMLOP_ONES,		"Ones",		"c",	},
	{ AMLOP_REVISION,	"Revision",	"R",	},
	{ AMLOP_BYTEPREFIX,	".Byte",	"b",	},
	{ AMLOP_WORDPREFIX,	".Word",	"w",	},
	{ AMLOP_DWORDPREFIX,	".DWord",	"d",	},
	{ AMLOP_QWORDPREFIX,	".QWord",	"q",	},
	{ AMLOP_STRINGPREFIX,	".String",	"a",	},
	{ AMLOP_DEBUG,		"DebugOp",	"D",	},
	{ AMLOP_BUFFER,		"Buffer",	"piB",	},
	{ AMLOP_PACKAGE,	"Package",	"pbT",	},
	{ AMLOP_VARPACKAGE,	"VarPackage",	"piT",	},

	/* Simple objects */
	{ AMLOP_LOCAL0,		"Local0",	"L",	},
	{ AMLOP_LOCAL1,		"Local1",	"L",	},
	{ AMLOP_LOCAL2,		"Local2",	"L",	},
	{ AMLOP_LOCAL3,		"Local3",	"L",	},
	{ AMLOP_LOCAL4,		"Local4",	"L",	},
	{ AMLOP_LOCAL5,		"Local5",	"L",	},
	{ AMLOP_LOCAL6,		"Local6",	"L",	},
	{ AMLOP_LOCAL7,		"Local7",	"L",	},
	{ AMLOP_ARG0,		"Arg0",		"A",	},
	{ AMLOP_ARG1,		"Arg1",		"A",	},
	{ AMLOP_ARG2,		"Arg2",		"A",	},
	{ AMLOP_ARG3,		"Arg3",		"A",	},
	{ AMLOP_ARG4,		"Arg4",		"A",	},
	{ AMLOP_ARG5,		"Arg5",		"A",	},
	{ AMLOP_ARG6,		"Arg6",		"A",	},

	/* Control flow */
	{ AMLOP_IF,		"If",		"piI",	},
	{ AMLOP_ELSE,		"Else",		"pT" },
	{ AMLOP_WHILE,		"While",	"pW",	},
	{ AMLOP_BREAK,		"Break",	"" },
	{ AMLOP_CONTINUE,	"Continue",	"" },
	{ AMLOP_RETURN,		"Return",	"t",	},
	{ AMLOP_FATAL,		"Fatal",	"bdi",	},
	{ AMLOP_NOP,		"Nop",		"",	},
	{ AMLOP_BREAKPOINT,	"BreakPoint",	"",     },

	/* Arithmetic operations */
	{ AMLOP_INCREMENT,	"Increment",	"t",	},
	{ AMLOP_DECREMENT,	"Decrement",	"t",	},
	{ AMLOP_ADD,		"Add",		"iir",	},
	{ AMLOP_SUBTRACT,	"Subtract",	"iir",	},
	{ AMLOP_MULTIPLY,	"Multiply",	"iir",	},
	{ AMLOP_DIVIDE,		"Divide",	"iirr",	},
	{ AMLOP_SHL,		"ShiftLeft",	"iir",	},
	{ AMLOP_SHR,		"ShiftRight",	"iir",	},
	{ AMLOP_AND,		"And",		"iir",	},
	{ AMLOP_NAND,		"Nand",		"iir",	},
	{ AMLOP_OR,		"Or",		"iir",	},
	{ AMLOP_NOR,		"Nor",		"iir",	},
	{ AMLOP_XOR,		"Xor",		"iir",	},
	{ AMLOP_NOT,		"Not",		"ir",	},
	{ AMLOP_MOD,		"Mod",		"iir",	},
	{ AMLOP_FINDSETLEFTBIT,	"FindSetLeftBit", "ir",	},
	{ AMLOP_FINDSETRIGHTBIT,"FindSetRightBit", "ir",},

	/* Logical test operations */
	{ AMLOP_LAND,		"LAnd",		"ii",	},
	{ AMLOP_LOR,		"LOr",		"ii",	},
	{ AMLOP_LNOT,		"LNot",		"i",	},
	{ AMLOP_LNOTEQUAL,	"LNotEqual",	"tt",	},
	{ AMLOP_LLESSEQUAL,	"LLessEqual",	"tt",	},
	{ AMLOP_LGREATEREQUAL,	"LGreaterEqual", "tt",	},
	{ AMLOP_LEQUAL,		"LEqual",	"tt",	},
	{ AMLOP_LGREATER,	"LGreater",	"tt",	},
	{ AMLOP_LLESS,		"LLess",	"tt",	},

	/* Named objects */
	{ AMLOP_NAMECHAR,	".NameRef",	"n",	},
	{ AMLOP_ALIAS,		"Alias",	"nN",	},
	{ AMLOP_NAME,		"Name",	"Nt",	},
	{ AMLOP_EVENT,		"Event",	"N",	},
	{ AMLOP_MUTEX,		"Mutex",	"Nb",	},
	{ AMLOP_DATAREGION,	"DataRegion",	"Nttt",	},
	{ AMLOP_OPREGION,	"OpRegion",	"Nbii",	},
	{ AMLOP_SCOPE,		"Scope",	"pNT",	},
	{ AMLOP_DEVICE,		"Device",	"pNT",	},
	{ AMLOP_POWERRSRC,	"Power Resource", "pNbwT",},
	{ AMLOP_THERMALZONE,	"ThermalZone",	"pNT",	},
	{ AMLOP_PROCESSOR,	"Processor",	"pNbdbT", },
	{ AMLOP_METHOD,		"Method",	"pNbM",	},

	/* Field operations */
	{ AMLOP_FIELD,		"Field",	"pnbF",	},
	{ AMLOP_INDEXFIELD,	"IndexField",	"pnnbF",},
	{ AMLOP_BANKFIELD,	"BankField",	"pnnibF",},
	{ AMLOP_CREATEFIELD,	"CreateField",	"tiiN",		},
	{ AMLOP_CREATEQWORDFIELD, "CreateQWordField","tiN",},
	{ AMLOP_CREATEDWORDFIELD, "CreateDWordField","tiN",},
	{ AMLOP_CREATEWORDFIELD, "CreateWordField", "tiN",},
	{ AMLOP_CREATEBYTEFIELD, "CreateByteField", "tiN",},
	{ AMLOP_CREATEBITFIELD,	"CreateBitField", "tiN",	},

	/* Conversion operations */
	{ AMLOP_TOINTEGER,	"ToInteger",	"tr",	},
	{ AMLOP_TOBUFFER,	"ToBuffer",	"tr",	},
	{ AMLOP_TODECSTRING,	"ToDecString",	"ir",	},
	{ AMLOP_TOHEXSTRING,	"ToHexString",	"ir",	},
	{ AMLOP_TOSTRING,	"ToString",	"tir",	},
	{ AMLOP_MID,		"Mid",		"tiir",	},
	{ AMLOP_FROMBCD,	"FromBCD",	"ir",	},
	{ AMLOP_TOBCD,		"ToBCD",	"ir",	},

	/* Mutex/Signal operations */
	{ AMLOP_ACQUIRE,	"Acquire",	"Sw",	},
	{ AMLOP_RELEASE,	"Release",	"S",	},
	{ AMLOP_SIGNAL,		"Signal",	"S",	},
	{ AMLOP_WAIT,		"Wait",		"Si",	},
	{ AMLOP_RESET,		"Reset",	"S",	},

	{ AMLOP_INDEX,		"Index",	"tir",	},
	{ AMLOP_DEREFOF,	"DerefOf",	"t",	},
	{ AMLOP_REFOF,		"RefOf",	"S",	},
	{ AMLOP_CONDREFOF,	"CondRef",	"SS",	},

	{ AMLOP_LOADTABLE,	"LoadTable",	"tttttt" },
	{ AMLOP_STALL,		"Stall",	"i",	},
	{ AMLOP_SLEEP,		"Sleep",	"i",	},
	{ AMLOP_LOAD,		"Load",		"nS",	},
	{ AMLOP_UNLOAD,		"Unload",	"t" },
	{ AMLOP_STORE,		"Store",	"tS",	},
	{ AMLOP_CONCAT,		"Concat",	"ttr",	},
	{ AMLOP_CONCATRES,	"ConcatRes",	"ttt" },
	{ AMLOP_NOTIFY,		"Notify",	"Si",	},
	{ AMLOP_SIZEOF,		"Sizeof",	"S",	},
	{ AMLOP_MATCH,		"Match",	"tbibii", },
	{ AMLOP_OBJECTTYPE,	"ObjectType",	"S",	},
	{ AMLOP_COPYOBJECT,	"CopyObject",	"tS",	},
};

int aml_pc(uint8_t *src)
{
	return src - aml_root.start;
}

struct aml_scope *aml_lastscope;

void
_aml_die(const char *fn, int line, const char *fmt, ...)
{
#ifndef SMALL_KERNEL
	struct aml_scope *root;
	struct aml_value *sp;

	int idx;
#endif /* SMALL_KERNEL */
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);

#ifndef SMALL_KERNEL
	for (root = aml_lastscope; root && root->pos; root = root->parent) {
		printf("%.4x Called: %s\n", aml_pc(root->pos),
		    aml_nodename(root->node));
		for (idx = 0; idx < AML_MAX_ARG; idx++) {
			sp = aml_getstack(root, AMLOP_ARG0+idx);
			if (sp && sp->type) {
			printf("  arg%d: ", idx);
				aml_showvalue(sp, 0);
			}
		}
		for (idx = 0; idx < AML_MAX_LOCAL; idx++) {
			sp = aml_getstack(root, AMLOP_LOCAL0+idx);
			if (sp && sp->type) {
				printf("  local%d: ", idx);
				aml_showvalue(sp, 0);
			}
		}
	}
#endif /* SMALL_KERNEL */

	/* XXX: don't panic */
	panic("aml_die %s:%d", fn, line);
}

void
aml_hashopcodes(void)
{
	int i;

	/* Dynamically allocate hash table */
	aml_ophash = (struct aml_opcode **)acpi_os_malloc(HASH_SIZE*sizeof(struct aml_opcode *));
	for (i = 0; i < sizeof(aml_table) / sizeof(aml_table[0]); i++)
		aml_ophash[HASH_KEY(aml_table[i].opcode)] = &aml_table[i];
}

struct aml_opcode *
aml_findopcode(int opcode)
{
	struct aml_opcode *hop;

	hop = aml_ophash[HASH_KEY(opcode)];
	if (hop && hop->opcode == opcode)
		return hop;
	return NULL;
}

#ifndef SMALL_KERNEL
const char *
aml_mnem(int opcode, uint8_t *pos)
{
	struct aml_opcode *tab;
	static char mnemstr[32];

	if ((tab = aml_findopcode(opcode)) != NULL) {
		strlcpy(mnemstr, tab->mnem, sizeof(mnemstr));
		if (pos != NULL) {
			switch (opcode) {
			case AMLOP_STRINGPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "\"%s\"", pos);
				break;
			case AMLOP_BYTEPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "0x%.2x",
					 *(uint8_t *)pos);
				break;
			case AMLOP_WORDPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "0x%.4x",
					 *(uint16_t *)pos);
				break;
			case AMLOP_DWORDPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "0x%.4x",
					 *(uint16_t *)pos);
				break;
			case AMLOP_NAMECHAR:
				strlcpy(mnemstr, aml_getname(pos), sizeof(mnemstr));
				break;
			}
		}
		return mnemstr;
	}
	return ("xxx");
}
#endif /* SMALL_KERNEL */

const char *
aml_args(int opcode)
{
	struct aml_opcode *tab;

	if ((tab = aml_findopcode(opcode)) != NULL)
		return tab->args;
	return ("");
}

struct aml_notify_data {
	struct aml_node		*node;
	char			pnpid[20];
	void			*cbarg;
	int			(*cbproc)(struct aml_node *, int, void *);
	int			poll;

	SLIST_ENTRY(aml_notify_data) link;
};

SLIST_HEAD(aml_notify_head, aml_notify_data);
struct aml_notify_head aml_notify_list =
    LIST_HEAD_INITIALIZER(&aml_notify_list);

/*
 *  @@@: Memory management functions
 */

long acpi_nalloc;

struct acpi_memblock {
	size_t size;
};

void *
_acpi_os_malloc(size_t size, const char *fn, int line)
{
	struct acpi_memblock *sptr;

	sptr = malloc(size+sizeof(*sptr), M_ACPI, M_WAITOK | M_ZERO);
	dnprintf(99, "alloc: %x %s:%d\n", sptr, fn, line);
	acpi_nalloc += size;
	sptr->size = size;
	return &sptr[1];
}

void
_acpi_os_free(void *ptr, const char *fn, int line)
{
	struct acpi_memblock *sptr;

	if (ptr != NULL) {
		sptr = &(((struct acpi_memblock *)ptr)[-1]);
		acpi_nalloc -= sptr->size;

		dnprintf(99, "free: %x %s:%d\n", sptr, fn, line);
		free(sptr, M_ACPI);
	}
}

void
acpi_sleep(int ms)
{
	int to = ms * hz / 1000;

	if (cold)
		delay(ms * 1000);
	else {
		if (to <= 0)
			to = 1;
		while (tsleep(dsdt_softc, PWAIT, "asleep", to) !=
		    EWOULDBLOCK);
	}
}

void
acpi_stall(int us)
{
	delay(us);
}

/*
 * @@@: Misc utility functions
 */

#ifdef ACPI_DEBUG
void
aml_dump(int len, u_int8_t *buf)
{
	int		idx;

	dnprintf(50, "{ ");
	for (idx = 0; idx < len; idx++) {
		dnprintf(50, "%s0x%.2x", idx ? ", " : "", buf[idx]);
	}
	dnprintf(50, " }\n");
}
#endif

/* Bit mangling code */
int
aml_tstbit(const u_int8_t *pb, int bit)
{
	pb += aml_bytepos(bit);

	return (*pb & aml_bitmask(bit));
}

void
aml_setbit(u_int8_t *pb, int bit, int val)
{
	pb += aml_bytepos(bit);

	if (val)
		*pb |= aml_bitmask(bit);
	else
		*pb &= ~aml_bitmask(bit);
}

/*
 * @@@: Notify functions
 */
void
acpi_poll(void *arg)
{
	dsdt_softc->sc_poll = 1;
	dsdt_softc->sc_wakeup = 0;
	wakeup(dsdt_softc);

	timeout_add(&dsdt_softc->sc_dev_timeout, 10 * hz);
}

void
aml_register_notify(struct aml_node *node, const char *pnpid,
    int (*proc)(struct aml_node *, int, void *), void *arg, int poll)
{
	struct aml_notify_data	*pdata;
	extern int acpi_poll_enabled;

	dnprintf(10, "aml_register_notify: %s %s %x\n",
	    node->name, pnpid ? pnpid : "", proc);

	pdata = acpi_os_malloc(sizeof(struct aml_notify_data));
	pdata->node = node;
	pdata->cbarg = arg;
	pdata->cbproc = proc;
	pdata->poll = poll;

	if (pnpid)
		strlcpy(pdata->pnpid, pnpid, sizeof(pdata->pnpid));

	SLIST_INSERT_HEAD(&aml_notify_list, pdata, link);

	if (poll && !acpi_poll_enabled)
		timeout_add(&dsdt_softc->sc_dev_timeout, 10 * hz);
}

void
aml_notify(struct aml_node *node, int notify_value)
{
	struct aml_notify_data	*pdata = NULL;

	if (node == NULL)
		return;

	SLIST_FOREACH(pdata, &aml_notify_list, link)
		if (pdata->node == node)
			pdata->cbproc(pdata->node, notify_value, pdata->cbarg);
}

void
aml_notify_dev(const char *pnpid, int notify_value)
{
	struct aml_notify_data	*pdata = NULL;

	if (pnpid == NULL)
		return;

	SLIST_FOREACH(pdata, &aml_notify_list, link)
		if (pdata->pnpid && !strcmp(pdata->pnpid, pnpid))
			pdata->cbproc(pdata->node, notify_value, pdata->cbarg);
}

void acpi_poll_notify(void)
{
	struct aml_notify_data	*pdata = NULL;

	SLIST_FOREACH(pdata, &aml_notify_list, link)
		if (pdata->cbproc && pdata->poll)
			pdata->cbproc(pdata->node, 0, pdata->cbarg);
}

/*
 * @@@: Namespace functions
 */

struct aml_node *__aml_search(struct aml_node *, uint8_t *, int);
void aml_delchildren(struct aml_node *);


/* Search for a name in children nodes */
struct aml_node *
__aml_search(struct aml_node *root, uint8_t *nameseg, int create)
{
	struct aml_node **sp, *node;

	/* XXX: Replace with SLIST/SIMPLEQ routines */
	if (root == NULL)
		return NULL;
	//rw_enter_read(&aml_nslock);
	for (sp = &root->child; *sp; sp = &(*sp)->sibling) {
		if (!strncmp((*sp)->name, nameseg, AML_NAMESEG_LEN)) {
			//rw_exit_read(&aml_nslock);
			return *sp;
	}
	}
	//rw_exit_read(&aml_nslock);
	if (create) {
		node = acpi_os_malloc(sizeof(struct aml_node));
		memcpy((void *)node->name, nameseg, AML_NAMESEG_LEN);
		node->value = aml_allocvalue(0,0,NULL);
		node->value->node = node;
		node->parent = root;
		node->sibling = NULL;

		//rw_enter_write(&aml_nslock);
		*sp = node;
		//rw_exit_write(&aml_nslock);
	}
	return *sp;
}

/* Get absolute pathname of AML node */
const char *
aml_nodename(struct aml_node *node)
{
	static char namebuf[128];

	namebuf[0] = 0;
	if (node) {
		aml_nodename(node->parent);
		if (node->parent != &aml_root)
			strlcat(namebuf, ".", sizeof(namebuf));
		strlcat(namebuf, node->name, sizeof(namebuf));
		return namebuf+1;
	}
	return namebuf;
}

const char *
aml_getname(const char *name)
{
	static char namebuf[128], *p;
	int count;

	p = namebuf;
	while (*name == AMLOP_ROOTCHAR || *name == AMLOP_PARENTPREFIX) {
		*(p++) = *(name++);
	}
	switch (*name) {
	case 0x00:
		count = 0;
		break;
	case AMLOP_MULTINAMEPREFIX:
		count = name[1];
		name += 2;
		break;
	case AMLOP_DUALNAMEPREFIX:
		count = 2;
		name += 1;
		break;
	default:
		count = 1;
	}
	while (count--) {
		memcpy(p, name, 4);
		p[4] = '.';
		p += 5;
		name += 4;
		if (*name == '.') name++;
	}
	*(--p) = 0;
	return namebuf;
}

/* Free all children nodes/values */
void
aml_delchildren(struct aml_node *node)
{
	struct aml_node *onode;

	if (node == NULL)
		return;
	while ((onode = node->child) != NULL) {
		node->child = onode->sibling;

		aml_delchildren(onode);

		/* Decrease reference count */
		aml_xdelref(&onode->value, "");

		/* Delete node */
		acpi_os_free(onode);
	}
}

/*
 * @@@: Value functions
 */

struct aml_scope	*aml_pushscope(struct aml_scope *, uint8_t *,
			    uint8_t *, struct aml_node *);
struct aml_scope	*aml_popscope(struct aml_scope *);

#define AML_LHS		0
#define AML_RHS		1
#define AML_DST		2
#define AML_DST2	3

/* Allocate+push parser scope */
struct aml_scope *
aml_pushscope(struct aml_scope *parent, uint8_t *start, uint8_t  *end,
    struct aml_node *node)
{
	struct aml_scope *scope;

	scope = acpi_os_malloc(sizeof(struct aml_scope));
	scope->pos = start;
	scope->end = end;
	scope->node = node;
	scope->parent = parent;
	scope->sc = dsdt_softc;

	aml_lastscope = scope;

	return scope;
}

struct aml_scope *
aml_popscope(struct aml_scope *scope)
{
	struct aml_scope *nscope;
	struct aml_vallist *ol;
	int idx;

	if (scope == NULL)
		return NULL;
	nscope = scope->parent;

	/* Free temporary values */
	while ((ol = scope->tmpvals) != NULL) {
		scope->tmpvals = ol->next;
		for (idx = 0; idx < ol->nobj; idx++) {
			aml_freevalue(&ol->obj[idx]);
		}
		acpi_os_free(ol);
	}
	acpi_os_free(scope);

	aml_lastscope = nscope;

	return nscope;
}

/*
 * Field I/O code
 */
void aml_unlockfield(struct aml_scope *, struct aml_value *);
void aml_lockfield(struct aml_scope *, struct aml_value *);

long acpi_acquire_global_lock(void*);
long acpi_release_global_lock(void*);
static long global_lock_count = 0;
#define acpi_acquire_global_lock(x) 1 
#define acpi_release_global_lock(x) 0
void
aml_lockfield(struct aml_scope *scope, struct aml_value *field)
{
	int st = 0;

	if (AML_FIELD_LOCK(field->v_field.flags) != AML_FIELD_LOCK_ON)
		return;

	/* If lock is already ours, just continue */
	if (global_lock_count++)
		return;

	/* Spin to acquire lock */
	while (!st) {
		st = acpi_acquire_global_lock(&dsdt_softc->sc_facs->global_lock);
		/* XXX - yield/delay? */
	}

	return;
}

void
aml_unlockfield(struct aml_scope *scope, struct aml_value *field)
{
	int st, x;

	if (AML_FIELD_LOCK(field->v_field.flags) != AML_FIELD_LOCK_ON)
		return;

	/* If we are the last ones, turn out the lights */
	if (--global_lock_count)
		return;

	/* Release lock */
	st = acpi_release_global_lock(&dsdt_softc->sc_facs->global_lock);
	if (!st)
		return;

	/* Signal others if someone waiting */
	x = acpi_read_pmreg(dsdt_softc, ACPIREG_PM1_CNT, 0);
	x |= ACPI_PM1_GBL_RLS;
	acpi_write_pmreg(dsdt_softc, ACPIREG_PM1_CNT, 0, x);

	return;
}

/*
 * @@@: Value set/compare/alloc/free routines
 */

#ifndef SMALL_KERNEL
void
aml_showvalue(struct aml_value *val, int lvl)
{
	int idx;

	if (val == NULL)
		return;

	if (val->node)
		printf(" [%s]", aml_nodename(val->node));
	printf(" %p cnt:%.2x stk:%.2x", val, val->refcnt, val->stack);
	switch (val->type) {
	case AML_OBJTYPE_STATICINT:
	case AML_OBJTYPE_INTEGER:
		printf(" integer: %llx\n", val->v_integer);
		break;
	case AML_OBJTYPE_STRING:
		printf(" string: %s\n", val->v_string);
		break;
	case AML_OBJTYPE_METHOD:
		printf(" method: %.2x\n", val->v_method.flags);
		break;
	case AML_OBJTYPE_PACKAGE:
		printf(" package: %.2x\n", val->length);
		for (idx = 0; idx < val->length; idx++)
			aml_showvalue(val->v_package[idx], lvl);
		break;
	case AML_OBJTYPE_BUFFER:
		printf(" buffer: %.2x {", val->length);
		for (idx = 0; idx < val->length; idx++)
			printf("%s%.2x", idx ? ", " : "", val->v_buffer[idx]);
		printf("}\n");
		break;
	case AML_OBJTYPE_FIELDUNIT:
	case AML_OBJTYPE_BUFFERFIELD:
		printf(" field: bitpos=%.4x bitlen=%.4x ref1:%x ref2:%x [%s]\n",
		    val->v_field.bitpos, val->v_field.bitlen,
		    val->v_field.ref1, val->v_field.ref2,
		    aml_mnem(val->v_field.type, NULL));
		if (val->v_field.ref1)
			printf("  ref1: %s\n", aml_nodename(val->v_field.ref1->node));
		if (val->v_field.ref2)
			printf("  ref2: %s\n", aml_nodename(val->v_field.ref2->node));
		break;
	case AML_OBJTYPE_MUTEX:
		printf(" mutex: %s ref: %d\n",
		    val->v_mutex ?  val->v_mutex->amt_name : "",
		    val->v_mutex ?  val->v_mutex->amt_ref_count : 0);
		break;
	case AML_OBJTYPE_EVENT:
		printf(" event:\n");
		break;
	case AML_OBJTYPE_OPREGION:
		printf(" opregion: %.2x,%.8llx,%x\n",
		    val->v_opregion.iospace, val->v_opregion.iobase,
		    val->v_opregion.iolen);
		break;
	case AML_OBJTYPE_NAMEREF:
		printf(" nameref: %s\n", aml_getname(val->v_nameref));
		break;
	case AML_OBJTYPE_DEVICE:
		printf(" device:\n");
		break;
	case AML_OBJTYPE_PROCESSOR:
		printf(" cpu: %.2x,%.4x,%.2x\n",
		    val->v_processor.proc_id, val->v_processor.proc_addr,
		    val->v_processor.proc_len);
		break;
	case AML_OBJTYPE_THERMZONE:
		printf(" thermzone:\n");
		break;
	case AML_OBJTYPE_POWERRSRC:
		printf(" pwrrsrc: %.2x,%.2x\n",
		    val->v_powerrsrc.pwr_level, val->v_powerrsrc.pwr_order);
		break;
	case AML_OBJTYPE_OBJREF:
		printf(" objref: %p index:%x opcode:%s\n", val->v_objref.ref,
		    val->v_objref.index, aml_mnem(val->v_objref.type, 0));
		aml_showvalue(val->v_objref.ref, lvl);
		break;
	default:
		printf(" !!type: %x\n", val->type);
	}
}
#endif /* SMALL_KERNEL */

int64_t
aml_val2int(struct aml_value *rval)
{
	int64_t ival = 0;

	if (rval == NULL) {
		dnprintf(50, "null val2int\n");
		return (0);
	}
	switch (rval->type) {
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_STATICINT:
		ival = rval->v_integer;
		break;
	case AML_OBJTYPE_BUFFER:
		aml_bufcpy(&ival, 0, rval->v_buffer, 0,
		    min(aml_intlen, rval->length*8));
		break;
	case AML_OBJTYPE_STRING:
		ival = aml_hextoint(rval->v_string);
		break;
	}
	return (ival);
}

/* Sets value into LHS: lhs must already be cleared */
struct aml_value *
_aml_setvalue(struct aml_value *lhs, int type, int64_t ival, const void *bval)
{
	memset(&lhs->_, 0x0, sizeof(lhs->_));

	lhs->type = type;
	switch (lhs->type) {
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_STATICINT:
		lhs->length = aml_intlen>>3;
		lhs->v_integer = ival;
		break;
	case AML_OBJTYPE_METHOD:
		lhs->v_method.flags = ival;
		lhs->v_method.fneval = bval;
		break;
	case AML_OBJTYPE_NAMEREF:
		lhs->v_nameref = (uint8_t *)bval;
		break;
	case AML_OBJTYPE_OBJREF:
		lhs->v_objref.index = ival;
		lhs->v_objref.ref = (struct aml_value *)bval;
		break;
	case AML_OBJTYPE_BUFFER:
		lhs->length = ival;
		lhs->v_buffer = (uint8_t *)acpi_os_malloc(ival);
		if (bval)
			memcpy(lhs->v_buffer, bval, ival);
		break;
	case AML_OBJTYPE_STRING:
		if (ival == -1)
			ival = strlen((const char *)bval);
		lhs->length = ival;
		lhs->v_string = (char *)acpi_os_malloc(ival+1);
		if (bval)
			strncpy(lhs->v_string, (const char *)bval, ival);
		break;
	case AML_OBJTYPE_PACKAGE:
		lhs->length = ival;
		lhs->v_package = (struct aml_value **)acpi_os_malloc(ival *
		    sizeof(struct aml_value *));
		for (ival = 0; ival < lhs->length; ival++)
			lhs->v_package[ival] = aml_allocvalue(
			    AML_OBJTYPE_UNINITIALIZED, 0, NULL);
		break;
	}
	return lhs;
}

/* Copy object to another value: lhs must already be cleared */
void
aml_copyvalue(struct aml_value *lhs, struct aml_value *rhs)
{
	int idx;

	lhs->type = rhs->type  & ~AML_STATIC;
	switch (lhs->type) {
	case AML_OBJTYPE_UNINITIALIZED:
		break;
	case AML_OBJTYPE_STATICINT:
	case AML_OBJTYPE_INTEGER:
		lhs->length = aml_intlen>>3;
		lhs->v_integer = rhs->v_integer;
		break;
	case AML_OBJTYPE_MUTEX:
		lhs->v_mutex = rhs->v_mutex;
		break;
	case AML_OBJTYPE_POWERRSRC:
		lhs->v_powerrsrc = rhs->v_powerrsrc;
		break;
	case AML_OBJTYPE_METHOD:
		lhs->v_method = rhs->v_method;
		break;
	case AML_OBJTYPE_BUFFER:
		_aml_setvalue(lhs, rhs->type, rhs->length, rhs->v_buffer);
		break;
	case AML_OBJTYPE_STRING:
		_aml_setvalue(lhs, rhs->type, rhs->length, rhs->v_string);
		break;
	case AML_OBJTYPE_OPREGION:
		lhs->v_opregion = rhs->v_opregion;
		break;
	case AML_OBJTYPE_PROCESSOR:
		lhs->v_processor = rhs->v_processor;
		break;
	case AML_OBJTYPE_NAMEREF:
		lhs->v_nameref = rhs->v_nameref;
		break;
	case AML_OBJTYPE_PACKAGE:
		_aml_setvalue(lhs, rhs->type, rhs->length, NULL);
		for (idx = 0; idx < rhs->length; idx++)
			aml_copyvalue(lhs->v_package[idx], rhs->v_package[idx]);
		break;
	case AML_OBJTYPE_OBJREF:
		lhs->v_objref = rhs->v_objref;
		break;
	default:
		printf("copyvalue: %x", rhs->type);
		break;
	}
}

/* Allocate dynamic AML value
 *   type : Type of object to allocate (AML_OBJTYPE_XXXX)
 *   ival : Integer value (action depends on type)
 *   bval : Buffer value (action depends on type)
 */
struct aml_value *
aml_allocvalue(int type, int64_t ival, const void *bval)
{
	struct aml_value *rv;

	rv = (struct aml_value *)acpi_os_malloc(sizeof(struct aml_value));
	if (rv != NULL) {
		aml_xaddref(rv, "");
		return _aml_setvalue(rv, type, ival, bval);
	}
	return NULL;
}

void
aml_freevalue(struct aml_value *val)
{
	int idx;

	if (val == NULL)
		return;
	switch (val->type) {
	case AML_OBJTYPE_STRING:
		acpi_os_free(val->v_string);
		break;
	case AML_OBJTYPE_BUFFER:
		acpi_os_free(val->v_buffer);
		break;
	case AML_OBJTYPE_PACKAGE:
		for (idx = 0; idx < val->length; idx++) {
			aml_freevalue(val->v_package[idx]);
			acpi_os_free(val->v_package[idx]);
		}
		acpi_os_free(val->v_package);
		break;
	case AML_OBJTYPE_OBJREF:
		aml_xdelref(&val->v_objref.ref, "");
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		aml_xdelref(&val->v_field.ref1, "");
		aml_xdelref(&val->v_field.ref2, "");
		break;
	}
	val->type = 0;
	memset(&val->_, 0, sizeof(val->_));
}

/*
 * @@@: Math eval routines
 */

/* Convert number from one radix to another
 * Used in BCD conversion routines */
u_int64_t
aml_convradix(u_int64_t val, int iradix, int oradix)
{
	u_int64_t rv = 0, pwr;

	rv = 0;
	pwr = 1;
	while (val) {
		rv += (val % iradix) * pwr;
		val /= iradix;
		pwr *= oradix;
	}
	return rv;
}

/* Calculate LSB */
int
aml_lsb(u_int64_t val)
{
	int		lsb;

	if (val == 0)
		return (0);

	for (lsb = 1; !(val & 0x1); lsb++)
		val >>= 1;

	return (lsb);
}

/* Calculate MSB */
int
aml_msb(u_int64_t val)
{
	int		msb;

	if (val == 0)
		return (0);

	for (msb = 1; val != 0x1; msb++)
		val >>= 1;

	return (msb);
}

/* Evaluate Math operands */
int64_t
aml_evalexpr(int64_t lhs, int64_t rhs, int opcode)
{
	int64_t res;

	switch (opcode) {
		/* Math operations */
	case AMLOP_INCREMENT:
	case AMLOP_ADD:
		res = (lhs + rhs);
		break;
	case AMLOP_DECREMENT:
	case AMLOP_SUBTRACT:
		res = (lhs - rhs);
		break;
	case AMLOP_MULTIPLY:
		res = (lhs * rhs);
		break;
	case AMLOP_DIVIDE:
		res = (lhs / rhs);
		break;
	case AMLOP_MOD:
		res = (lhs % rhs);
		break;
	case AMLOP_SHL:
		res = (lhs << rhs);
		break;
	case AMLOP_SHR:
		res = (lhs >> rhs);
		break;
	case AMLOP_AND:
		res = (lhs & rhs);
		break;
	case AMLOP_NAND:
		res = ~(lhs & rhs);
		break;
	case AMLOP_OR:
		res = (lhs | rhs);
		break;
	case AMLOP_NOR:
		res = ~(lhs | rhs);
		break;
	case AMLOP_XOR:
		res = (lhs ^ rhs);
		break;
	case AMLOP_NOT:
		res = ~(lhs);
		break;

		/* Conversion/misc */
	case AMLOP_FINDSETLEFTBIT:
		res = aml_msb(lhs);
		break;
	case AMLOP_FINDSETRIGHTBIT:
		res = aml_lsb(lhs);
		break;
	case AMLOP_TOINTEGER:
		res = (lhs);
		break;
	case AMLOP_FROMBCD:
		res = aml_convradix(lhs, 16, 10);
		break;
	case AMLOP_TOBCD:
		res = aml_convradix(lhs, 10, 16);
		break;

		/* Logical/Comparison */
	case AMLOP_LAND:
		res = (lhs && rhs);
		break;
	case AMLOP_LOR:
		res = (lhs || rhs);
		break;
	case AMLOP_LNOT:
		res = (!lhs);
		break;
	case AMLOP_LNOTEQUAL:
		res = (lhs != rhs);
		break;
	case AMLOP_LLESSEQUAL:
		res = (lhs <= rhs);
		break;
	case AMLOP_LGREATEREQUAL:
		res = (lhs >= rhs);
		break;
	case AMLOP_LEQUAL:
		res = (lhs == rhs);
		break;
	case AMLOP_LGREATER:
		res = (lhs > rhs);
		break;
	case AMLOP_LLESS:
		res = (lhs < rhs);
		break;
	}

	dnprintf(15,"aml_evalexpr: %s %llx %llx = %llx\n",
		 aml_mnem(opcode, NULL), lhs, rhs, res);

	return res;
}

/*
 * aml_bufcpy copies/shifts buffer data, special case for aligned transfers
 * dstPos/srcPos are bit positions within destination/source buffers
 */
void
aml_bufcpy(void *pvDst, int dstPos, const void *pvSrc, int srcPos, int len)
{
	const u_int8_t *pSrc = pvSrc;
	u_int8_t *pDst = pvDst;
	int		idx;

	if (aml_bytealigned(dstPos|srcPos|len)) {
		/* Aligned transfer: use memcpy */
		memcpy(pDst+aml_bytepos(dstPos), pSrc+aml_bytepos(srcPos),
		    aml_bytelen(len));
		return;
	}

	/* Misaligned transfer: perform bitwise copy (slow) */
	for (idx = 0; idx < len; idx++)
		aml_setbit(pDst, idx + dstPos, aml_tstbit(pSrc, idx + srcPos));
}

/*
 * @@@: External API
 *
 * evaluate an AML node
 * Returns a copy of the value in res  (must be freed by user)
 */

void
aml_walknodes(struct aml_node *node, int mode,
    int (*nodecb)(struct aml_node *, void *), void *arg)
{
	struct aml_node *child;

	if (node == NULL)
		return;
	if (mode == AML_WALK_PRE)
		nodecb(node, arg);
	for (child = node->child; child; child = child->sibling)
		aml_walknodes(child, mode, nodecb, arg);
	if (mode == AML_WALK_POST)
		nodecb(node, arg);
}

int
aml_find_node(struct aml_node *node, const char *name,
    int (*cbproc)(struct aml_node *, void *arg), void *arg)
{
	const char *nn;
	int st = 0;

	while (node) {
		if ((nn = node->name) != NULL) {
			if (*nn == AMLOP_ROOTCHAR) nn++;
			while (*nn == AMLOP_PARENTPREFIX) nn++;
			if (!strcmp(name, nn))
				st = cbproc(node, arg);
		}
		/* Only recurse if cbproc() wants us to */
		if (!st)
			aml_find_node(node->child, name, cbproc, arg);
		node = node->sibling;
	}
	return st;
}

/*
 * @@@: Parser functions
 */
uint8_t *aml_parsename(struct aml_scope *);
uint8_t *aml_parseend(struct aml_scope *scope);
int	aml_parselength(struct aml_scope *);
int	aml_parseopcode(struct aml_scope *);

/* Get AML Opcode */
int
aml_parseopcode(struct aml_scope *scope)
{
	int opcode = (scope->pos[0]);
	int twocode = (scope->pos[0]<<8) + scope->pos[1];

	/* Check if this is an embedded name */
	switch (opcode) {
	case AMLOP_ROOTCHAR:
	case AMLOP_PARENTPREFIX:
	case AMLOP_MULTINAMEPREFIX:
	case AMLOP_DUALNAMEPREFIX:
	case AMLOP_NAMECHAR:
		return AMLOP_NAMECHAR;
	}
	if (opcode >= 'A' && opcode <= 'Z') {
		return AMLOP_NAMECHAR;
	}
	if (twocode == AMLOP_LNOTEQUAL || twocode == AMLOP_LLESSEQUAL ||
	    twocode == AMLOP_LGREATEREQUAL || opcode == AMLOP_EXTPREFIX) {
		scope->pos += 2;
		return twocode;
	}
	scope->pos += 1;
	return opcode;
}

/* Decode embedded AML Namestring */
uint8_t *
aml_parsename(struct aml_scope *scope)
{
	uint8_t *name = scope->pos;

	while (*scope->pos == AMLOP_ROOTCHAR || *scope->pos == AMLOP_PARENTPREFIX)
		scope->pos++;

	switch (*scope->pos) {
	case 0x00:
		break;
	case AMLOP_MULTINAMEPREFIX:
		scope->pos += 2+AML_NAMESEG_LEN*scope->pos[1];
		break;
	case AMLOP_DUALNAMEPREFIX:
		scope->pos += 1+AML_NAMESEG_LEN*2;
		break;
	default:
		scope->pos += AML_NAMESEG_LEN;
		break;
	}
	return name;
}

/* Decode AML Length field
 *  AML Length field is encoded:
 *    byte0    byte1    byte2    byte3
 *    00xxxxxx                             : if upper bits == 00, length = xxxxxx
 *    01--xxxx yyyyyyyy                    : if upper bits == 01, length = yyyyyyyyxxxx
 *    10--xxxx yyyyyyyy zzzzzzzz           : if upper bits == 10, length = zzzzzzzzyyyyyyyyxxxx
 *    11--xxxx yyyyyyyy zzzzzzzz wwwwwwww  : if upper bits == 11, length = wwwwwwwwzzzzzzzzyyyyyyyyxxxx
 */
int
aml_parselength(struct aml_scope *scope)
{
	int len;
	uint8_t lcode;

	lcode = *(scope->pos++);
	if (lcode <= 0x3F) {
		return lcode;
	}

	/* lcode >= 0x40, multibyte length, get first byte of extended length */
	len = lcode & 0xF;
	len += *(scope->pos++) << 4L;
	if (lcode >= 0x80) {
		len += *(scope->pos++) << 12L;
	}
	if (lcode >= 0xC0) {
		len += *(scope->pos++) << 20L;
	}
	return len;
}

/* Get address of end of scope; based on current address */
uint8_t *
aml_parseend(struct aml_scope *scope)
{
	uint8_t *pos = scope->pos;
	int len;

	len = aml_parselength(scope);
	if (pos+len > scope->end) {
		dnprintf(10,
		    "Bad scope... runover pos:%.4x new end:%.4x scope "
		    "end:%.4x\n", aml_pc(pos), aml_pc(pos+len),
		    aml_pc(scope->end));
		pos = scope->end;
	}
	return pos+len;
}

/*
 * @@@: Opcode utility functions
 */

int amlop_delay;

u_int64_t
aml_getpciaddr(struct acpi_softc *sc, struct aml_node *root)
{
	struct aml_value tmpres;
	u_int64_t pciaddr;

	/* PCI */
	pciaddr = 0;
	if (!aml_evalname(dsdt_softc, root, "_ADR", 0, NULL, &tmpres)) {
		/* Device:Function are bits 16-31,32-47 */
		pciaddr += (aml_val2int(&tmpres) << 16L);
		aml_freevalue(&tmpres);
		dnprintf(20, "got _adr [%s]\n", aml_nodename(root));
	} else {
		/* Mark invalid */
		pciaddr += (0xFFFF << 16L);
		return pciaddr;
	}

	if (!aml_evalname(dsdt_softc, root, "_BBN", 0, NULL, &tmpres)) {
		/* PCI bus is in bits 48-63 */
		pciaddr += (aml_val2int(&tmpres) << 48L);
		aml_freevalue(&tmpres);
		dnprintf(20, "got _bbn [%s]\n", aml_nodename(root));
	}
	dnprintf(20, "got pciaddr: %s:%llx\n", aml_nodename(root), pciaddr);
	return pciaddr;
}

/* Fixup references for BufferFields/FieldUnits */
#if 0
void		aml_fixref(struct aml_value **);

void
aml_fixref(struct aml_value **res)
{
	struct aml_value *oldres;

	while (*res && (*res)->type == AML_OBJTYPE_OBJREF &&
	    (*res)->v_objref.index == -1) {
		oldres = (*res)->v_objref.ref;
		aml_delref(res);
		aml_addref(oldres);
		*res = oldres;
	}
}
#endif

/*
 * @@@: Opcode functions
 */

int odp;

const char hext[] = "0123456789ABCDEF";

const char *
aml_eisaid(u_int32_t pid)
{
	static char id[8];

	id[0] = '@' + ((pid >> 2) & 0x1F);
	id[1] = '@' + ((pid << 3) & 0x18) + ((pid >> 13) & 0x7);
	id[2] = '@' + ((pid >> 8) & 0x1F);
	id[3] = hext[(pid >> 20) & 0xF];
	id[4] = hext[(pid >> 16) & 0xF];
	id[5] = hext[(pid >> 28) & 0xF];
	id[6] = hext[(pid >> 24) & 0xF];
	id[7] = 0;
	return id;
}

#if 0
/*
 * @@@: Fixup DSDT code
 */
struct aml_fixup {
	int		offset;
	u_int8_t	oldv, newv;
} __ibm300gl[] = {
	{ 0x19, 0x3a, 0x3b },
	{ -1 }
};

struct aml_blacklist {
	const char	*oem, *oemtbl;
	struct aml_fixup *fixtab;
	u_int8_t	cksum;
} amlfix_list[] = {
	{ "IBM   ", "CDTPWSNH", __ibm300gl, 0x41 },
	{ NULL },
};

void
aml_fixup_dsdt(u_int8_t *acpi_hdr, u_int8_t *base, int len)
{
	struct acpi_table_header *hdr = (struct acpi_table_header *)acpi_hdr;
	struct aml_blacklist *fixlist;
	struct aml_fixup *fixtab;

	for (fixlist = amlfix_list; fixlist->oem; fixlist++) {
		if (!memcmp(fixlist->oem, hdr->oemid, 6) &&
		    !memcmp(fixlist->oemtbl, hdr->oemtableid, 8) &&
		    fixlist->cksum == hdr->checksum) {
			/* Found a potential fixup entry */
			for (fixtab = fixlist->fixtab; fixtab->offset != -1;
			    fixtab++) {
				if (base[fixtab->offset] == fixtab->oldv)
					base[fixtab->offset] = fixtab->newv;
			}
		}
	}
}
#endif

/*
 * @@@: Default Object creation
 */
static char osstring[] = "Macrosift Windogs MT";
struct aml_defval {
	const char		*name;
	int			type;
	int64_t			ival;
	const void		*bval;
	struct aml_value	**gval;
} aml_defobj[] = {
	{ "_OS_", AML_OBJTYPE_STRING, -1, osstring },
	{ "_REV", AML_OBJTYPE_INTEGER, 2, NULL },
	{ "_GL", AML_OBJTYPE_MUTEX, 1, NULL, &aml_global_lock },
	{ "_OSI", AML_OBJTYPE_METHOD, 1, aml_callosi },
	{ NULL }
};

/* _OSI Default Method:
 * Returns True if string argument matches list of known OS strings
 * We return True for Windows to fake out nasty bad AML
 */
char *aml_valid_osi[] = {
	"Windows 2000",
	"Windows 2001",
	"Windows 2001.1",
	"Windows 2001 SP0",
	"Windows 2001 SP1",
	"Windows 2001 SP2",
	"Windows 2001 SP3",
	"Windows 2001 SP4",
	"Windows 2006",
	NULL
};

struct aml_value *
aml_callosi(struct aml_scope *scope, struct aml_value *val)
{
	int idx, result=0;
	struct aml_value *fa;

	fa = aml_getstack(scope, AMLOP_ARG0);
	for (idx=0; !result && aml_valid_osi[idx] != NULL; idx++) {
		dnprintf(10,"osi: %s,%s\n", fa->v_string, aml_valid_osi[idx]);
		result = !strcmp(fa->v_string, aml_valid_osi[idx]);
	}
	dnprintf(10,"@@ OSI found: %x\n", result);
	return aml_allocvalue(AML_OBJTYPE_INTEGER, result, NULL);
}

void
aml_create_defaultobjects()
{
	struct aml_value *tmp;
	struct aml_defval *def;

	osstring[1] = 'i';
	osstring[6] = 'o';
	osstring[15] = 'w';
	osstring[18] = 'N';

	strlcpy(aml_root.name, "\\", sizeof(aml_root.name));
	aml_root.value = aml_allocvalue(0, 0, NULL);
	aml_root.value->node = &aml_root;

	for (def = aml_defobj; def->name; def++) {
		/* Allocate object value + add to namespace */
		aml_xparsename((uint8_t *)def->name, &aml_root,
		    ns_xcreate, &tmp);
		_aml_setvalue(tmp, def->type, def->ival, def->bval);
		if (def->gval) {
			/* Set root object pointer */
			*def->gval = tmp;
		}
	}
}

#ifdef ACPI_DEBUG
int
aml_print_resource(union acpi_resource *crs, void *arg)
{
	int typ = AML_CRSTYPE(crs);

	switch (typ) {
	case LR_EXTIRQ:
		printf("extirq\tflags:%.2x len:%.2x irq:%.4x\n",
		    crs->lr_extirq.flags, crs->lr_extirq.irq_count,
		    aml_letohost32(crs->lr_extirq.irq[0]));
		break;
	case SR_IRQ:
		printf("irq\t%.4x %.2x\n", aml_letohost16(crs->sr_irq.irq_mask),
		    crs->sr_irq.irq_flags);
		break;
	case SR_DMA:
		printf("dma\t%.2x %.2x\n", crs->sr_dma.channel,
		    crs->sr_dma.flags);
		break;
	case SR_IOPORT:
		printf("ioport\tflags:%.2x _min:%.4x _max:%.4x _aln:%.2x _len:%.2x\n",
		    crs->sr_ioport.flags, crs->sr_ioport._min,
		    crs->sr_ioport._max, crs->sr_ioport._aln,
		    crs->sr_ioport._len);
		break;
	case SR_STARTDEP:
		printf("startdep\n");
		break;
	case SR_ENDDEP:
		printf("enddep\n");
		break;
	case LR_WORD:
		printf("word\ttype:%.2x flags:%.2x tflag:%.2x gra:%.4x min:%.4x max:%.4x tra:%.4x len:%.4x\n",
			crs->lr_word.type, crs->lr_word.flags, crs->lr_word.tflags,
			crs->lr_word._gra, crs->lr_word._min, crs->lr_word._max,
			crs->lr_word._tra, crs->lr_word._len);
		break;
	case LR_DWORD:
		printf("dword\ttype:%.2x flags:%.2x tflag:%.2x gra:%.8x min:%.8x max:%.8x tra:%.8x len:%.8x\n",
			crs->lr_dword.type, crs->lr_dword.flags, crs->lr_dword.tflags,
			crs->lr_dword._gra, crs->lr_dword._min, crs->lr_dword._max,
			crs->lr_dword._tra, crs->lr_dword._len);
		break;
	case LR_QWORD:
		printf("dword\ttype:%.2x flags:%.2x tflag:%.2x gra:%.16llx min:%.16llx max:%.16llx tra:%.16llx len:%.16llx\n",
			crs->lr_qword.type, crs->lr_qword.flags, crs->lr_qword.tflags,
			crs->lr_qword._gra, crs->lr_qword._min, crs->lr_qword._max,
			crs->lr_qword._tra, crs->lr_qword._len);
		break;
	default:
		printf("unknown type: %x\n", typ);
		break;
	}
	return (0);
}
#endif /* ACPI_DEBUG */

union acpi_resource *aml_mapresource(union acpi_resource *);

union acpi_resource *
aml_mapresource(union acpi_resource *crs)
{
	static union acpi_resource map;
	int rlen;

	rlen = AML_CRSLEN(crs);
	if (rlen >= sizeof(map))
		return crs;

	memset(&map, 0, sizeof(map));
	memcpy(&map, crs, rlen);

	return &map;
}

int
aml_parse_resource(int length, uint8_t *buffer,
    int (*crs_enum)(union acpi_resource *, void *), void *arg)
{
	int off, rlen;
	union acpi_resource *crs;

	for (off = 0; off < length; off += rlen) {
		crs = (union acpi_resource *)(buffer+off);

		rlen = AML_CRSLEN(crs);
		if (crs->hdr.typecode == 0x79 || rlen <= 3)
			break;

		crs = aml_mapresource(crs);
#ifdef ACPI_DEBUG
		aml_print_resource(crs, NULL);
#endif
		crs_enum(crs, arg);
	}

	return 0;
}

void
aml_foreachpkg(struct aml_value *pkg, int start,
    void (*fn)(struct aml_value *, void *), void *arg)
{
	int idx;

	if (pkg->type != AML_OBJTYPE_PACKAGE)
		return;
	for (idx=start; idx<pkg->length; idx++)
		fn(pkg->v_package[idx], arg);
}

#if 0
int
acpi_parse_aml(struct acpi_softc *sc, u_int8_t *start, u_int32_t length)
{
	u_int8_t *end;

	dsdt_softc = sc;

	strlcpy(aml_root.name, "\\", sizeof(aml_root.name));
	if (aml_root.start == NULL) {
		aml_root.start = start;
		aml_root.end = start+length;
	}
	end = start+length;
	aml_parsenode(NULL, &aml_root, start, &end, NULL);
	dnprintf(50, " : parsed %d AML bytes\n", length);

	return (0);
}
#endif

/*
 * Walk nodes and perform fixups for nameref
 */
int aml_fixup_node(struct aml_node *, void *);

int aml_fixup_node(struct aml_node *node, void *arg)
{
	struct aml_value *val = arg;
	int i;

	if (node->value == NULL)
		return (0);
	if (arg == NULL)
		aml_fixup_node(node, node->value);
	else if (val->type == AML_OBJTYPE_NAMEREF) {
		node = aml_searchname(node, val->v_nameref);
		if (node && node->value) {
			_aml_setvalue(val, AML_OBJTYPE_OBJREF, -1,
			    node->value);
		}
	} else if (val->type == AML_OBJTYPE_PACKAGE) {
		for (i = 0; i < val->length; i++)
			aml_fixup_node(node, val->v_package[i]);
	} else if (val->type == AML_OBJTYPE_OPREGION) {
		if (val->v_opregion.iospace != GAS_PCI_CFG_SPACE)
			return (0);
		if (ACPI_PCI_FN(val->v_opregion.iobase) != 0xFFFF)
			return (0);
		val->v_opregion.iobase =
		    ACPI_PCI_REG(val->v_opregion.iobase) +
		    aml_getpciaddr(dsdt_softc, node);
		dnprintf(20, "late ioaddr : %s:%llx\n",
		    aml_nodename(node), val->v_opregion.iobase);
	}
	return (0);
}

void
aml_postparse()
{
	aml_walknodes(&aml_root, AML_WALK_PRE, aml_fixup_node, NULL);
}

const char *
aml_val_to_string(const struct aml_value *val)
{
	static char buffer[256];

	int len;

	switch (val->type) {
	case AML_OBJTYPE_BUFFER:
		len = val->length;
		if (len >= sizeof(buffer))
			len = sizeof(buffer) - 1;
		memcpy(buffer, val->v_buffer, len);
		buffer[len] = 0;
		break;
	case AML_OBJTYPE_STRING:
		strlcpy(buffer, val->v_string, sizeof(buffer));
		break;
	case AML_OBJTYPE_INTEGER:
		snprintf(buffer, sizeof(buffer), "%llx", val->v_integer);
		break;
	default:
		snprintf(buffer, sizeof(buffer),
		    "Failed to convert type %d to string!", val->type);
	};

	return (buffer);
}

/*
 * XXX: NEW PARSER CODE GOES HERE 
 */
struct aml_value *aml_xeval(struct aml_scope *, struct aml_value *, int, int,
    struct aml_value *);
struct aml_value *aml_xparsesimple(struct aml_scope *, char, 
    struct aml_value *);
struct aml_value *aml_xparse(struct aml_scope *, int, const char *);

struct aml_scope *aml_xfindscope(struct aml_scope *, int, int);
struct aml_scope *aml_xpushscope(struct aml_scope *, struct aml_value *, 
    struct aml_node *, int);
struct aml_scope *aml_xpopscope(struct aml_scope *);

void		aml_showstack(struct aml_scope *);
void		aml_xconvert(struct aml_value *, struct aml_value **, int, int);

int		aml_xmatchtest(int64_t, int64_t, int);
int		aml_xmatch(struct aml_value *, int, int, int, int, int);

int		aml_xcompare(struct aml_value *, struct aml_value *, int);
void		aml_xconcat(struct aml_value *, struct aml_value *, 
    struct aml_value **);
void		aml_xconcatres(struct aml_value *, struct aml_value *, 
    struct aml_value **);
int		aml_ccrlen(union acpi_resource *, void *);
void		aml_xmid(struct aml_value *, int, int, struct aml_value **);

void		aml_xstore(struct aml_scope *, struct aml_value *, int64_t, 
    struct aml_value *);

int
valid_acpihdr(void *buf, int len, const char *sig)
{
	struct acpi_table_header *hdr = buf;

	if (sig && strncmp(hdr->signature, sig, 4)) {
		return 0;
	}
	if (len < hdr->length) {
		return 0;
	}
	if (acpi_checksum(hdr, hdr->length) != 0) {
		return 0;
	}
	return 1; 
}

/*
 * Reference Count functions
 */
void
aml_xaddref(struct aml_value *val, const char *lbl)
{
	if (val == NULL)
		return;
	dnprintf(50, "XAddRef: %p %s:[%s] %d\n", 
	    val, lbl,
	    val->node ? aml_nodename(val->node) : "INTERNAL",
	    val->refcnt);
	val->refcnt++;
}

/* Decrease reference counter */
void
aml_xdelref(struct aml_value **pv, const char *lbl)
{
	struct aml_value *val;

	if (pv == NULL || *pv == NULL)
		return;
	val = *pv;
	val->refcnt--;
	if (val->refcnt == 0) {
		dnprintf(50, "XDelRef: %p %s %2d [%s] %s\n", 
		    val, lbl,
		    val->refcnt,
		    val->node ? aml_nodename(val->node) : "INTERNAL",
		    val->refcnt ? "" : "---------------- FREEING");

		aml_freevalue(val);
		acpi_os_free(val);
		*pv = NULL;
	}
}

/* Walk list of parent scopes until we find one of 'type'
 * If endscope is set, mark all intermediate scopes as invalid (used for Method/While) */
struct aml_scope *
aml_xfindscope(struct aml_scope *scope, int type, int endscope)
{
	while (scope) { 
		if (endscope)
			scope->pos = NULL;
		if (scope->type == type)
			break;
		scope = scope->parent;
	}
	return scope;
}

struct aml_value *
aml_getstack(struct aml_scope *scope, int opcode)
{
  	struct aml_value *sp;

	sp = NULL;
	scope = aml_xfindscope(scope, AMLOP_METHOD, 0);
	if (scope == NULL)
		return NULL;
	if (opcode >= AMLOP_LOCAL0 && opcode <= AMLOP_LOCAL7) {
		if (scope->locals == NULL)
			scope->locals = aml_allocvalue(AML_OBJTYPE_PACKAGE, 8, NULL);
		sp = scope->locals->v_package[opcode - AMLOP_LOCAL0];
		sp->stack = opcode;
	}
	else if (opcode >= AMLOP_ARG0 && opcode <= AMLOP_ARG6) {
	  	if (scope->args == NULL)
			scope->args = aml_allocvalue(AML_OBJTYPE_PACKAGE, 7, NULL);
		sp = scope->args->v_package[opcode - AMLOP_ARG0];
		if (sp->type == AML_OBJTYPE_OBJREF)
			sp = sp->v_objref.ref;
	}
	return sp;
}

#ifdef ACPI_DEBUG
/* Dump AML Stack */
void 
aml_showstack(struct aml_scope *scope)
{
	struct aml_value *sp;
	int idx;

	dnprintf(10, "===== Stack %s:%s\n", aml_nodename(scope->node), 
	    aml_mnem(scope->type, 0));
	for (idx=0; scope->args && idx<7; idx++) {
		sp = aml_getstack(scope, AMLOP_ARG0+idx);
		if (sp && sp->type) {
			dnprintf(10," Arg%d: ", idx);
			aml_showvalue(sp, 10);
		}
	}
	for (idx=0; scope->locals && idx<8; idx++) {
		sp = aml_getstack(scope, AMLOP_LOCAL0+idx);
		if (sp && sp->type) {
			dnprintf(10," Local%d: ", idx);
			aml_showvalue(sp, 10);
		}
	}
}
#endif

/* Create a new scope object */
struct aml_scope *
aml_xpushscope(struct aml_scope *parent, struct aml_value *range,
    struct aml_node *node, int type)
{
	struct aml_scope *scope;
	uint8_t *start, *end;

	if (range->type == AML_OBJTYPE_METHOD) {
		start = range->v_method.start;
		end = range->v_method.end;
	}
	else {
		start = range->v_buffer;
		end = start + range->length;
		if (start == end) {
			return NULL;
		}
	}
	scope = acpi_os_malloc(sizeof(struct aml_scope));
	if (scope == NULL)
		return NULL;

	scope->node = node;
	scope->start = start;
	scope->end = end;
	scope->pos = scope->start;
	scope->parent = parent;
	scope->type = type;
	scope->sc = dsdt_softc;

	aml_lastscope = scope;

	return scope;
}

/* Free a scope object and any children */
struct aml_scope *
aml_xpopscope(struct aml_scope *scope)
{
	struct aml_scope *nscope;

	if (scope == NULL)
		return NULL;

	nscope = scope->parent;

	if (scope->type == AMLOP_METHOD) {
		aml_delchildren(scope->node);
	}
	if (scope->locals) {
		aml_freevalue(scope->locals);
		acpi_os_free(scope->locals);
		scope->locals = NULL;
	}
	if (scope->args) {
		aml_freevalue(scope->args);
		acpi_os_free(scope->args);
		scope->args = NULL;
	}
	acpi_os_free(scope);
	aml_lastscope = nscope;

	return nscope;
}

/* Test AMLOP_MATCH codes */
int
aml_xmatchtest(int64_t a, int64_t b, int op)
{
	switch (op) {
	case AML_MATCH_TR:
		return (1);
	case AML_MATCH_EQ:
		return (a == b);
	case AML_MATCH_LT:
		return (a < b);
	case AML_MATCH_LE:
		return (a <= b);
	case AML_MATCH_GE:
		return (a >= b);
	case AML_MATCH_GT:
		return (a > b);
	}
	return 0;
}

/* Search a package for a matching value */
int
aml_xmatch(struct aml_value *pkg, int index,
	   int op1, int v1,
	   int op2, int v2)
{
	struct aml_value *tmp;
	int flag;

	while (index < pkg->length) {
		/* Convert package value to integer */
		aml_xconvert(pkg->v_package[index], &tmp, 
		    AML_OBJTYPE_INTEGER, 0);

		/* Perform test */
		flag = aml_xmatchtest(tmp->v_integer, v1, op1) && 
		    aml_xmatchtest(tmp->v_integer, v2, op2);
		aml_xdelref(&tmp, "xmatch");

		if (flag)
			return index;
		index++;
	}
	return -1;
}

/*
 * Namespace functions
 */

/* Search for name in namespace */
void
ns_xsearch(struct aml_node *node, int n, uint8_t *pos, void *arg)
{
	struct aml_value **rv = arg;
	struct aml_node *rnode;

	/* If name search is relative, check up parent nodes */
	for (rnode=node; n == 1 && rnode; rnode=rnode->parent) {
		if (__aml_search(rnode, pos, 0) != NULL) {
			break;
		}
	}
	while (n--) {
		rnode = __aml_search(rnode, pos, 0);
		pos += 4;
	}
	if (rnode != NULL) {
		*rv = rnode->value;
		return;
	}
	*rv = NULL;
}

/* Create name in namespace */
void
ns_xcreate(struct aml_node *node, int n, uint8_t *pos, void *arg)
{
	struct aml_value **rv = arg;

	while (n--) {
		node = __aml_search(node, pos, 1);
		pos += 4;
	}
	*rv = node->value;
}

void
ns_xdis(struct aml_node *node, int n, uint8_t *pos, void *arg)
{
	printf(aml_nodename(node));
	while (n--) {
		printf("%s%c%c%c%c", n ? "." : "", 
		    pos[0], pos[1], pos[2], pos[3]);
		pos+=4;
	}
}

uint8_t *
aml_xparsename(uint8_t *pos, struct aml_node *node, 
    void (*fn)(struct aml_node *, int, uint8_t *, void *), void *arg)
{
	uint8_t *rpos = pos;
	struct aml_value **rv = arg;

	if (*pos == AMLOP_ROOTCHAR) {
		node = &aml_root;
		pos++;
	}
	while (*pos == AMLOP_PARENTPREFIX) {
		node = node ? node->parent : &aml_root;
		pos++;
	}
	if (*pos == 0) {
		fn(node, 0, pos, arg);
		pos++;
	}
	else if (*pos == AMLOP_MULTINAMEPREFIX) {
		fn(node, pos[1], pos+2, arg);
		pos += 2 + 4 * pos[1];
	}
	else if (*pos == AMLOP_DUALNAMEPREFIX) {
		fn(node, 2, pos+1, arg);
		pos += 9;
	}
	else if (*pos == '_' || (*pos >= 'A' && *pos <= 'Z')) {
		fn(node, 1, pos, arg);
		pos += 4;
	}
	else {
		printf("Invalid name!!!\n");
	}
	if (rv && *rv == NULL) {
		*rv = aml_allocvalue(AML_OBJTYPE_NAMEREF, 0, rpos);
	}
	return pos;
}

/*
 * Conversion routines
 */
int64_t
aml_hextoint(const char *str)
{
	int64_t v = 0;
	char c;

	while (*str) {
		if (*str >= '0' && *str <= '9') {
			c = *(str++) - '0';
		}
		else if (*str >= 'a' && *str <= 'f') {
			c = *(str++) - 'a' + 10;
		}
		else if (*str >= 'A' && *str <= 'F') {
			c = *(str++) - 'A' + 10;
		}
		else {
			break;
		}
		v = (v << 4) + c;
	}
	return v;

}

void
aml_xconvert(struct aml_value *a, struct aml_value **b, int ctype, int mode)
{
	struct aml_value *c = NULL;

	/* Object is already this type */
	if (a->type == ctype) {
		aml_xaddref(a, "XConvert");
		*b = a;
		return;
	}
	switch (ctype) {
	case AML_OBJTYPE_BUFFER:
		dnprintf(10,"convert to buffer\n");
		switch (a->type) {
		case AML_OBJTYPE_INTEGER:
			c = aml_allocvalue(AML_OBJTYPE_BUFFER, a->length, 
			    &a->v_integer);
			break;
		case AML_OBJTYPE_STRING:
			c = aml_allocvalue(AML_OBJTYPE_BUFFER, a->length, 
			    a->v_string);
			break;
		}
		break;
	case AML_OBJTYPE_INTEGER:
		dnprintf(10,"convert to integer : %x\n", a->type);
		switch (a->type) {
		case AML_OBJTYPE_BUFFER:
			c = aml_allocvalue(AML_OBJTYPE_INTEGER, 0, NULL);
			memcpy(&c->v_integer, a->v_buffer, 
			    min(a->length, c->length));
			break;
		case AML_OBJTYPE_STRING:
			c = aml_allocvalue(AML_OBJTYPE_INTEGER, 0, NULL);
			c->v_integer = aml_hextoint(a->v_string);
			break;
		case AML_OBJTYPE_UNINITIALIZED:
			c = aml_allocvalue(AML_OBJTYPE_INTEGER, 0, NULL);
			break;
		}
		break;
	case AML_OBJTYPE_STRING:
		dnprintf(10,"convert to string\n");
		switch (a->type) {
		case AML_OBJTYPE_INTEGER:
			c = aml_allocvalue(AML_OBJTYPE_STRING, 20, NULL);
			snprintf(c->v_string, c->length, (mode == 'x') ? 
			    "0x%llx" : "%lld", a->v_integer);
			break;
		case AML_OBJTYPE_BUFFER:
			c = aml_allocvalue(AML_OBJTYPE_STRING, a->length,
			    a->v_buffer);
			break;
		}
		break;
	}
	if (c == NULL) {
#ifndef SMALL_KERNEL
		aml_showvalue(a, 0);
#endif
		aml_die("Could not convert!!!\n");
	}
	*b = c;
}

int
aml_xcompare(struct aml_value *a1, struct aml_value *a2, int opcode)
{
	int rc = 0;

	/* Convert A2 to type of A1 */
	aml_xconvert(a2, &a2, a1->type, 0);
	if (a1->type == AML_OBJTYPE_INTEGER) {
		rc = aml_evalexpr(a1->v_integer, a2->v_integer, opcode);
	}
	else {
		/* Perform String/Buffer comparison */
		rc = memcmp(a1->v_buffer, a2->v_buffer, 
		    min(a1->length, a2->length));
		if (rc == 0) {
			/* If buffers match, which one is longer */
			rc = a1->length - a2->length;
		}
		/* Perform comparison against zero */
		rc = aml_evalexpr(rc, 0, opcode);
	}
	/* Either deletes temp buffer, or decrease refcnt on original A2 */
	aml_xdelref(&a2, "xcompare");
	return rc;
}

/* Concatenate two objects, returning pointer to new object */
void
aml_xconcat(struct aml_value *a1, struct aml_value *a2, struct aml_value **res)
{
	struct aml_value *c;

	/* Convert arg2 to type of arg1 */
	aml_xconvert(a2, &a2, a1->type, 0);
	switch (a1->type) {
	case AML_OBJTYPE_INTEGER:
		c = aml_allocvalue(AML_OBJTYPE_BUFFER, 
		    a1->length + a2->length, NULL);
		memcpy(c->v_buffer, &a1->v_integer, a1->length);
		memcpy(c->v_buffer+a1->length, &a2->v_integer, a2->length);
		break;
	case AML_OBJTYPE_BUFFER:
		c = aml_allocvalue(AML_OBJTYPE_BUFFER, 
		    a1->length + a2->length, NULL);
		memcpy(c->v_buffer, a1->v_buffer, a1->length);
		memcpy(c->v_buffer+a1->length, a2->v_buffer, a2->length);
		break;
	case AML_OBJTYPE_STRING:
		c = aml_allocvalue(AML_OBJTYPE_STRING, 
		    a1->length + a2->length, NULL);
		memcpy(c->v_string, a1->v_string, a1->length);
		memcpy(c->v_string+a1->length, a2->v_string, a2->length);
		break;
	default:
		aml_die("concat type mismatch %d != %d\n", a1->type, a2->type);
		break;
	}
	/* Either deletes temp buffer, or decrease refcnt on original A2 */
	aml_xdelref(&a2, "xconcat");
	*res = c;
}

/* Calculate length of Resource Template */
int
aml_ccrlen(union acpi_resource *rs, void *arg)
{
	int *plen = arg;
	
	*plen += AML_CRSLEN(rs);
	return 0;
}

/* Concatenate resource templates, returning pointer to new object */
void
aml_xconcatres(struct aml_value *a1, struct aml_value *a2, struct aml_value **res)
{
	struct aml_value *c;
	int l1 = 0, l2 = 0;

	if (a1->type != AML_OBJTYPE_BUFFER || a2->type != AML_OBJTYPE_BUFFER) {
		aml_die("concatres: not buffers\n");
	}

	/* Walk a1, a2, get length minus end tags, concatenate buffers, add end tag */
	aml_parse_resource(a1->length, a1->v_buffer, aml_ccrlen, &l1);
	aml_parse_resource(a2->length, a2->v_buffer, aml_ccrlen, &l2);

	/* Concatenate buffers, add end tag */
	c = aml_allocvalue(AML_OBJTYPE_BUFFER, l1+l2+2, NULL);
	memcpy(c->v_buffer,    a1->v_buffer, l1);
	memcpy(c->v_buffer+l1, a2->v_buffer, l2);
	c->v_buffer[l1+l2+0] = 0x79;
	c->v_buffer[l1+l2+1] = 0x00;

	*res = c;
}

/* Extract substring from string or buffer */
void
aml_xmid(struct aml_value *src, int index, int length, struct aml_value **res)
{
	int idx;

	for (idx=index; idx<index+length; idx++) {
		if (idx >= src->length)
			break;
		if (src->v_buffer[idx] == 0)
			break;
	}
	aml_die("mid\n");
}		

/*
 * Field I/O utility functions 
 */
void  aml_xresolve(struct aml_scope *, struct aml_value *);
void *aml_xgetptr(struct aml_value *, int);
void aml_xgasio(int, uint64_t, int, void *, int, int, const char *);
void aml_xfldio(struct aml_scope *, struct aml_value *, 
    struct aml_value *, int);
void aml_xcreatefield(struct aml_value *, int, struct aml_value *, int, int,
    struct aml_value *, int, int);
void aml_xparsefieldlist(struct aml_scope *, int, int,
    struct aml_value *, struct aml_value *, int);
int aml_evalhid(struct aml_node *, struct aml_value *);

#define GAS_PCI_CFG_SPACE_UNEVAL  0xCC

int
aml_evalhid(struct aml_node *node, struct aml_value *val)
{
	if (aml_evalname(dsdt_softc, node, "_HID", 0, NULL, val))
		return (-1);

	/* Integer _HID: convert to EISA ID */
	if (val->type == AML_OBJTYPE_INTEGER) 
		_aml_setvalue(val, AML_OBJTYPE_STRING, -1, aml_eisaid(val->v_integer));
	return (0);
}

int
aml_xgetpci(struct aml_node *node, int64_t *base)
{
	struct aml_node *pci_root;
	struct aml_value hid;
	int64_t v;

	*base = 0;
	dnprintf(10,"RESOLVE PCI: %s\n", aml_nodename(node));
	for (pci_root=node->parent; pci_root; pci_root=pci_root->parent) {
		/* PCI Root object will have _HID value */
		if (aml_evalhid(pci_root, &hid) == 0) {
			aml_freevalue(&hid);
			break;
		}
	}
	if (!aml_evalinteger(NULL, node->parent, "_ADR", 0, NULL, &v))
		*base += (v << 16L);
	if (!aml_evalinteger(NULL, pci_root, "_BBN", 0, NULL, &v))
		*base += (v << 48L);
	return 0;
}

void
aml_xresolve(struct aml_scope *scope, struct aml_value *val)
{
	int64_t base;

	if (val->type != AML_OBJTYPE_OPREGION || val->v_opregion.flag)
		return;
	if (val->v_opregion.iospace != GAS_PCI_CFG_SPACE)
		return;

	/* Evaluate PCI Address */
	aml_xgetpci(val->node, &base);
	val->v_opregion.iobase += base;
	val->v_opregion.flag = 1;
}

/* Perform IO to address space
 *    type = GAS_XXXX
 *    base = base address
 *    rlen = length in bytes to read/write
 *    buf  = buffer
 *    mode = ACPI_IOREAD/ACPI_IOWRITE
 *    sz   = access_size (bits)
 */
void
aml_xgasio(int type, uint64_t base, int rlen, void *buf, int mode, int sz,
    const char *lbl)
{
	sz >>= 3;
	acpi_gasio(dsdt_softc, mode, type, base, sz, rlen, buf);
#ifdef ACPI_DEBUG
	{
		int idx;
		printf("%sio: [%s]  ty:%x bs=%.8llx sz=%.4x rlen=%.4x ",
		    mode == ACPI_IOREAD ? "rd" : "wr", lbl,
		    type, base, sz, rlen);
		for (idx=0; idx<rlen; idx++) {
			printf("%.2x ", ((uint8_t *)buf)[idx]);
		}
	}
	printf("\n");
#endif
}

void *
aml_xgetptr(struct aml_value *tmp, int blen)
{
	if (blen > aml_intlen) {
		_aml_setvalue(tmp, AML_OBJTYPE_BUFFER, aml_bytelen(blen), 0);
		return tmp->v_buffer;
	}
	_aml_setvalue(tmp, AML_OBJTYPE_INTEGER, 0, NULL);
	return &tmp->v_integer;
}

/* Read and Write BufferField and FieldUnit objects */
void
aml_xfldio(struct aml_scope *scope, struct aml_value *fld, 
	   struct aml_value *buf, int mode)
{
	struct aml_value tmp, *data;
	int bpos, blen, preserve=1, mask, aligned, rlen, slen;
	void *sptr, *dptr;
		
 	switch (AML_FIELD_ACCESS(fld->v_field.flags)) {
	case AML_FIELD_WORDACC:
		mask=15;
		break;
	case AML_FIELD_DWORDACC:
		mask=31;
		break;
	case AML_FIELD_QWORDACC:
		mask=63;
		break;
	default:
		mask=7;
		break;
	}
	data = fld->v_field.ref1;
	bpos = fld->v_field.bitpos;
	blen = fld->v_field.bitlen;
	rlen = aml_bytelen((bpos & 7) + blen);
	aligned = !((bpos|blen)&mask);
	preserve = AML_FIELD_UPDATE(fld->v_field.flags);

	dnprintf(30,"\nquick: %s: [%s] %.4x-%.4x msk=%.2x algn=%d prsrv=%d [%s]\n",
	    mode == ACPI_IOREAD ? "read from" : "write to",
	    aml_nodename(fld->node),
	    bpos, blen, mask, aligned, preserve,
	    aml_mnem(fld->v_field.type, 0));

	memset(&tmp, 0, sizeof(tmp));
	if (fld->v_field.ref2 != NULL) {
		/* Write index */
		dnprintf(30,"writing index fldio: %d\n", fld->v_field.ref3);
		_aml_setvalue(&tmp, AML_OBJTYPE_INTEGER, 
		    fld->v_field.ref3, NULL);
		aml_xfldio(scope, fld->v_field.ref2, &tmp, ACPI_IOWRITE);
	}

	/* Get pointer to Data Object */
	switch (data->type) {
	case AML_OBJTYPE_BUFFER:
		dptr = data->v_buffer;
		break;
	case AML_OBJTYPE_STRING:
		dptr = data->v_string;
		break;
	case AML_OBJTYPE_INTEGER:
		dptr = &data->v_integer;
		break;
	case AML_OBJTYPE_OPREGION:
		/* Depending on size, allocate buffer or integer */
		aml_xresolve(scope, data);
		dptr = aml_xgetptr(&tmp, rlen << 3);
		break;
	case AML_OBJTYPE_FIELDUNIT:
	case AML_OBJTYPE_BUFFERFIELD:
		/* Set to integer for now.. */
		_aml_setvalue(&tmp, AML_OBJTYPE_INTEGER, 0x0, NULL);
		dptr = &tmp.v_integer;
		break;
	default:
		aml_die("jk XREAD/WRITE: unknown type: %x\n", data->type);
		break;
	}

	aml_lockfield(scope, fld);
	if (mode == ACPI_IOREAD) {
		sptr = aml_xgetptr(buf, blen);
		switch (data->type) {
		case AML_OBJTYPE_OPREGION:
			/* Do GASIO into temp buffer, bitcopy into result */
			aml_xgasio(data->v_opregion.iospace,
			    data->v_opregion.iobase+(bpos>>3),
			    rlen, dptr, ACPI_IOREAD, mask+1,
			    aml_nodename(fld->node));
			aml_bufcpy(sptr, 0, dptr, bpos & 7, blen);
			break;
		case AML_OBJTYPE_FIELDUNIT:
		case AML_OBJTYPE_BUFFERFIELD:
			/* Do FieldIO into temp buffer, bitcopy into result */
			aml_xfldio(scope, data, &tmp, ACPI_IOREAD);
			aml_bufcpy(sptr, 0, dptr, bpos & 7, blen);
			break;
		default:
			/* bitcopy into result */
			aml_bufcpy(sptr, 0, dptr, bpos, blen);
			break;
		}
	}
	else {
		switch (buf->type) {
		case AML_OBJTYPE_INTEGER:
			slen = aml_intlen;
			break;
		default:
			slen = buf->length<<3;
			break;
		}
		if (slen < blen) {
#ifndef SMALL_KERNEL
			aml_showvalue(fld, 0);
			aml_showvalue(buf, 0);
#endif
			aml_die("BIG SOURCE %d %d %s", buf->length, blen>>3, "");
		}
		if (buf->type != AML_OBJTYPE_INTEGER)
			aml_die("writefield: not integer\n");
		sptr = &buf->v_integer;

		switch (data->type) {
		case AML_OBJTYPE_OPREGION:
			if (!aligned && preserve == AML_FIELD_PRESERVE) {
				/* Preserve contents: read current value */
				aml_xgasio(data->v_opregion.iospace,
				    data->v_opregion.iobase+(bpos>>3),
				    rlen, dptr, ACPI_IOREAD, mask+1,
				    aml_nodename(fld->node));
			}
			/* Bitcopy data into temp buffer, write GAS */
			if (preserve == AML_FIELD_WRITEASONES)
				memset(dptr, 0xFF, tmp.length);
			aml_bufcpy(dptr, bpos & 7, sptr, 0, blen);
			aml_xgasio(data->v_opregion.iospace,
			    data->v_opregion.iobase+(bpos>>3),
			    rlen, dptr, ACPI_IOWRITE, mask+1,
			    aml_nodename(fld->node));
			break;
		case AML_OBJTYPE_FIELDUNIT:
		case AML_OBJTYPE_BUFFERFIELD:
			if (!aligned && preserve == AML_FIELD_PRESERVE) {
				/* Preserve contents: read current value */
				aml_xfldio(scope, data, &tmp, ACPI_IOREAD);
				if (tmp.type != AML_OBJTYPE_INTEGER)
					dptr = tmp.v_buffer;
			}
			else {
				dptr = aml_xgetptr(&tmp, rlen<<3);
			}
			/* Bitcopy data into temp buffer, write field */
			if (preserve == AML_FIELD_WRITEASONES)
				memset(dptr, 0xFF, tmp.length);
			aml_bufcpy(dptr, bpos & 7, sptr, 0, blen);
			aml_xfldio(scope, data, &tmp, ACPI_IOWRITE);
			break;
		default:
			if (blen > aml_intlen) {
				aml_die("jk Big Buffer other!\n");
			}
			aml_bufcpy(dptr, bpos, sptr, 0, blen);
			break;
		}
	}
	aml_freevalue(&tmp);
	aml_unlockfield(scope, fld);
}

/* Create Field Object          data		index
 *   AMLOP_FIELD		n:OpRegion	NULL
 *   AMLOP_INDEXFIELD		n:Field		n:Field
 *   AMLOP_BANKFIELD		n:OpRegion	n:Field
 *   AMLOP_CREATEFIELD		t:Buffer	NULL
 *   AMLOP_CREATEBITFIELD	t:Buffer	NULL
 *   AMLOP_CREATEBYTEFIELD	t:Buffer	NULL
 *   AMLOP_CREATEWORDFIELD	t:Buffer	NULL
 *   AMLOP_CREATEDWORDFIELD	t:Buffer	NULL
 *   AMLOP_CREATEQWORDFIELD	t:Buffer	NULL
 *   AMLOP_INDEX		t:Buffer	NULL
 */
void
aml_xcreatefield(struct aml_value *field, int opcode,
		struct aml_value *data, int bpos, int blen,
		struct aml_value *index, int indexval, int flags)
{
	dnprintf(10, "## %s(%s): %s %.4x-%.4x\n", 
	    aml_mnem(opcode, 0),
	    blen > aml_intlen ? "BUF" : "INT",
	    aml_nodename(field->node), bpos, blen);
	if (index) {
		dnprintf(10, "  index:%s:%.2x\n", aml_nodename(index->node), 
		    indexval);
	}
	dnprintf(10, "  data:%s\n", aml_nodename(data->node));
	field->type = (opcode == AMLOP_FIELD || 
	    opcode == AMLOP_INDEXFIELD || 
	    opcode == AMLOP_BANKFIELD) ?
	    AML_OBJTYPE_FIELDUNIT : 
	    AML_OBJTYPE_BUFFERFIELD;

	if (field->type == AML_OBJTYPE_BUFFERFIELD && 
	    data->type != AML_OBJTYPE_BUFFER) 
	{
		printf("WARN: %s not buffer\n",
		    aml_nodename(data->node));
		aml_xconvert(data, &data, AML_OBJTYPE_BUFFER, 0);
	}
	field->v_field.type = opcode;
	field->v_field.bitpos = bpos;
	field->v_field.bitlen = blen;
	field->v_field.ref3 = indexval;
	field->v_field.ref2 = index;
	field->v_field.ref1 = data;
	field->v_field.flags = flags;

	/* Increase reference count */
	aml_xaddref(data, "Field.Data");
	aml_xaddref(index, "Field.Index");
}

/* Parse Field/IndexField/BankField scope */
void
aml_xparsefieldlist(struct aml_scope *mscope, int opcode, int flags, 
    struct aml_value *data, struct aml_value *index, int indexval)
{
	struct aml_value *rv;
	int bpos, blen;

	if (mscope == NULL)
		return;
	bpos = 0;
	while (mscope->pos < mscope->end) {
		switch (*mscope->pos) {
		case 0x00: // reserved, length
			mscope->pos++;
			blen = aml_parselength(mscope);
			break;
		case 0x01: // flags
			mscope->pos += 3;
			blen = 0;
			break;
		default: // 4-byte name, length
			mscope->pos = aml_xparsename(mscope->pos, mscope->node,
			    ns_xcreate, &rv);
			blen = aml_parselength(mscope);
			switch (opcode) {
			case AMLOP_FIELD:
				/* nbF */
				aml_xcreatefield(rv, opcode, data, bpos, 
				    blen, NULL, 0, flags);
				break;
			case AMLOP_INDEXFIELD:
				/* nnbF */
				aml_xcreatefield(rv, opcode, data, bpos & 7, 
				    blen, index, bpos>>3, flags);
				break;
			case AMLOP_BANKFIELD:
				/* nnibF */
				aml_xcreatefield(rv, opcode, data, bpos, 
				    blen, index, indexval, flags);
				break;
			}
			break;
		}
		bpos += blen;
	}
}

/*
 * Mutex/Event utility functions 
 */
int	acpi_xmutex_acquire(struct aml_scope *, struct aml_value *, int);
void	acpi_xmutex_release(struct aml_scope *, struct aml_value *);
int	acpi_xevent_wait(struct aml_scope *, struct aml_value *, int);
void	acpi_xevent_signal(struct aml_scope *, struct aml_value *);
void	acpi_xevent_reset(struct aml_scope *, struct aml_value *);

int
acpi_xmutex_acquire(struct aml_scope *scope, struct aml_value *mtx, 
    int timeout)
{
	int err;

	if (mtx->v_mtx.owner == NULL || scope == mtx->v_mtx.owner) {
		/* We are now the owner */
		mtx->v_mtx.owner = scope;
		if (mtx == aml_global_lock) {
			dnprintf(10,"LOCKING GLOBAL\n");
			err = acpi_acquire_global_lock(&dsdt_softc->sc_facs->global_lock);
		}
		dnprintf(5,"%s acquires mutex %s\n", scope->node->name,
		    mtx->node->name);
		return 0;
	}
	else if (timeout == 0) {
		return 1;
	}
	/* Wait for mutex */
	return 0;
}

void
acpi_xmutex_release(struct aml_scope *scope, struct aml_value *mtx)
{
	int err;

	if (mtx == aml_global_lock) {
	  	dnprintf(10,"UNLOCKING GLOBAL\n");
		err=acpi_release_global_lock(&dsdt_softc->sc_facs->global_lock);
	}
	dnprintf(5, "%s releases mutex %s\n", scope->node->name,
	    mtx->node->name);
	mtx->v_mtx.owner = NULL;
	/* Wakeup waiters */
}

int
acpi_xevent_wait(struct aml_scope *scope, struct aml_value *evt, int timeout)
{
	if (evt->v_evt.state == 1) {
		/* Object is signaled */
		return 0;
	}
	else if (timeout == 0) {
		/* Zero timeout */
		return 1;
	}
	/* Wait for timeout or signal */
	return 0;
}

void
acpi_xevent_signal(struct aml_scope *scope, struct aml_value *evt)
{
	evt->v_evt.state = 1;
	/* Wakeup waiters */
}

void
acpi_xevent_reset(struct aml_scope *scope, struct aml_value *evt)
{
	evt->v_evt.state = 0;
}

/* Store result value into an object */
void
aml_xstore(struct aml_scope *scope, struct aml_value *lhs , int64_t ival, 
    struct aml_value *rhs)
{
	struct aml_value tmp;
	int mlen;

	/* Already set */
	if (lhs == rhs || lhs == NULL || lhs->type == AML_OBJTYPE_NOTARGET) {
		return;
	}
	memset(&tmp, 0, sizeof(tmp));
	tmp.refcnt=99;
	if (rhs == NULL) {
		rhs = _aml_setvalue(&tmp, AML_OBJTYPE_INTEGER, ival, NULL);
	}
	if (rhs->type == AML_OBJTYPE_BUFFERFIELD || 
	    rhs->type == AML_OBJTYPE_FIELDUNIT) {
		aml_xfldio(scope, rhs, &tmp, ACPI_IOREAD);
		rhs = &tmp;
	}
	/* Store to LocalX: free value */
	if (lhs->stack >= AMLOP_LOCAL0 && lhs->stack <= AMLOP_LOCAL7)
		aml_freevalue(lhs);

	while (lhs->type == AML_OBJTYPE_OBJREF) {
		lhs = lhs->v_objref.ref;
	}
	switch (lhs->type) {
	case AML_OBJTYPE_UNINITIALIZED:
		aml_copyvalue(lhs, rhs);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		aml_xfldio(scope, lhs, rhs, ACPI_IOWRITE);
		break;
	case AML_OBJTYPE_DEBUGOBJ:
		break;
	case AML_OBJTYPE_INTEGER:
		aml_xconvert(rhs, &rhs, lhs->type, 0);
		lhs->v_integer = rhs->v_integer;
		aml_xdelref(&rhs, "store.int");
		break;
	case AML_OBJTYPE_BUFFER:
	case AML_OBJTYPE_STRING:
		aml_xconvert(rhs, &rhs, lhs->type, 0);
		if (lhs->length < rhs->length) {
			dnprintf(10,"Overrun! %d,%d\n", lhs->length, rhs->length);
			aml_freevalue(lhs);
			_aml_setvalue(lhs, rhs->type, rhs->length, NULL);
		}
		mlen = min(lhs->length, rhs->length);
		memset(lhs->v_buffer, 0x00, lhs->length);
		memcpy(lhs->v_buffer, rhs->v_buffer, mlen);
		aml_xdelref(&rhs, "store.bufstr");
		break;
	case AML_OBJTYPE_PACKAGE:
		/* Convert to LHS type, copy into LHS */
		if (rhs->type != AML_OBJTYPE_PACKAGE) {
			aml_die("Copy non-package into package?");
		}
		aml_freevalue(lhs);
		aml_copyvalue(lhs, rhs);
		break;
	default:
		aml_die("Store to default type!	 %x\n", lhs->type);
		break;
	}
	aml_freevalue(&tmp);
}

#ifndef SMALL_KERNEL
/* Disassembler routines */
void aml_disprintf(void *arg, const char *fmt, ...);

void
aml_disprintf(void *arg, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void
aml_disasm(struct aml_scope *scope, int lvl, 
    void (*dbprintf)(void *, const char *, ...), 
    void *arg)
{
	int pc, opcode;
	struct aml_opcode *htab;
	uint64_t ival;
	struct aml_value *rv, tmp;
	uint8_t *end;
	struct aml_scope *ms;
	char *ch;
	char  mch[64];

	if (dbprintf == NULL)
		dbprintf = aml_disprintf;

	pc = aml_pc(scope->pos);
	opcode = aml_parseopcode(scope);
	htab = aml_findopcode(opcode);

	/* Display address + indent */
	if (lvl <= 0x7FFF) {
		dbprintf(arg, "%.4x ", pc);
		for (pc=0; pc<lvl; pc++) {
			dbprintf(arg, "	 ");
		}
	}
	ch = NULL;
	switch (opcode) {
	case AMLOP_NAMECHAR:
		scope->pos = aml_xparsename(scope->pos, scope->node, 
		    ns_xsearch, &rv);
		if (rv->type == AML_OBJTYPE_NAMEREF) {
			ch = "@@@";
			aml_xdelref(&rv, "disasm");
			break;
		}
		/* if this is a method, get arguments */
		strlcpy(mch, aml_nodename(rv->node), sizeof(mch));
		if (rv->type == AML_OBJTYPE_METHOD) {
			strlcat(mch, "(", sizeof(mch));
			for (ival=0; ival<AML_METHOD_ARGCOUNT(rv->v_method.flags); ival++) {
				strlcat(mch, ival ? ", %z" : "%z", 
				    sizeof(mch));
			}
			strlcat(mch, ")", sizeof(mch));
		}
		ch = mch;
		break;

	case AMLOP_ZERO:
	case AMLOP_ONE:
	case AMLOP_ONES:
	case AMLOP_LOCAL0:
	case AMLOP_LOCAL1:
	case AMLOP_LOCAL2:
	case AMLOP_LOCAL3:
	case AMLOP_LOCAL4:
	case AMLOP_LOCAL5:
	case AMLOP_LOCAL6:
	case AMLOP_LOCAL7:
	case AMLOP_ARG0:
	case AMLOP_ARG1:
	case AMLOP_ARG2:
	case AMLOP_ARG3:
	case AMLOP_ARG4:
	case AMLOP_ARG5:
	case AMLOP_ARG6:
	case AMLOP_NOP:
	case AMLOP_REVISION:
	case AMLOP_DEBUG:
	case AMLOP_CONTINUE:
	case AMLOP_BREAKPOINT:
	case AMLOP_BREAK:
		ch="%m";
		break;
	case AMLOP_BYTEPREFIX:
		ch="%b";
		break;
	case AMLOP_WORDPREFIX:
		ch="%w";
		break;
	case AMLOP_DWORDPREFIX:
		ch="%d";
		break;
	case AMLOP_QWORDPREFIX:
		ch="%q";
		break;
	case AMLOP_STRINGPREFIX:
	  	ch="%a";
		break;

	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
	case AMLOP_LNOT:
	case AMLOP_SIZEOF:
	case AMLOP_DEREFOF:
	case AMLOP_REFOF:
	case AMLOP_OBJECTTYPE:
	case AMLOP_UNLOAD:
	case AMLOP_RELEASE:
	case AMLOP_SIGNAL:
	case AMLOP_RESET:
	case AMLOP_STALL:
	case AMLOP_SLEEP:
	case AMLOP_RETURN:
		ch="%m(%n)";
		break;
	case AMLOP_OR:
	case AMLOP_ADD:
	case AMLOP_AND:
	case AMLOP_NAND:
	case AMLOP_XOR:
	case AMLOP_SHL:
	case AMLOP_SHR:
	case AMLOP_NOR:
	case AMLOP_MOD:
	case AMLOP_SUBTRACT:
	case AMLOP_MULTIPLY:
	case AMLOP_INDEX:
	case AMLOP_CONCAT:
	case AMLOP_CONCATRES:
	case AMLOP_TOSTRING:
		ch="%m(%n, %n, %n)";
		break;
	case AMLOP_CREATEBYTEFIELD:
	case AMLOP_CREATEWORDFIELD:
	case AMLOP_CREATEDWORDFIELD:
	case AMLOP_CREATEQWORDFIELD:
	case AMLOP_CREATEBITFIELD:
		ch="%m(%n, %n, %N)";
		break;
	case AMLOP_CREATEFIELD:
		ch="%m(%n, %n, %n, %N)";
		break;
	case AMLOP_DIVIDE:
	case AMLOP_MID:
		ch="%m(%n, %n, %n, %n)";
		break;
	case AMLOP_LAND:
	case AMLOP_LOR:
	case AMLOP_LNOTEQUAL:
	case AMLOP_LLESSEQUAL:
	case AMLOP_LLESS:
	case AMLOP_LEQUAL:
	case AMLOP_LGREATEREQUAL:
	case AMLOP_LGREATER:
	case AMLOP_NOT:
	case AMLOP_FINDSETLEFTBIT:
	case AMLOP_FINDSETRIGHTBIT:
	case AMLOP_TOINTEGER:
	case AMLOP_TOBUFFER:
	case AMLOP_TOHEXSTRING:
	case AMLOP_TODECSTRING:
	case AMLOP_FROMBCD:
	case AMLOP_TOBCD:
	case AMLOP_WAIT:
	case AMLOP_LOAD:
	case AMLOP_STORE:
	case AMLOP_NOTIFY:
	case AMLOP_COPYOBJECT:
		ch="%m(%n, %n)";
		break;
	case AMLOP_ACQUIRE:
		ch = "%m(%n, %w)";
		break;
	case AMLOP_CONDREFOF:
		ch="%m(%R, %n)";
		break;
	case AMLOP_ALIAS:
		ch="%m(%n, %N)";
		break;
	case AMLOP_NAME:
		ch="%m(%N, %n)";
		break;
	case AMLOP_EVENT:
		ch="%m(%N)";
		break;
	case AMLOP_MUTEX:
		ch = "%m(%N, %b)";
		break;
	case AMLOP_OPREGION:
		ch = "%m(%N, %b, %n, %n)";
		break;
	case AMLOP_DATAREGION:
		ch="%m(%N, %n, %n, %n)";
		break;
	case AMLOP_FATAL:
		ch = "%m(%b, %d, %n)";
		break;
	case AMLOP_IF:
	case AMLOP_WHILE:
	case AMLOP_SCOPE:
	case AMLOP_THERMALZONE:
	case AMLOP_VARPACKAGE:
		end = aml_parseend(scope);
		ch = "%m(%n) {\n%T}";
		break;
	case AMLOP_DEVICE:
		end = aml_parseend(scope);
		ch = "%m(%N) {\n%T}";
		break;
	case AMLOP_POWERRSRC:
		end = aml_parseend(scope);
		ch = "%m(%N, %b, %w) {\n%T}";
		break;
	case AMLOP_PROCESSOR:
		end = aml_parseend(scope);
		ch = "%m(%N, %b, %d, %b) {\n%T}";
		break;
	case AMLOP_METHOD:
		end = aml_parseend(scope);
		ch = "%m(%N, %b) {\n%T}";
		break;
	case AMLOP_PACKAGE:
		end = aml_parseend(scope);
		ch = "%m(%b) {\n%T}";
		break;
	case AMLOP_ELSE:
		end = aml_parseend(scope);
		ch = "%m {\n%T}";
		break;
	case AMLOP_BUFFER:
		end = aml_parseend(scope);
		ch = "%m(%n) { %B }";
		break;
	case AMLOP_INDEXFIELD:
		end = aml_parseend(scope);
		ch = "%m(%n, %n, %b) {\n%F}";
		break;
	case AMLOP_BANKFIELD:
		end = aml_parseend(scope);
		ch = "%m(%n, %n, %n, %b) {\n%F}";
		break;
	case AMLOP_FIELD:
		end = aml_parseend(scope);
		ch = "%m(%n, %b) {\n%F}";
		break;
	case AMLOP_MATCH:
		ch = "%m(%n, %b, %n, %b, %n, %n)";
		break;
	case AMLOP_LOADTABLE:
		ch = "%m(%n, %n, %n, %n, %n, %n)";
		break;
	default:
		aml_die("opcode = %x\n", opcode);
		break;
	}

	/* Parse printable buffer args */
	while (ch && *ch) {
		char c;

		if (*ch != '%') {
			dbprintf(arg,"%c", *(ch++));
			continue;
		}
		c = *(++ch);
		switch (c) {
		case 'b':
		case 'w':
		case 'd':
		case 'q':
			/* Parse simple object: don't allocate */
			aml_xparsesimple(scope, c, &tmp);
			dbprintf(arg,"0x%llx", tmp.v_integer);
			break;
		case 'a':
			dbprintf(arg, "\'%s\'", scope->pos);
			scope->pos += strlen(scope->pos)+1;
			break;			
		case 'N':
			/* Create Name */
			rv = aml_xparsesimple(scope, c, NULL);
			dbprintf(arg,aml_nodename(rv->node));
			break;
		case 'm':
			/* display mnemonic */
			dbprintf(arg,htab->mnem);
			break;
		case 'R':
			/* Search name */
			scope->pos = aml_xparsename(scope->pos, scope->node, 
			    ns_xdis, &rv);
			break;
		case 'z':
		case 'n':
			/* generic arg: recurse */
			aml_disasm(scope, lvl | 0x8000, dbprintf, arg);
			break;
		case 'B':
			/* Buffer */
			scope->pos = end;
			break;
		case 'F':
			/* Field List */
			tmp.v_buffer = scope->pos;
			tmp.length   = end - scope->pos;

			ms = aml_xpushscope(scope, &tmp, scope->node, 0);
			while (ms && ms->pos < ms->end) {
				if (*ms->pos == 0x00) {
					ms->pos++;
					aml_parselength(ms);
				}
				else if (*ms->pos == 0x01) {
					ms->pos+=3;
				}
				else {
					ms->pos = aml_xparsename(ms->pos, 
					    ms->node, ns_xcreate, &rv);
					aml_parselength(ms);
					dbprintf(arg,"	%s\n", 
					    aml_nodename(rv->node));
				}
			}
			aml_xpopscope(ms);

			/* Display address and closing bracket */
			dbprintf(arg,"%.4x ", aml_pc(scope->pos));
			for (pc=0; pc<(lvl & 0x7FFF); pc++) {
				dbprintf(arg,"	");
			}
			scope->pos = end;
			break;
		case 'T':
			/* Scope: Termlist */
			tmp.v_buffer = scope->pos;
			tmp.length   = end - scope->pos;

			ms = aml_xpushscope(scope, &tmp, scope->node, 0);
			while (ms && ms->pos < ms->end) {
				aml_disasm(ms, (lvl + 1) & 0x7FFF, 
				    dbprintf, arg);
			}
			aml_xpopscope(ms);

			/* Display address and closing bracket */
			dbprintf(arg,"%.4x ", aml_pc(scope->pos));
			for (pc=0; pc<(lvl & 0x7FFF); pc++) {
				dbprintf(arg,"	");
			}
			scope->pos = end;
			break;
		}
		ch++;
	}
	if (lvl <= 0x7FFF) {
		dbprintf(arg,"\n");
	}
}
#endif /* SMALL_KERNEL */

int aml_busy;

/* Evaluate method or buffervalue objects */
struct aml_value *
aml_xeval(struct aml_scope *scope, struct aml_value *my_ret, int ret_type,
    int argc, struct aml_value *argv)
{
	struct aml_value *tmp = my_ret;
	struct aml_scope *ms;
	int idx;

	switch (tmp->type) {
	case AML_OBJTYPE_METHOD:
		dnprintf(10,"\n--== Eval Method [%s, %d args] to %c ==--\n", 
		    aml_nodename(tmp->node), 
		    AML_METHOD_ARGCOUNT(tmp->v_method.flags),
		    ret_type);
		ms = aml_xpushscope(scope, tmp, tmp->node, AMLOP_METHOD);
		
		/* Parse method arguments */
		for (idx=0; idx<AML_METHOD_ARGCOUNT(tmp->v_method.flags); idx++) {
			struct aml_value *sp;

			sp = aml_getstack(ms, AMLOP_ARG0+idx);
			if (argv) {
				aml_copyvalue(sp, &argv[idx]);
			}
			else {
				sp->type = AML_OBJTYPE_OBJREF;
				sp->v_objref.type = AMLOP_ARG0 + idx;
				sp->v_objref.ref = aml_xparse(scope, 't', "ARGX");
			}
		}
#ifdef ACPI_DEBUG
		aml_showstack(ms);
#endif
		
		/* Evaluate method scope */
		if (tmp->v_method.fneval != NULL) {
			my_ret = tmp->v_method.fneval(ms, NULL);
		}
		else {
			aml_xparse(ms, 'T', "METHEVAL");
			my_ret = ms->retv;
		}
		dnprintf(10,"\n--==Finished evaluating method: %s %c\n", 
		    aml_nodename(tmp->node), ret_type);
#ifdef ACPI_DEBUG
		aml_showvalue(my_ret, 0);
		aml_showstack(ms);
#endif
		aml_xpopscope(ms);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		my_ret = aml_allocvalue(0,0,NULL);
		dnprintf(20,"quick: Convert Bufferfield to %c 0x%x\n", 
		    ret_type, my_ret);
		aml_xfldio(scope, tmp, my_ret, ACPI_IOREAD);
		break;
	}
	if (ret_type == 'i' && my_ret && my_ret->type != AML_OBJTYPE_INTEGER) {
#ifndef SMALL_KERNEL
		aml_showvalue(my_ret, 8-100);
#endif
		aml_die("Not Integer");
	}
	return my_ret;
}

/*
 * The following opcodes produce return values
 *   TOSTRING	-> Str
 *   TOHEXSTR	-> Str
 *   TODECSTR	-> Str
 *   STRINGPFX	-> Str
 *   BUFFER	-> Buf
 *   CONCATRES	-> Buf
 *   TOBUFFER	-> Buf
 *   MID	-> Buf|Str
 *   CONCAT	-> Buf|Str
 *   PACKAGE	-> Pkg
 *   VARPACKAGE -> Pkg
 *   LOCALx	-> Obj
 *   ARGx	-> Obj
 *   NAMECHAR	-> Obj
 *   REFOF	-> ObjRef
 *   INDEX	-> ObjRef
 *   DEREFOF	-> DataRefObj
 *   COPYOBJECT -> DataRefObj
 *   STORE	-> DataRefObj

 *   ZERO	-> Int
 *   ONE	-> Int
 *   ONES	-> Int
 *   REVISION	-> Int
 *   B/W/D/Q	-> Int
 *   OR		-> Int
 *   AND	-> Int
 *   ADD	-> Int
 *   NAND	-> Int
 *   XOR	-> Int
 *   SHL	-> Int
 *   SHR	-> Int
 *   NOR	-> Int
 *   MOD	-> Int
 *   SUBTRACT	-> Int
 *   MULTIPLY	-> Int
 *   DIVIDE	-> Int
 *   NOT	-> Int
 *   TOBCD	-> Int
 *   FROMBCD	-> Int
 *   FSLEFTBIT	-> Int
 *   FSRIGHTBIT -> Int
 *   INCREMENT	-> Int
 *   DECREMENT	-> Int
 *   TOINTEGER	-> Int
 *   MATCH	-> Int
 *   SIZEOF	-> Int
 *   OBJECTTYPE -> Int
 *   TIMER	-> Int

 *   CONDREFOF	-> Bool
 *   ACQUIRE	-> Bool
 *   WAIT	-> Bool
 *   LNOT	-> Bool
 *   LAND	-> Bool
 *   LOR	-> Bool
 *   LLESS	-> Bool
 *   LEQUAL	-> Bool
 *   LGREATER	-> Bool
 *   LNOTEQUAL	-> Bool
 *   LLESSEQUAL -> Bool
 *   LGREATEREQ -> Bool

 *   LOADTABLE	-> DDB
 *   DEBUG	-> Debug

 *   The following opcodes do not generate a return value:
 *   NOP
 *   BREAKPOINT
 *   RELEASE
 *   RESET
 *   SIGNAL
 *   NAME
 *   ALIAS
 *   OPREGION
 *   DATAREGION
 *   EVENT
 *   MUTEX
 *   SCOPE
 *   DEVICE
 *   THERMALZONE
 *   POWERRSRC
 *   PROCESSOR
 *   METHOD
 *   CREATEFIELD
 *   CREATEBITFIELD
 *   CREATEBYTEFIELD
 *   CREATEWORDFIELD
 *   CREATEDWORDFIELD
 *   CREATEQWORDFIELD
 *   FIELD
 *   INDEXFIELD
 *   BANKFIELD
 *   STALL
 *   SLEEP
 *   NOTIFY
 *   FATAL
 *   LOAD
 *   UNLOAD
 *   IF
 *   ELSE
 *   WHILE
 *   BREAK
 *   CONTINUE
 */

/* Parse a simple object from AML Bytestream */
struct aml_value *
aml_xparsesimple(struct aml_scope *scope, char ch, struct aml_value *rv)
{
	if (ch == AML_ARG_CREATENAME) {
		scope->pos = aml_xparsename(scope->pos, scope->node, 
		    ns_xcreate, &rv);
		return rv;
	}
	else if (ch == AML_ARG_SEARCHNAME) {
		scope->pos = aml_xparsename(scope->pos, scope->node, 
		    ns_xsearch, &rv);
		return rv;
	}
	if (rv == NULL)
		rv = aml_allocvalue(0,0,NULL);
	switch (ch) {
	case AML_ARG_REVISION:
		_aml_setvalue(rv, AML_OBJTYPE_INTEGER, AML_REVISION, NULL);
		break;
	case AML_ARG_DEBUG:
		_aml_setvalue(rv, AML_OBJTYPE_DEBUGOBJ, 0, NULL);
		break;
	case AML_ARG_BYTE:
		_aml_setvalue(rv, AML_OBJTYPE_INTEGER, 
		    aml_get8(scope->pos), NULL);
		scope->pos += 1;
		break;
	case AML_ARG_WORD:
		_aml_setvalue(rv, AML_OBJTYPE_INTEGER, 
		    aml_get16(scope->pos), NULL);
		scope->pos += 2;
		break;
	case AML_ARG_DWORD:
		_aml_setvalue(rv, AML_OBJTYPE_INTEGER, 
		    aml_get32(scope->pos), NULL);
		scope->pos += 4;
		break;
	case AML_ARG_QWORD:
		_aml_setvalue(rv, AML_OBJTYPE_INTEGER, 
		    aml_get64(scope->pos), NULL);
		scope->pos += 8;
		break;
	case AML_ARG_STRING:
		_aml_setvalue(rv, AML_OBJTYPE_STRING, -1, scope->pos);
		scope->pos += rv->length+1;
		break;
	}
	return rv;
}

/*
 * Main Opcode Parser/Evaluator 
 *
 * ret_type is expected type for return value
 *   'o' = Data Object (Int/Str/Buf/Pkg/Name)
 *   'i' = Integer
 *   't' = TermArg     (Int/Str/Buf/Pkg)
 *   'r' = Target      (NamedObj/Local/Arg/Null)
 *   'S' = SuperName   (NamedObj/Local/Arg)
 *   'T' = TermList
 */
#define aml_debugger(x)

int maxdp;

struct aml_value *
aml_xparse(struct aml_scope *scope, int ret_type, const char *stype)
{
	int    opcode, idx, pc, optype[8];
	struct aml_opcode *htab;
	struct aml_value *opargs[8], *my_ret, *tmp, *cname;
	struct aml_scope *mscope, *iscope;
	const char *ch;
	int64_t ival;

	my_ret = NULL;
	if (scope == NULL || scope->pos >= scope->end) {
		return NULL;
	}
	if (odp++ > 125)
		panic("depth");
	if (odp > maxdp) {
		maxdp = odp;
		dnprintf(10, "max depth: %d\n", maxdp);
	}
	iscope = scope;
 start:
	/* --== Stage 0: Get Opcode ==-- */
	pc = aml_pc(scope->pos);
	aml_debugger(scope);

	opcode = aml_parseopcode(scope);
	htab = aml_findopcode(opcode);
	if (htab == NULL) {
		/* No opcode handler */
		aml_die("Unknown opcode: %.4x @ %.4x", opcode, pc);
	}
	dnprintf(18,"%.4x %s\n", pc, aml_mnem(opcode, scope->pos));

	/* --== Stage 1: Process opcode arguments ==-- */
	cname = NULL;
	memset(opargs, 0, sizeof(opargs));
	memset(optype, 0, sizeof(optype));
	idx = 0;
	for (ch = htab->args; *ch; ch++) {
		struct aml_value *rv;
		uint8_t *end;

		rv = NULL;
		switch (*ch) {
		case AML_ARG_OBJLEN:
			end = aml_parseend(scope);
			break;
		case AML_ARG_IFELSE: 
                        /* Special Case: IF-ELSE:piTbpT or IF:piT */
			ch = (*end == AMLOP_ELSE && end < scope->end) ? 
			    "-TbpT" : "-T";
			break;

			/* Complex arguments */
		case 's':
		case 'S': 
		case AML_ARG_TARGET: 
		case AML_ARG_TERMOBJ:
		case AML_ARG_INTEGER:
			if (*ch == 'r' && *scope->pos == AMLOP_ZERO) {
				/* Special case: NULL Target */
				rv = aml_allocvalue(AML_OBJTYPE_NOTARGET, 0, NULL);
				scope->pos++;
			}
			else {
				rv = aml_xparse(scope, *ch, htab->mnem);
				if (rv == NULL)
					aml_die("NULL RESULT");
			}
			break;

			/* Simple arguments */
		case AML_ARG_WHILE:
		case AML_ARG_BUFFER:
		case AML_ARG_METHOD:
		case AML_ARG_FIELDLIST:
		case AML_ARG_TERMOBJLIST:
			rv = aml_allocvalue(AML_OBJTYPE_SCOPE, 0, NULL);
			rv->v_buffer = scope->pos;
			rv->length = end - scope->pos;
			scope->pos = end;
			break;
		case AML_ARG_CONST:
			rv = aml_allocvalue(AML_OBJTYPE_INTEGER, 
			    (char)opcode, NULL);
			break;
		case AML_ARG_CREATENAME:
			rv = aml_xparsesimple(scope, *ch, NULL);
			cname = rv;
			if (cname->type != 0 && opcode != AMLOP_SCOPE)
				dnprintf(10, "%s value already exists %s\n",
				    aml_nodename(cname->node),
				    htab->mnem);
			break;
		case AML_ARG_SEARCHNAME:
			rv = aml_xparsesimple(scope, *ch, NULL);
			if (rv->type != AML_OBJTYPE_NAMEREF)
				aml_xaddref(rv, "Search Name");
			break;
		case AML_ARG_BYTE:
		case AML_ARG_WORD:
		case AML_ARG_DWORD:
		case AML_ARG_QWORD:
		case AML_ARG_DEBUG:
		case AML_ARG_STRING:
		case AML_ARG_REVISION:
			rv = aml_xparsesimple(scope, *ch, NULL);
			break;
		case AML_ARG_STKLOCAL:
		case AML_ARG_STKARG:
			rv = aml_getstack(scope, opcode);
			break;
		default:
			aml_die("Unknown arg type: %c\n", *ch);
			break;
		}
		if (rv != NULL) {
			optype[idx] = *ch;
			opargs[idx++] = rv;
		}
	}

	/* --== Stage 2: Process opcode ==-- */
	ival = 0;
	my_ret = NULL;
	mscope = NULL;
	switch (opcode) {
	case AMLOP_NOP:
	case AMLOP_BREAKPOINT:
		break;
	case AMLOP_LOCAL0:
	case AMLOP_LOCAL1:
	case AMLOP_LOCAL2:
	case AMLOP_LOCAL3:
	case AMLOP_LOCAL4:
	case AMLOP_LOCAL5:
	case AMLOP_LOCAL6:
	case AMLOP_LOCAL7:
	case AMLOP_ARG0:
	case AMLOP_ARG1:
	case AMLOP_ARG2:
	case AMLOP_ARG3:
	case AMLOP_ARG4:
	case AMLOP_ARG5:
	case AMLOP_ARG6:
		my_ret = opargs[0];
		aml_xaddref(my_ret, htab->mnem);
		break;
	case AMLOP_NAMECHAR:
		/* opargs[0] = named object (node != NULL), or nameref */
		my_ret = opargs[0];
		if (my_ret->type == AML_OBJTYPE_OBJREF) {
			my_ret = my_ret->v_objref.ref;
			aml_xaddref(my_ret, "de-alias");
		}
		if (ret_type == 'i' || ret_type == 't' || ret_type == 'T') {
			/* Return TermArg or Integer: Evaluate object */
			my_ret = aml_xeval(scope, my_ret, ret_type, 0, NULL);
		}
		else if (my_ret->type == AML_OBJTYPE_METHOD) {
			/* This should only happen with CondRef */
			dnprintf(12,"non-termarg method : %s\n", stype);
			aml_xaddref(my_ret, "zoom");
		}
		break;

	case AMLOP_ZERO:
	case AMLOP_ONE:
	case AMLOP_ONES:
	case AMLOP_DEBUG:
	case AMLOP_REVISION:
	case AMLOP_BYTEPREFIX:
	case AMLOP_WORDPREFIX:
	case AMLOP_DWORDPREFIX:
	case AMLOP_QWORDPREFIX:
	case AMLOP_STRINGPREFIX:
		my_ret = opargs[0];
		break;

	case AMLOP_BUFFER:
		/* Buffer: iB => Buffer */
		my_ret = aml_allocvalue(AML_OBJTYPE_BUFFER, 
		    opargs[0]->v_integer, NULL);
		memcpy(my_ret->v_buffer, opargs[1]->v_buffer, 
		    opargs[1]->length);
		break;
	case AMLOP_PACKAGE:
	case AMLOP_VARPACKAGE:
		/* Package/VarPackage: bT/iT => Package */
		my_ret = aml_allocvalue(AML_OBJTYPE_PACKAGE, 
		    opargs[0]->v_integer, 0);
		mscope = aml_xpushscope(scope, opargs[1], scope->node, 
		    AMLOP_PACKAGE);

		for (idx=0; idx<my_ret->length; idx++) {
			const char *nn;

			tmp = aml_xparse(mscope, 'o', "Package");
			if (tmp == NULL) {
				continue;
			}
			nn = NULL;
			if (tmp->node)
				/* Object is a named node: store as string */
				nn = aml_nodename(tmp->node);
			else if (tmp->type == AML_OBJTYPE_NAMEREF)
				/* Object is nameref: store as string */
				nn = aml_getname(tmp->v_nameref);
			if (nn != NULL) {
				aml_xdelref(&tmp, "pkg.node");
				tmp = aml_allocvalue(AML_OBJTYPE_STRING, 
				    -1, nn);
			}
			/* Package value already allocated; delete it
			 * and replace with pointer to return value */
			aml_xdelref(&my_ret->v_package[idx], "pkg/init");
			my_ret->v_package[idx] = tmp;
		}
		aml_xpopscope(mscope);
		mscope = NULL;
		break;

		/* Math/Logical operations */
	case AMLOP_OR:
	case AMLOP_ADD:
	case AMLOP_AND:
	case AMLOP_NAND:
	case AMLOP_XOR:
	case AMLOP_SHL:
	case AMLOP_SHR:
	case AMLOP_NOR:
	case AMLOP_MOD:
	case AMLOP_SUBTRACT:
	case AMLOP_MULTIPLY:
		/* XXX: iir => I */
		ival = aml_evalexpr(opargs[0]->v_integer, 
		    opargs[1]->v_integer, opcode);
		aml_xstore(scope, opargs[2], ival, NULL);
		break;
	case AMLOP_DIVIDE:
		/* Divide: iirr => I */
		ival = aml_evalexpr(opargs[0]->v_integer, 
		    opargs[1]->v_integer, AMLOP_MOD);
		aml_xstore(scope, opargs[2], ival, NULL);

		ival = aml_evalexpr(opargs[0]->v_integer, 
		    opargs[1]->v_integer, AMLOP_DIVIDE);
		aml_xstore(scope, opargs[3], ival, NULL);
		break;
	case AMLOP_NOT:
	case AMLOP_TOBCD:
	case AMLOP_FROMBCD:
	case AMLOP_FINDSETLEFTBIT:
	case AMLOP_FINDSETRIGHTBIT:
		/* XXX: ir => I */
		ival = aml_evalexpr(opargs[0]->v_integer, 0, opcode);
		aml_xstore(scope, opargs[1], ival, NULL);
		break;
	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
		/* Inc/Dec: S => I */
		my_ret = aml_xeval(scope, opargs[0], AML_ARG_INTEGER, 0, NULL);
		ival = aml_evalexpr(my_ret->v_integer, 1, opcode);
		aml_xstore(scope, opargs[0], ival, NULL);
		break;
	case AMLOP_LNOT:
		/* LNot: i => Bool */
		ival = aml_evalexpr(opargs[0]->v_integer, 0, opcode);
		break;
	case AMLOP_LOR:
	case AMLOP_LAND:
		/* XXX: ii => Bool */
		ival = aml_evalexpr(opargs[0]->v_integer, 
		    opargs[1]->v_integer, opcode);
		break;
	case AMLOP_LLESS:
	case AMLOP_LEQUAL:
	case AMLOP_LGREATER:
	case AMLOP_LNOTEQUAL:
	case AMLOP_LLESSEQUAL:
	case AMLOP_LGREATEREQUAL:
		/* XXX: tt => Bool */
		ival = aml_xcompare(opargs[0], opargs[1], opcode);
		break;

		/* Reference/Store operations */
	case AMLOP_CONDREFOF:
		/* CondRef: rr => I */
		ival = 0;
		if (opargs[0]->node != NULL) {
			aml_freevalue(opargs[1]);

			/* Create Object Reference */
			_aml_setvalue(opargs[1], AML_OBJTYPE_OBJREF, 0, opargs[0]);
			opargs[1]->v_objref.type = AMLOP_REFOF;
			aml_xaddref(opargs[1], "CondRef");
			
			/* Mark that we found it */
			ival = -1;
		}
		break;
	case AMLOP_REFOF:
		/* RefOf: r => ObjRef */
		my_ret = aml_allocvalue(AML_OBJTYPE_OBJREF, 0, opargs[0]);
		my_ret->v_objref.type = AMLOP_REFOF;
		aml_xaddref(my_ret->v_objref.ref, "RefOf");
		break;
	case AMLOP_INDEX:
		/* Index: tir => ObjRef */
		idx = opargs[1]->v_integer;
		if (idx >= opargs[0]->length || idx < 0) {
#ifndef SMALL_KERNEL
			aml_showvalue(opargs[0], 0);
#endif
			aml_die("Index out of bounds %d/%d\n", idx, 
			    opargs[0]->length);
		}
		switch (opargs[0]->type) {
		case AML_OBJTYPE_PACKAGE:
			/* Don't set opargs[0] to NULL */
			if (ret_type == 't' || ret_type == 'i' || ret_type == 'T') {
				my_ret = opargs[0]->v_package[idx];
				aml_xaddref(my_ret, "Index.Package");
			}
			else {
				my_ret = aml_allocvalue(AML_OBJTYPE_OBJREF, 0,
				    opargs[0]->v_package[idx]);
				my_ret->v_objref.type = AMLOP_PACKAGE;
				aml_xaddref(my_ret->v_objref.ref, 
				    "Index.Package");
			}
			break;
		case AML_OBJTYPE_BUFFER:
		case AML_OBJTYPE_STRING:
		case AML_OBJTYPE_INTEGER:
			aml_xconvert(opargs[0], &tmp, AML_OBJTYPE_BUFFER, 0);
			if (ret_type == 't' || ret_type == 'i' || ret_type == 'T') {
				dnprintf(12,"Index.Buf Term: %d = %x\n", 
				    idx, tmp->v_buffer[idx]);
				ival = tmp->v_buffer[idx];
			}
			else {
				dnprintf(12, "Index.Buf Targ\n");
				my_ret = aml_allocvalue(0,0,NULL);
				aml_xcreatefield(my_ret, AMLOP_INDEX, tmp, 
				    8 * idx, 8, NULL, 0, AML_FIELD_BYTEACC);
			}
			aml_xdelref(&tmp, "Index.BufStr");
			break;
		default:
			aml_die("Unknown index : %x\n", opargs[0]->type);
			break;
		}
		aml_xstore(scope, opargs[2], ival, my_ret);
		break;
	case AMLOP_DEREFOF:
		/* DerefOf: t:ObjRef => DataRefObj */
		if (opargs[0]->type == AML_OBJTYPE_OBJREF) {
			my_ret = opargs[0]->v_objref.ref;
			aml_xaddref(my_ret, "DerefOf");
		}
		else {
			my_ret = opargs[0];
			//aml_xaddref(my_ret, "DerefOf");
		}
		break;
	case AMLOP_COPYOBJECT:
		/* CopyObject: t:DataRefObj, s:implename => DataRefObj */
		my_ret = opargs[0];
		aml_freevalue(opargs[1]);
		aml_copyvalue(opargs[1], opargs[0]);
		break;
	case AMLOP_STORE:
		/* Store: t:DataRefObj, S:upername => DataRefObj */
		my_ret = opargs[0];
		aml_xstore(scope, opargs[1], 0, opargs[0]);
		break;

		/* Conversion */
	case AMLOP_TOINTEGER:
		/* Source:CData, Result => Integer */
		aml_xconvert(opargs[0], &my_ret, AML_OBJTYPE_INTEGER, 0);
		aml_xstore(scope, opargs[1], 0, my_ret);
		break;
	case AMLOP_TOBUFFER:
		/* Source:CData, Result => Buffer */
		aml_xconvert(opargs[0], &my_ret, AML_OBJTYPE_BUFFER, 0);
		aml_xstore(scope, opargs[1], 0, my_ret);
		break;
	case AMLOP_TOHEXSTRING:
		/* Source:CData, Result => String */
		aml_xconvert(opargs[0], &my_ret, AML_OBJTYPE_STRING, 'x');
		aml_xstore(scope, opargs[1], 0, my_ret);
		break;
	case AMLOP_TODECSTRING:
		/* Source:CData, Result => String */
		aml_xconvert(opargs[0], &my_ret, AML_OBJTYPE_STRING, 'd');
		aml_xstore(scope, opargs[1], 0, my_ret);
		break;
	case AMLOP_TOSTRING:
		/* Source:B, Length:I, Result => String */
		aml_xconvert(opargs[0], &my_ret, AML_OBJTYPE_STRING, 0);
		aml_die("tostring\n");
		break;
	case AMLOP_CONCAT:
		/* Source1:CData, Source2:CData, Result => CData */
		aml_xconcat(opargs[0], opargs[1], &my_ret);
		aml_xstore(scope, opargs[2], 0, my_ret);
		break;
	case AMLOP_CONCATRES:
		/* Concat two resource buffers: buf1, buf2, result => Buffer */
		aml_xconcatres(opargs[0], opargs[1], &my_ret);
		aml_xstore(scope, opargs[2], 0, my_ret);
		break;
	case AMLOP_MID:
		/* Source:BS, Index:I, Length:I, Result => BS */
		aml_xmid(opargs[0], opargs[1]->v_integer, 
		    opargs[2]->v_integer, &my_ret);
		aml_xstore(scope, opargs[3], 0, my_ret);
		break;
	case AMLOP_MATCH:
		/* Match: Pkg, Op1, Val1, Op2, Val2, Index */
		ival = aml_xmatch(opargs[0], opargs[5]->v_integer,
		    opargs[1]->v_integer, opargs[2]->v_integer,
		    opargs[3]->v_integer, opargs[4]->v_integer);
		break;
	case AMLOP_SIZEOF:
		/* Sizeof: S => i */
		ival = opargs[0]->length;
		break;
	case AMLOP_OBJECTTYPE:
		/* ObjectType: S => i */
		ival = opargs[0]->type;
		break;

		/* Mutex/Event handlers */
	case AMLOP_ACQUIRE:
		/* Acquire: Sw => Bool */
		ival = acpi_xmutex_acquire(scope, opargs[0], 
		    opargs[1]->v_integer);
		break;
	case AMLOP_RELEASE:
		/* Release: S */
		acpi_xmutex_release(scope, opargs[0]);
		break;
	case AMLOP_WAIT:
		/* Wait: Si => Bool */
		ival = acpi_xevent_wait(scope, opargs[0], 
		    opargs[1]->v_integer);
		break;
	case AMLOP_RESET:
		/* Reset: S */
		acpi_xevent_reset(scope, opargs[0]);
		break;
	case AMLOP_SIGNAL:
		/* Signal: S */
		acpi_xevent_signal(scope, opargs[0]);
		break;

		/* Named objects */
	case AMLOP_NAME:
		/* Name: Nt */
		aml_freevalue(cname);
		aml_copyvalue(cname, opargs[1]);
		break;
	case AMLOP_ALIAS:
		/* Alias: nN */
		cname->type = AML_OBJTYPE_OBJREF;
		cname->v_objref.type = AMLOP_ALIAS;
		cname->v_objref.ref = opargs[0];
		while (cname->v_objref.ref->type == AML_OBJTYPE_OBJREF) {
			/* Single indirection level */
			cname->v_objref.ref = cname->v_objref.ref->v_objref.ref;
		}
		aml_xaddref(cname->v_objref.ref, "Alias");
		break;
	case AMLOP_OPREGION:
		/* OpRegion: Nbii */
		cname->type = AML_OBJTYPE_OPREGION;
		cname->v_opregion.iospace = opargs[1]->v_integer;
		cname->v_opregion.iobase = opargs[2]->v_integer;
		cname->v_opregion.iolen = opargs[3]->v_integer;
		cname->v_opregion.flag = 0;
		break;
	case AMLOP_DATAREGION:
		/* DataTableRegion: N,t:SigStr,t:OemIDStr,t:OemTableIDStr */
		cname->type = AML_OBJTYPE_OPREGION;
		cname->v_opregion.iospace = GAS_SYSTEM_MEMORY;
		cname->v_opregion.iobase = 0;
		cname->v_opregion.iolen = 0;
		aml_die("AML-DataTableRegion\n");
		break;
	case AMLOP_EVENT:
		/* Event: N */
		cname->type = AML_OBJTYPE_EVENT;
		cname->v_integer = 0;
		break;
	case AMLOP_MUTEX:
		/* Mutex: Nw */
		cname->type = AML_OBJTYPE_MUTEX;
		cname->v_mtx.synclvl = opargs[1]->v_integer;
		break;
	case AMLOP_SCOPE:
		/* Scope: NT */
		mscope = aml_xpushscope(scope, opargs[1], cname->node, opcode);
		break;
	case AMLOP_DEVICE:
		/* Device: NT */
		cname->type = AML_OBJTYPE_DEVICE;
		mscope = aml_xpushscope(scope, opargs[1], cname->node, opcode);
		break;
	case AMLOP_THERMALZONE:
		/* ThermalZone: NT */
		cname->type = AML_OBJTYPE_THERMZONE;
		mscope = aml_xpushscope(scope, opargs[1], cname->node, opcode);
		break;
	case AMLOP_POWERRSRC:
		/* PowerRsrc: NbwT */
		cname->type = AML_OBJTYPE_POWERRSRC;
		cname->v_powerrsrc.pwr_level = opargs[1]->v_integer;
		cname->v_powerrsrc.pwr_order = opargs[2]->v_integer;
		mscope = aml_xpushscope(scope, opargs[3], cname->node, opcode);
		break;
	case AMLOP_PROCESSOR:
		/* Processor: NbdbT */
		cname->type = AML_OBJTYPE_PROCESSOR;
		cname->v_processor.proc_id = opargs[1]->v_integer;
		cname->v_processor.proc_addr = opargs[2]->v_integer;
		cname->v_processor.proc_len = opargs[3]->v_integer;
		mscope = aml_xpushscope(scope, opargs[4], cname->node, opcode);
		break;
	case AMLOP_METHOD:
		/* Method: NbM */
		cname->type = AML_OBJTYPE_METHOD;
		cname->v_method.flags = opargs[1]->v_integer;
		cname->v_method.start = opargs[2]->v_buffer;
		cname->v_method.end = cname->v_method.start + opargs[2]->length;
		cname->v_method.base = aml_root.start;
		break;

		/* Field objects */
	case AMLOP_CREATEFIELD:
		/* Source:B, BitIndex:I, NumBits:I, FieldName */
		aml_xcreatefield(cname, opcode, opargs[0], opargs[1]->v_integer,
		    opargs[2]->v_integer, NULL, 0, 0);
		break;
	case AMLOP_CREATEBITFIELD:
		/* Source:B, BitIndex:I, FieldName */
		aml_xcreatefield(cname, opcode, opargs[0], opargs[1]->v_integer,    
		    1, NULL, 0, 0);	
		break;
	case AMLOP_CREATEBYTEFIELD:
		/* Source:B, ByteIndex:I, FieldName */
		aml_xcreatefield(cname, opcode, opargs[0], opargs[1]->v_integer*8,  
		    8, NULL, 0, AML_FIELD_BYTEACC);
		break;
	case AMLOP_CREATEWORDFIELD:
		/* Source:B, ByteIndex:I, FieldName */
		aml_xcreatefield(cname, opcode, opargs[0], opargs[1]->v_integer*8, 
		    16, NULL, 0, AML_FIELD_WORDACC);
		break;
	case AMLOP_CREATEDWORDFIELD:
		/* Source:B, ByteIndex:I, FieldName */
		aml_xcreatefield(cname, opcode, opargs[0], opargs[1]->v_integer*8, 
		    32, NULL, 0, AML_FIELD_DWORDACC);
		break;
	case AMLOP_CREATEQWORDFIELD:
		/* Source:B, ByteIndex:I, FieldName */
		aml_xcreatefield(cname, opcode, opargs[0], opargs[1]->v_integer*8, 
		    64, NULL, 0, AML_FIELD_QWORDACC);
		break;
	case AMLOP_FIELD:
		/* Field: n:OpRegion, b:Flags, F:ieldlist */
		mscope = aml_xpushscope(scope, opargs[2], scope->node, opcode);
		aml_xparsefieldlist(mscope, opcode, opargs[1]->v_integer, 
		    opargs[0], NULL, 0);
		mscope = NULL;
		break;
	case AMLOP_INDEXFIELD:
		/* IndexField: n:Index, n:Data, b:Flags, F:ieldlist */
		mscope = aml_xpushscope(scope, opargs[3], scope->node, opcode);
		aml_xparsefieldlist(mscope, opcode, opargs[2]->v_integer, 
		    opargs[1], opargs[0], 0);
		mscope = NULL;
		break;
	case AMLOP_BANKFIELD:
		/* BankField: n:OpRegion, n:Field, i:Bank, b:Flags, F:ieldlist */
		mscope = aml_xpushscope(scope, opargs[4], scope->node, opcode);
		aml_xparsefieldlist(mscope, opcode, opargs[3]->v_integer, 
		    opargs[0], opargs[1], opargs[2]->v_integer);
		mscope = NULL;
		break;

		/* Misc functions */
	case AMLOP_STALL:
		/* Stall: i */
		acpi_stall(opargs[0]->v_integer);
		break;
	case AMLOP_SLEEP:
		/* Sleep: i */
		acpi_sleep(opargs[0]->v_integer);
		break;
	case AMLOP_NOTIFY:
		/* Notify: Si */
		dnprintf(50,"Notifying: %s %x\n", 
		    aml_nodename(opargs[0]->node), 
		    opargs[1]->v_integer);
		aml_notify(opargs[0]->node, opargs[1]->v_integer);
		break;
	case AMLOP_TIMER:
		/* Timer: => i */
		ival = 0xDEADBEEF;
		break;
	case AMLOP_FATAL:
		/* Fatal: bdi */
		aml_die("AML FATAL ERROR: %x,%x,%x\n",
		    opargs[0]->v_integer, opargs[1]->v_integer, 
		    opargs[2]->v_integer);
		break;
	case AMLOP_LOADTABLE:
		/* LoadTable(Sig:Str, OEMID:Str, OEMTable:Str, [RootPath:Str], [ParmPath:Str], 
		   [ParmData:DataRefObj]) => DDBHandle */
		aml_die("LoadTable");
		break;
	case AMLOP_LOAD:
		/* Load(Object:NameString, DDBHandle:SuperName) */
		tmp = opargs[0];
		if (tmp->type != AML_OBJTYPE_OPREGION || 
		    tmp->v_opregion.iospace != GAS_SYSTEM_MEMORY) {
			aml_die("LOAD: not a memory region!\n");
		}

		/* Create buffer and read from memory */
		_aml_setvalue(opargs[1], AML_OBJTYPE_BUFFER,
		    tmp->v_opregion.iolen, NULL);
		aml_xgasio(tmp->v_opregion.iospace, tmp->v_opregion.iobase, 
		    tmp->v_opregion.iolen, 
		    opargs[1]->v_buffer, ACPI_IOREAD, 8, "");
		
		/* Validate that this is a SSDT */
		if (!valid_acpihdr(opargs[1]->v_buffer, opargs[1]->length, 
			"SSDT")) {
			aml_die("LOAD: Not a SSDT!\n");
		}

		/* Parse block: set header bytes to NOP */
		memset(opargs[1]->v_buffer, AMLOP_NOP, sizeof(struct acpi_table_header));
		mscope = aml_xpushscope(scope, opargs[1], scope->node, 
		    AMLOP_SCOPE);
		break;
	case AMLOP_UNLOAD:
		/* DDBHandle */
		aml_die("Unload");
		break;

		/* Control Flow */
	case AMLOP_IF:
		/* Arguments: iT or iTbT */
		if (opargs[0]->v_integer) {
			dnprintf(10,"parse-if @ %.4x\n", pc);
			mscope = aml_xpushscope(scope, opargs[1], scope->node,
			    AMLOP_IF);
		}
		else if (opargs[3] != NULL) {
			dnprintf(10,"parse-else @ %.4x\n", pc);
			mscope = aml_xpushscope(scope, opargs[3], scope->node,
			    AMLOP_ELSE);
		}
		break;
	case AMLOP_WHILE:
		mscope = aml_xpushscope(scope, opargs[0], scope->node,
		    AMLOP_WHILE);
		while (mscope->pos != NULL) {
			/* At beginning of scope.. reset and perform test */
			mscope->pos = mscope->start;
			tmp = aml_xparse(mscope, AML_ARG_INTEGER, "While-Test");
			ival = tmp->v_integer;
			aml_xdelref(&tmp, "while");

			dnprintf(10,"@@@@@@ WHILE: %llx @ %x\n", ival, pc);
			if (ival == 0) {
				break;
			}
			aml_xparse(mscope, 'T', "While");
		}
		aml_xpopscope(mscope);
		mscope = NULL;
		break;
	case AMLOP_BREAK:
		/* Break: Find While Scope parent, mark type as null */
		mscope = aml_xfindscope(scope, AMLOP_WHILE, 1);
		mscope->pos = NULL;
		mscope = NULL;
		break;
	case AMLOP_CONTINUE:
		/* Find Scope.. mark all objects as invalid on way to root */
		mscope = aml_xfindscope(scope, AMLOP_WHILE, 1);
		mscope->pos = mscope->start;
		mscope = NULL;
		break;
	case AMLOP_RETURN:
		mscope = aml_xfindscope(scope, AMLOP_METHOD, 1);
		if (mscope->retv) {
			aml_die("already allocated\n");
		}
		mscope->retv = aml_allocvalue(0,0,NULL);
		aml_copyvalue(mscope->retv, opargs[0]);
		mscope = NULL;
		break;
	default:
		/* may be set direct result */
		aml_die("Unknown opcode: %x:%s\n", opcode, htab->mnem);
		break;
	}
	if (mscope != NULL) {
		/* Change our scope to new scope */
		scope = mscope;
	}
	if ((ret_type == 'i' || ret_type == 't') && my_ret == NULL) {
		dnprintf(10,"quick: %.4x [%s] alloc return integer = 0x%llx\n",
		    pc, htab->mnem, ival);
		my_ret = aml_allocvalue(AML_OBJTYPE_INTEGER, ival, NULL);
	}
	if (ret_type == 'i' && my_ret && my_ret->type != AML_OBJTYPE_INTEGER) {
		dnprintf(10,"quick: %.4x convert to integer %s -> %s\n", 
		    pc, htab->mnem, stype);
		aml_xconvert(my_ret, &my_ret, AML_OBJTYPE_INTEGER, 0);
	}
	if (my_ret != NULL) {
		/* Display result */
		dnprintf(20,"quick: %.4x %18s %c %.4x\n", pc, stype, 
		    ret_type, my_ret->stack);
	}

	/* End opcode: display/free arguments */
	for (idx=0; optype[idx] != 0; idx++) {
		if (opargs[idx] == my_ret || optype[idx] == 'N')
			opargs[idx] = NULL;
		aml_xdelref(&opargs[idx], "oparg");
	}

	/* If parsing whole scope and not done, start again */
	if (ret_type == 'T') {
		aml_xdelref(&my_ret, "scope.loop");
		while (scope->pos >= scope->end && scope != iscope) {
			/* Pop intermediate scope */
			scope = aml_xpopscope(scope);
		}
		if (scope->pos && scope->pos < scope->end)
			goto start;
	}

	odp--;
	dnprintf(50, ">>return [%s] %s %c %p\n", aml_nodename(scope->node), 
	    stype, ret_type, my_ret);
	return my_ret;
}

int
acpi_parse_aml(struct acpi_softc *sc, u_int8_t *start, u_int32_t length)
{
	struct aml_scope *scope;
	struct aml_value res;

	dsdt_softc = sc;

	aml_root.start = start;
	memset(&res, 0, sizeof(res));
	res.type = AML_OBJTYPE_SCOPE;
	res.length = length;
	res.v_buffer = start;
	
	/* Push toplevel scope, parse AML */
	scope = aml_xpushscope(NULL, &res, &aml_root, AMLOP_SCOPE);
	aml_busy++;
	aml_xparse(scope, 'T', "TopLevel");
	aml_busy--;
	aml_xpopscope(scope);

	return 0;
}

/*
 * @@@: External API
 *
 * evaluate an AML node
 * Returns a copy of the value in res  (must be freed by user)
 */
int
aml_evalnode(struct acpi_softc *sc, struct aml_node *node,
    int argc, struct aml_value *argv, struct aml_value *res)
{
	struct aml_value *xres;
	
	if (node == NULL || node->value == NULL)
		return (ACPI_E_BADVALUE);
	if (res)
		memset(res, 0, sizeof(*res));
	dnprintf(12,"EVALNODE: %s %d\n", aml_nodename(node), acpi_nalloc);
	switch (node->value->type) {
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_PACKAGE:
	case AML_OBJTYPE_STRING:
	case AML_OBJTYPE_BUFFER:
	case AML_OBJTYPE_PROCESSOR:
	case AML_OBJTYPE_THERMZONE:
	case AML_OBJTYPE_POWERRSRC:
		if (res)
			aml_copyvalue(res, node->value);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
	case AML_OBJTYPE_METHOD:
		aml_busy++;
		xres = aml_xeval(NULL, node->value, 't', argc, argv);
		aml_busy--;
		if (res && xres)
			aml_copyvalue(res, xres);
		if (xres != node->value)
			aml_xdelref(&xres, "EvalNode");
		break;
	default:
		return (-1);
	}
	return (0);
}

/*
 * evaluate an AML name
 * Returns a copy of the value in res  (must be freed by user)
 */
int
aml_evalname(struct acpi_softc *sc, struct aml_node *parent, const char *name,
    int argc, struct aml_value *argv, struct aml_value *res)
{
	parent = aml_searchname(parent, name);
	return aml_evalnode(sc, parent, argc, argv, res);
}

/*
 * evaluate an AML integer object
 */
int
aml_evalinteger(struct acpi_softc *sc, struct aml_node *parent,
    const char *name, int argc, struct aml_value *argv, int64_t *ival)
{
	struct aml_value res;
	int rc;

	parent = aml_searchname(parent, name);
	rc = aml_evalnode(sc, parent, argc, argv, &res);
	*ival = aml_val2int(&res);
	aml_freevalue(&res);

	return rc;
}

/*
 * Search for an AML name in namespace.. root only 
 */
struct aml_node *
aml_searchname(struct aml_node *root, const void *vname)
{
	char *name = (char *)vname;

	dnprintf(25,"Searchname: %s:%s = ", aml_nodename(root), vname);
	if (*name == AMLOP_ROOTCHAR) {
		root = &aml_root;
		name++;
	}
	while (*name != 0) {
		root = __aml_search(root, name, 0);
		name += (name[4] == '.') ? 5 : 4;
	}
	dnprintf(25,"%p %s\n", root, aml_nodename(root));
	return root;
}

/*
 * Search for relative name
 */
struct aml_node *
aml_searchrel(struct aml_node *root, const void *vname)
{
	struct aml_node *res;

	while (root) {
		res = aml_searchname(root, vname);
		if (res != NULL)
			return res;
		root = root->parent;
	}
	return NULL;
}
