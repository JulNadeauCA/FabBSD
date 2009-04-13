/*	$FabBSD$	*/
/*	Public domain	*/

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <cnc.h>

#define NGC_LINE_MAX	256	/* As specified by RS274/NGC */
#define NGC_WORD_MAX	16	/* Characters per word */
#define NGC_NPARAMS	5401	/* Maximum persistent parameters (1..5400) */
#define NGC_NWORKOFFS	10	/* Maximum work offsets (1..9) */
#define NGC_NAXES	6	/* Supported # of axes */

typedef double ngc_real_t;
typedef struct ngc_vector {
	ngc_real_t v[NGC_NAXES];
} ngc_vec_t;

struct ngc_prog;
struct ngc_block;

/* Interpreter modes */
enum ngc_interp_mode {		/* Interpretation mode */
	NGC_MILLING,		/* For milling machines */
	NGC_TURNING		/* For lathes */
};
enum ngc_distance_mode {	/* Dimensioning mode */
	NGC_INCREMENTAL,	/* Incremental coordinates */
	NGC_ABSOLUTE		/* Absolute coordinates */
};
enum ngc_unit_system {		/* Unit system */
	NGC_METRIC,		/* Metric dimensions */
	NGC_INCH		/* English dimensions */
};
enum ngc_feedrate_mode {	/* Feed rate mode */
	NGC_UNITS_PER_MIN,	/* F = (inches/degs/mm) per minute */
	NGC_ONE_OVER_MIN	/* Complete move in 1/F minutes */
};
enum ngc_plane_mode {		/* Plane selection for arcs */
	NGC_PLANE_XY,		/* G17 (X = radius, Y = angle) */
	NGC_PLANE_ZX,		/* G18 (Z = radius, Y = angle) */
	NGC_PLANE_YZ		/* G19 (Y = radius, Z = angle) */
};
enum ngc_cro_mode {		/* Cutter radius offset mode */
	NGC_CUTTER_OFFS_OFF,	/* G40: Offset disabled */
	NGC_CUTTER_OFFS_LEFT,	/* G41: Keep left of contour */
	NGC_CUTTER_OFFS_RIGHT	/* G42: Keep right of contour */
};
enum ngc_pathctl_mode {
	NGC_EXACT_PATH,		/* G61: Exact path mode */
	NGC_EXACT_STOP,		/* G61.1: Exact path mode */
	NGC_CONT_PATH		/* G64: Continuous mode */
};
enum ngc_retract_mode {		/* Retract perpendicular to selected plane... */
	NGC_RETRACT_POSITION,	/* G99: ...to position indicated by R */
	NGC_RETRACT_PREVIOUS	/* G98: ...to position prior to cycle start
				   or R, whichever position is higher */
};
enum ngc_direction {		/* Direction of rotation */
	NGC_CW,			/* Clockwise */
	NGC_CCW			/* Counterclockwise */
};
enum ngc_axis_type {		/* Type of axis */
	NGC_LINEAR,		/* Controls linear motion */
	NGC_ROTARY		/* Controls rotary motion */
};

/* Address description */
enum ngc_address_type {
	NGC_UNUSED,		/* Not a valid address */
	NGC_INTEGER,		/* Integer */
	NGC_SIGNED_REAL,	/* Signed real number */
	NGC_UNSIGNED_REAL,	/* Unsigned real number */
};
struct ngc_address {
	char addr;			/* Address letter */
	enum ngc_address_type type;	/* Data type */
	const char *descr;		/* Description */
};

/* Implemented command */
struct ngc_command {
	const char *name;			/* Command name (e.g., "G0") */
	int modalgrp;				/* Modal group */
	const char *descr;			/* Short description */
	int (*fn)(const struct ngc_block *bl);
};

/* Word in instruction block */
struct ngc_word {
	char type;			/* Word type */
	union {
		int i;			/* Integer */
		ngc_real_t f;		/* Real number */
	} data;
	TAILQ_ENTRY(ngc_word) words;
};

/* Instruction block */
struct ngc_block {
	int n;				/* RS274 block number (Nxxxx) or -1 */
	int line;			/* Actual line number in file */
	u_int8_t flags;
#define NGC_BLOCK_OPTIONAL_DELETE 0x01	/* Optional delete flag */
	struct ngc_prog *prog;		/* Back pointer to program */
	TAILQ_HEAD(,ngc_word) words;	/* Defined words */
};

/* Input program */
struct ngc_prog {
	char path[MAXPATHLEN];		/* Input file path */
	int name;			/* Program number (EIA / ISO) */
	int nRefs;			/* # of calls */
	int nCalls;			/* # of invocations */
	struct ngc_block *blks;		/* Instruction blocks */
	u_int            nBlks;
	TAILQ_ENTRY(ngc_prog) progs;
};
TAILQ_HEAD(ngc_progq, ngc_prog);

