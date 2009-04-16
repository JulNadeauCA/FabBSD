/*	$FabBSD$	*/
/*	Public domain	*/

/*
 * Modal groups. Only one member of a modal group may be effective at any
 * point during program execution.
 */

static const int  Gnonmodal[] = { 4, 10, 28, 30, 53, 92, 92001, 92002, 92003,	-1 };
static const int    Gmotion[] = { 0, 1, 2, 3, 33, 38002, 76, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,	-1 };
static const int  Gplanesel[] = { 17, 18, 19,		-1 };
static const int   Gdimmode[] = { 90, 91,		-1 };
static const int  Mstopping[] = { 0, 1, 2, 30, 60,	-1 };
static const int  Gfeedrate[] = { 93, 94,		-1 };
static const int   Miopoint[] = { 62, 63, 64, 65,	-1 };
static const int     Gunits[] = { 20, 21,		-1 };
static const int   Mtoolchg[] = { 6,			-1 };
static const int       Gcro[] = { 40, 41, 42,		-1 };
static const int   Mspindle[] = { 3, 4, 5,		-1 };
static const int       Gtlo[] = { 43, 49,		-1 };
static const int   Mcoolant[] = { 7, 8, 9,		-1 };
static const int    Mswover[] = { 48, 49, 50, 51, 52, 53,	-1 };
static const int Gcannedret[] = { 98, 99,		-1 };
static const int   Gcsyssel[] = { 54, 55, 56, 57, 58, 59, 59001, 59002, 59003,	-1 };
static const int   Gpathctl[] = { 61, 61001, 64,	-1 };

const struct ngc_modal_group ngc_modal_grp[] = {
	{  0, 'G', "Nonmodal",			Gnonmodal	},
	{  1, 'G', "Motion",			Gmotion		},
	{  2, 'G', "Plane Selection",		Gplanesel	},
	{  3, 'G', "Dimensioning Mode",		Gdimmode	},
	{  4, 'M', "Stopping",			Mstopping	},
	{  5, 'G', "Feed Rate Mode",		Gfeedrate	},
	{  5, 'M', "Set I/O Point",		Miopoint	},
	{  6, 'G', "Unit Selection",		Gunits		},
	{  6, 'M', "Tool Change",		Mtoolchg	},
	{  7, 'G', "Cutter Radius Offset",	Gcro		},
	{  7, 'M', "Spindle Commands",		Mspindle	},
	{  8, 'G', "Tool Length Offset",	Gtlo		},
	{  8, 'M', "Coolant",			Mcoolant	},
	{  9, 'M', "Switch overrides",		Mswover		},
	{ 10, 'G', "Canned Cycle Return",	Gcannedret	},
	{ 12, 'G', "Coord. Sys. Selection",	Gcsyssel	},
	{ 13, 'G', "Path Control Mode",		Gpathctl	}
};
const int ngc_modal_grp_count = sizeof(ngc_modal_grp)/sizeof(ngc_modal_grp[0]);
