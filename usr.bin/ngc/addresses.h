/*	$FabBSD$	*/
/*	Public domain	*/

/*
 * Addresses for milling format
 */
const struct ngc_address ngc_maddr[26] = {
	{ 'A', NGC_SIGNED_REAL,		"Rotary axis about X (degs)" },
	{ 'B', NGC_SIGNED_REAL,		"Rotary axis about Y (degs)" },
	{ 'C', NGC_UNUSED,		NULL },
	{ 'D', NGC_INTEGER,		"Cutter radius offset#" },
	{ 'E', NGC_UNUSED,		NULL },
	{ 'F', NGC_UNSIGNED_REAL,	"Feed rate" },
	{ 'G', NGC_INTEGER,		"Preparatory command" },
	{ 'H', NGC_INTEGER,		"Tool position/length offset#" },
	{ 'I', NGC_SIGNED_REAL,		"Arcs: arc center mod for X; "
					"Fixed cycles: x-shift" },
	{ 'J', NGC_SIGNED_REAL,		"Arcs: arc center mod for Y; "
					"Fixed cycles: y-shift" },
	{ 'K', NGC_SIGNED_REAL,		"Arcs: arc center mod for Z" },
	{ 'L', NGC_INTEGER,		"Loop iteration count" },
	{ 'M', NGC_INTEGER,		"Miscellaneous command" },
	{ 'N', NGC_INTEGER,		"Block number" },
	{ 'O', NGC_INTEGER,		"EIA/ISO program number" },
	{ 'P', NGC_INTEGER,		"Subroutine# in calls; "
					"G10: work offset#; "
					"G54.1: work offset#; "
					"G04: dwell time in ms; "
					"M99: block# in main" },
	{ 'Q', NGC_SIGNED_REAL,		"G73/G83: depth of peck; "
					"G76/G87: shift amount" },
	{ 'R', NGC_SIGNED_REAL,		"Fixed cycles: retract point; "
					"Arcs: radius" },
	{ 'S', NGC_INTEGER,		"Spindle speed (RPM)" },
	{ 'T', NGC_INTEGER,		"Tool command" },
	{ 'U', NGC_UNUSED,		NULL },
	{ 'V', NGC_UNUSED,		NULL },
	{ 'W', NGC_UNUSED,		NULL },
	{ 'X', NGC_SIGNED_REAL,		"X-axis coordinate; "
					"G04: dwell time in secs" },
	{ 'Y', NGC_SIGNED_REAL,		"Y-axis coordinate" },
	{ 'Z', NGC_SIGNED_REAL,		"Z-axis coordinate" }
};

/*
 * Addresses for turning format
 */
const struct ngc_address ngc_taddr[26] = {
	{ 'A', NGC_INTEGER,		"G76: angle of thread; "
					"Direct drawing input: angle" },
	{ 'B', NGC_UNUSED,		NULL },
	{ 'C', NGC_SIGNED_REAL,		"Direct drawing input: chamfer" },
	{ 'D', NGC_INTEGER,		"G73: # of divisions; "
					"G71/G72: depth of cut; "
					"G74/G75: relief amount; "
					"G76: depth of first thread" },
	{ 'E', NGC_UNSIGNED_REAL,	"Precision feedrate for threading" },
	{ 'F', NGC_UNSIGNED_REAL,	"Feed rate" },
	{ 'G', NGC_INTEGER,		"Preparatory command" },
	{ 'H', NGC_UNUSED,		NULL },
	{ 'I', NGC_SIGNED_REAL,		"Arcs: arc center mod for X; "
					"Cycles: taper height in X; "
					"G73: X-axis relief; "
					"Direction of chamfering; "
					"G74: X motion amount" },
	{ 'J', NGC_UNUSED,		NULL },
	{ 'K', NGC_SIGNED_REAL,		"Arcs: arc center mod for Z; "
					"Cycles: taper height in Z; "
					"G73: Z-axis relief; "
					"Direction of chamfering; "
					"G75: Z motion amount; "
					"G76: thread depth" },
	{ 'L', NGC_INTEGER,		"Loop iteration count" },
	{ 'M', NGC_INTEGER,		"Miscellaneous command" },
	{ 'N', NGC_INTEGER,		"Block number" },
	{ 'O', NGC_INTEGER,		"EIA/ISO program number" },
	{ 'P', NGC_INTEGER,		"Subroutine# in calls; "
					"G10: work offset#; "
					"G04: dwell time in ms" },
	{ 'Q', NGC_INTEGER,		"G71/G72: End block number" },
	{ 'R', NGC_SIGNED_REAL,		"Arcs: radius" },
	{ 'S', NGC_INTEGER,		"Spindle speed (RPM)" },
	{ 'T', NGC_INTEGER,		"Tool command" },
	{ 'U', NGC_SIGNED_REAL,		"Incremental value in X; "
					"Stock allowance in X; "
					"G04: dwell time in secs" },
	{ 'V', NGC_UNUSED,		NULL },
	{ 'W', NGC_SIGNED_REAL,		"Incremental value in Z; "
					"Stock allowance in Z" },
	{ 'X', NGC_SIGNED_REAL,		"Absolute value in X; "
					"G04: dwell time in secs" },
	{ 'Y', NGC_UNUSED,		NULL },
	{ 'Z', NGC_SIGNED_REAL,		"Absolute value in Z" }
};

const struct ngc_address *ngc_addr = NULL;
