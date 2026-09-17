#ifndef RUFUX_GUI_H
#define RUFUX_GUI_H
// GTK4 frontend (replaces Win32 src/ui.c / rufus.c dialog).
// Called from main when --gui is passed and HAVE_GTK=1.
int rufux_gui_run(int argc, char **argv);
#endif
