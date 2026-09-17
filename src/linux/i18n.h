#ifndef LUFUS_I18N_H
#define LUFUS_I18N_H
// gettext wrapper. Identity macro when NLS is off so no extra dep.
#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#ifndef LUFUS_LOCALEDIR
#define LUFUS_LOCALEDIR "/usr/share/locale"
#endif
#define _(s) gettext(s)
static inline void lufus_i18n_init(void) {
  setlocale(LC_ALL, "");
  bindtextdomain("lufus", LUFUS_LOCALEDIR);
  textdomain("lufus");
}
#else
#define _(s) (s)
static inline void lufus_i18n_init(void) {}
#endif
#endif
