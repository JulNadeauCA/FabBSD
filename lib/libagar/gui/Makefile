TOP=..
include ${TOP}/Makefile.config

SUBDIR=	fonts

LIB=		ag_gui
LIB_INSTALL=	Yes
LIB_SHARED=	Yes
LIB_GUID=	"71a13512-13e7-44a6-ba69-e868296cdc34"
LIB_LINKS=	ag_core_static pthreads SDL opengl freetype

SRCS=	gui.c view.c widget.c window.c style.c style_default.c iconmgr.c \
	colors.c cursors.c ttf.c text.c keymap_ascii.c keymap_utf8.c \
	keymap_latin1.c keymap_compose.c keysyms.c editable.c \
	box.c label.c button.c checkbox.c textbox.c primitive.c radio.c \
	fixed_plotter.c tlist.c scrollbar.c spinbutton.c titlebar.c toolbar.c \
	fspinbutton.c combo.c ucombo.c units.c nlunits.c hsvpal.c \
	mspinbutton.c mfspinbutton.c statusbar.c menu.c menu_view.c \
	treetbl.c separator.c notebook.c pixmap.c file_dlg.c objsel.c \
	fixed.c numerical.c table.c glview.c mpane.c pane.c console.c graph.c \
	socket.c icon.c progress_bar.c slider.c text_cache.c load_surface.c \
	load_color.c load_xcf.c file_selector.c scrollview.c font_selector.c \
	time_sdl.c

MAN3=	AG_View.3 AG_Widget.3 AG_Button.3 AG_FixedPlotter.3 AG_Checkbox.3 \
	AG_Label.3 AG_Radio.3 AG_Textbox.3 AG_Window.3 AG_Scrollbar.3 \
	AG_Tlist.3 AG_Text.3 AG_Box.3 AG_HBox.3 AG_VBox.3 AG_Combo.3 \
	AG_FSpinbutton.3 AG_UCombo.3 AG_Spinbutton.3 AG_Units.3 AG_HSVPal.3 \
	AG_Menu.3 AG_SizeSpec.3 AG_Table.3 AG_Pane.3 AG_MPane.3 AG_Fixed.3 \
	AG_Pixmap.3 AG_Colors.3 AG_Notebook.3 AG_Toolbar.3 AG_FileDlg.3 \
	AG_Console.3 AG_GLView.3 AG_Graph.3 AG_Icon.3 AG_Socket.3 AG_Cursor.3 \
	AG_Separator.3 AG_Numerical.3 AG_Style.3 AG_ProgressBar.3 \
	AG_Editable.3 AG_Slider.3 AG_Surface.3 AG_Scrollview.3 AG_Rect.3 \
	AG_Titlebar.3 AG_FontSelector.3 AG_Treetbl.3

CFLAGS+=-D_AGAR_GUI_INTERNAL ${SDL_CFLAGS} ${FREETYPE_CFLAGS} ${OPENGL_CFLAGS} \
	${X11_CFLAGS} ${JPEG_CFLAGS} ${MATH_CFLAGS} ${GETTEXT_CFLAGS}

include .manlinks.mk
include ${TOP}/mk/build.lib.mk
include ${TOP}/mk/build.man.mk