/* Modal groups for G commands */
enum ngc_nonmodal_g {
	NGC_G_NONMODAL = 0		/* Non-modal G commands */
};
enum ngc_modal_g {
	NGC_GMOT	= 1,		/* Motion */
	NGC_GPLANESEL	= 2,		/* Plane selection */
	NGC_GDIMMODE	= 3,		/* Dimensioning mode */
	NGC_GFEEDRATE	= 5,		/* Feed rate */
	NGC_GUNITS	= 6,		/* Unit selection */
	NGC_GCUTTEROFFS = 7,		/* Cutter radius offset */
	NGC_GTLO	= 8,		/* Tool length offset */
	NGC_GCANNEDRET	= 10,		/* Return mode in canned cycles */
	NGC_GCSYSSEL	= 12,		/* Coordinate system selection */
	NGC_GPATHCTL	= 13		/* Path control mode */
};

/* Modal groups for M codes */
enum ngc_modal_m {
	NGC_GMSTOP	= 4,		/* Stopping */
	NGC_GMTOOLCHG	= 6,		/* Tool changer commands */
	NGC_GMSPINDLE	= 7,		/* Spindle commands */
	NGC_GMCOOLANT	= 8,		/* Coolant commands */
	NGC_GMFSOVR	= 9		/* Feed/speed override */
};

extern int ngc_verbose;
extern int ngc_warnings;
extern int ngc_simulate;

extern struct ngc_progq ngc_progs;		/* Programs */
extern ngc_real_t       ngc_params[NGC_NPARAMS];/* Parameters */
extern int              ngc_csys_bases[10];	/* Coord sys param bases */
extern ngc_real_t       ngc_csys_offset[NGC_NAXES]; /* Coord sys offsets */
extern const struct ngc_address	ngc_maddr[26]; 	/* For milling mode */
extern const struct ngc_address	ngc_taddr[26]; 	/* For turning mode */
extern const struct ngc_address	*ngc_addr; 	/* For current mode */

extern ngc_vec_t		 ngc_pos;	/* Current position */
extern ngc_real_t		 ngc_linscale;	/* Linear scaling factor */
extern ngc_real_t		 ngc_rotscale;	/* Rotary scaling factor */
extern ngc_real_t		 ngc_feedscale;	/* mm/min to steps/sec */

extern const char		*ngc_axis_words; /* Axis address words */
extern const enum ngc_axis_type  ngc_axis_types[NGC_NAXES]; /* Linear/rotary */

extern enum ngc_interp_mode	 ngc_mode;	/* Interpreter mode */
extern const char		*ngc_modenames[];
extern enum ngc_distance_mode	 ngc_distmode;	/* Dimensioning mode */
extern enum ngc_unit_system	 ngc_units;	/* Unit system */
extern enum ngc_feedrate_mode	 ngc_feedmode;	/* Feedrate input mode */
extern ngc_real_t		 ngc_feedfast;	/* Feedrate for rapid motion */
extern ngc_real_t		 ngc_feedrate;	/* Feedrate for feed motion */
extern enum ngc_plane_mode	 ngc_planesel;	/* Plane selection */
extern enum ngc_cro_mode	 ngc_cromode;	/* Cutter radius offset */
extern int			 ngc_tlo;	/* Tool length offset */
extern enum ngc_pathctl_mode	 ngc_pathctl;	/* Path control mode */
extern enum ngc_retract_mode	 ngc_retract;	/* Canned cycle retract mode */

extern int ngc_spinspeed;	/* Effective spindle speed */
extern int ngc_tool;		/* Selected tool */
extern int ngc_csys;		/* Effective coordinate system */

void             verbose(const char *, ...);
void             warning(const char *, ...);
struct ngc_prog *ngc_prog_new(int, const char *);
void             ngc_prog_del(struct ngc_prog *);
void             ngc_nltonul(char *, size_t);
int              ngc_block_exec(struct ngc_prog *, int);

struct ngc_word *ngc_getcmd(const struct ngc_block *, char, int);
struct ngc_word *ngc_getparam(const struct ngc_block *, char);
int              ngc_axiswords_to_vec(const struct ngc_block *, ngc_vec_t *);

void      ngc_vec_zero(ngc_vec_t *);
ngc_vec_t ngc_vec_add(const ngc_vec_t *, const ngc_vec_t *);
ngc_vec_t ngc_vec_mult(const ngc_vec_t *, const ngc_vec_t *);
void      ngc_vec_print(char *, size_t, const ngc_vec_t *)
              __attribute__((__bounded__(__string__,1,2)));

ngc_real_t ngc_units_to_mm(ngc_real_t);
