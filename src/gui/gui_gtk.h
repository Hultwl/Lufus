#ifndef LUFUS_GUI_H
#define LUFUS_GUI_H
// GTK4 frontend (replaces Win32 src/ui.c / rufus.c dialog).
// Called from main when --gui is passed and HAVE_GTK=1.
int lufus_gui_run(int argc, char **argv);
#endif
