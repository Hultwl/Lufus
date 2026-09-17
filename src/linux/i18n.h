#ifndef RUFUX_I18N_H
#define RUFUX_I18N_H
// gettext wrapper. Identity macro when NLS is off so no extra dep.
#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#ifndef RUFUX_LOCALEDIR
#define RUFUX_LOCALEDIR "/usr/share/locale"
#endif
#define _(s) gettext(s)
static inline void rufux_i18n_init(void) {
  setlocale(LC_ALL, "");
  bindtextdomain("rufux", RUFUX_LOCALEDIR);
  textdomain("rufux");
}
#else
#define _(s) (s)
static inline void rufux_i18n_init(void) {}
#endif
#endif
