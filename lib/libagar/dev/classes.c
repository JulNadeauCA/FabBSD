/*
 * Copyright (c) 2008 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Display registered AG_Object classes.
 */

#include <core/core.h>

#include <gui/window.h>
#include <gui/table.h>

#include "dev.h"

static void
GenClassTable(AG_Table *tbl, AG_ObjectClass *cls)
{
	AG_ObjectClass *subcls;
	
	AG_TableAddRow(tbl, "%s:%d:%s:%s",
	    cls->name, cls->size,
	    cls->libs[0] != '\0' ? cls->libs : "(none)",
	    cls->hier);

	TAILQ_FOREACH(subcls, &cls->sub, subclasses)
		GenClassTable(tbl, subcls);
}

static void
PollClasses(AG_Event *event)
{
	AG_Table *tbl = AG_SELF();

	/* XXX tree */
	AG_TableBegin(tbl);
	GenClassTable(tbl, agClassTree);
	AG_TableEnd(tbl);
}

AG_Window *
DEV_ClassInfo(void)
{
	AG_Window *win;
	AG_Table *tbl;

	if ((win = AG_WindowNewNamed(0, "DEV_ClassInfo")) == NULL) {
		return (NULL);
	}
	AG_WindowSetCaption(win, _("Registered classes"));

	tbl = AG_TableNewPolled(win, AG_TABLE_EXPAND,
	    PollClasses, NULL);
	AG_TableAddCol(tbl, _("Name"), "<XXXXXXXXXXXXXXX>", NULL);
	AG_TableAddCol(tbl, _("Size"), "<XXXX>", NULL);
	AG_TableAddCol(tbl, _("Modules"), "<XXXXXXX>", NULL);
	AG_TableAddCol(tbl, _("Hierarchy"), NULL, NULL);

	AG_WindowSetGeometryAlignedPct(win, AG_WINDOW_MC, 60, 60);
	return (win);
}
