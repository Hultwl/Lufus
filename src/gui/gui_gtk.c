// Lufus GTK4 GUI — Phase 3 stable.
#include "gui_gtk.h"
#include "../linux/device.h"
#include "../linux/iso_probe.h"
#include "../linux/checksum.h"
#include "../linux/writer.h"
#include "../linux/extract.h"
#include "../linux/secureboot.h"
#include "../linux/i18n.h"

#ifdef HAVE_GTK
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static GtkWidget *log_view;
static GtkWidget *dev_drop;
static GtkWidget *mode_drop;
static GtkWidget *scheme_drop;
static GtkWidget *fs_drop;
static GtkWidget *progress;
static GtkWidget *sum_label;
static GtkWidget *sb_label;
static int opt_dark = -1; // -1 system, 0 light, 1 dark
static char sel_iso[1024] = {0};
static LufusDevice devs_cache[64];
static int devs_n = 0;

static void gui_log(const char *msg) {
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buf, &end);
  gtk_text_buffer_insert(buf, &end, msg, -1);
  gtk_text_buffer_insert(buf, &end, "\n", -1);
}

static void on_refresh(GtkButton *btn, gpointer u) {
  (void)btn; (void)u;
  devs_n = lufus_list_devices(devs_cache, 64, 0);
  if (devs_n < 0) devs_n = 0;
  GtkStringList *sl = gtk_string_list_new(NULL);
  char tmp[256];
  for (int i = 0; i < devs_n; i++) {
    snprintf(tmp, sizeof tmp, "%s  %s %s (%.1f GB)%s", devs_cache[i].devnode,
             devs_cache[i].vendor, devs_cache[i].model,
             devs_cache[i].size_bytes / 1073741824.0,
             devs_cache[i].mounted ? " [MOUNTED]" : "");
    gtk_string_list_append(sl, tmp);
  }
  if (devs_n == 0)
    gtk_string_list_append(sl, _("(no removable devices — insert USB)"));
  gtk_drop_down_set_model(GTK_DROP_DOWN(dev_drop), G_LIST_MODEL(sl));
  snprintf(tmp, sizeof tmp, "Found %d removable device(s).", devs_n);
  gui_log(tmp);
}

static void on_iso_response(GtkNativeDialog *d, int r, gpointer w) {
  (void)w;
  if (r == GTK_RESPONSE_ACCEPT) {
    GListModel *files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(d));
    GFile *gf = G_FILE(g_list_model_get_object(files, 0));
    char *p = gf ? g_file_get_path(gf) : NULL;
    if (p) {
      snprintf(sel_iso, sizeof sel_iso, "%s", p);
      LufusIsoInfo info = {0};
      char msg[1024];
      if (lufus_probe_iso_detail(p, &info) == 0) {
        snprintf(msg, sizeof msg, "ISO: %s\n  label='%s' size=%.1f MB valid=%s boot=%s",
                 p, info.label[0] ? info.label : "(none)",
                 info.size_bytes / 1048576.0,
                 info.valid_iso ? "yes" : "no", info.bootable ? "yes" : "no");
        gui_log(msg);
        // quick checksum for small files only (<256MB) to avoid UI freeze
        if (info.size_bytes < (256ull << 20)) {
          unsigned char sum[32];
          char err[256] = {0};
          if (lufus_sha256_file(p, sum, NULL, NULL, err, sizeof err) == 0) {
            char hex[65];
            lufus_hex32(sum, hex);
            char s[128];
            snprintf(s, sizeof s, "SHA-256: %.16s…", hex);
            gtk_label_set_text(GTK_LABEL(sum_label), s);
            snprintf(msg, sizeof msg, "SHA-256: %s", hex);
            gui_log(msg);
          } else {
            gtk_label_set_text(GTK_LABEL(sum_label), "SHA-256: (error)");
          }
        } else {
          gtk_label_set_text(GTK_LABEL(sum_label), _("SHA-256: (large file — use CLI)"));
        }
      } else {
        gui_log("Cannot probe selected file.");
      }
      g_free(p);
    }
    if (gf) g_object_unref(gf);
    g_object_unref(files);
  }
  g_object_unref(d);
}

static void on_iso(GtkButton *btn, gpointer win) {
  (void)btn;
  GtkFileChooserNative *fc = gtk_file_chooser_native_new(
      "Select ISO", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
  GtkFileFilter *f = gtk_file_filter_new();
  gtk_file_filter_add_pattern(f, "*.iso");
  gtk_file_filter_add_pattern(f, "*.img");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), f);
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(fc));
  g_signal_connect(fc, "response", G_CALLBACK(on_iso_response), win);
}

static void write_progress_cb(unsigned long long done, unsigned long long total, void *u) {
  (void)u;
  if (total) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), (double)done / (double)total);
  while (g_main_context_iteration(NULL, FALSE)) {}
}

static const char *drop_text(GtkWidget *drop, const char *fallback) {
  GListModel *m = gtk_drop_down_get_model(GTK_DROP_DOWN(drop));
  guint s = gtk_drop_down_get_selected(GTK_DROP_DOWN(drop));
  if (!m || s == GTK_INVALID_LIST_POSITION) return fallback;
  GtkStringObject *o = GTK_STRING_OBJECT(g_list_model_get_object(m, s));
  return o ? gtk_string_object_get_string(o) : fallback;
}

static void on_start(GtkButton *b, gpointer u) {
  (void)b; (void)u;
  if (!sel_iso[0]) { gui_log(_("Select an ISO first.")); return; }
  if (devs_n == 0) { gui_log(_("No removable device. Insert USB and Refresh.")); return; }
  guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(dev_drop));
  if (sel >= (guint)devs_n) { gui_log(_("Select a device first.")); return; }
  const char *dst = devs_cache[sel].devnode;
  const char *mode = drop_text(mode_drop, "dd");
  const char *scheme = drop_text(scheme_drop, "gpt");
  const char *fs = drop_text(fs_drop, "vfat");
  // keep values short: dropdown shows "dd — direct image" etc.
  char mode_s[16] = {0}, scheme_s[16] = {0}, fs_s[16] = {0};
  sscanf(mode, "%15s", mode_s); sscanf(scheme, "%15s", scheme_s); sscanf(fs, "%15s", fs_s);
  char msg[1408];
  snprintf(msg, sizeof msg, "Plan: %s -> %s [mode=%s scheme=%s fs=%s] (dry-run, no writes)",
           sel_iso, dst, mode_s, scheme_s, fs_s);
  gui_log(msg);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.0);
  if (!strncmp(mode_s, "extract", 7)) {
    char err[512] = {0};
    // dry-run extract plan against a temp dir probe (no writes to USB)
    char tmp[] = "/tmp/lufus-gui-XXXXXX";
    if (!mkdtemp(tmp)) { gui_log("tmpdir failed"); return; }
    int rc = lufus_extract_iso(sel_iso, tmp, 1, err, sizeof err);
    rmdir(tmp);
    if (rc == 0) {
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 1.0);
      gui_log("Extract dry-run OK. Real: CLI 'create SRC MNT --mode extract --real'.");
    } else {
      snprintf(msg, sizeof msg, "Extract plan failed: %s", err);
      gui_log(msg);
    }
    return;
  }
  LufusWriteOpts o = {.dry_run = 1, .verify = 0};
  char err[512] = {0};
  int rc = lufus_write_image(sel_iso, dst, &o, write_progress_cb, NULL, err, sizeof err);
  if (rc != 0) {
    snprintf(msg, sizeof msg, "Target check: %s — simulating source read only.", err);
    gui_log(msg);
    unsigned char sum[32];
    char e2[256] = {0};
    if (lufus_sha256_file(sel_iso, sum, (LufusHashProgress)write_progress_cb, NULL, e2, sizeof e2) == 0) {
      gui_log("Dry-run read OK (source verified readable).");
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 1.0);
      return;
    }
    snprintf(msg, sizeof msg, "Dry-run failed: %s", err);
    gui_log(msg);
    return;
  }
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 1.0);
  gui_log("Dry-run OK — no bytes written. Real: CLI with --real --yes (see create).");
}

static void activate(GtkApplication *app, gpointer u) {
  (void)u;
  GtkWidget *win = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(win), _("Lufus — USB Creator (Linux)"));
  gtk_window_set_default_size(GTK_WINDOW(win), 600, 570);
  if (opt_dark >= 0) {
    GtkSettings *st = gtk_settings_get_default();
    if (st) g_object_set(st, "gtk-application-prefer-dark-theme", opt_dark ? TRUE : FALSE, NULL);
  }
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top(box, 12); gtk_widget_set_margin_bottom(box, 12);
  gtk_widget_set_margin_start(box, 12); gtk_widget_set_margin_end(box, 12);
  gtk_window_set_child(GTK_WINDOW(win), box);

  GtkWidget *dev_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  dev_drop = gtk_drop_down_new(NULL, NULL);
  gtk_widget_set_hexpand(dev_drop, TRUE);
  GtkWidget *ref = gtk_button_new_with_label(_("Refresh"));
  g_signal_connect(ref, "clicked", G_CALLBACK(on_refresh), NULL);
  gtk_box_append(GTK_BOX(dev_row), dev_drop);
  gtk_box_append(GTK_BOX(dev_row), ref);
  gtk_box_append(GTK_BOX(box), dev_row);

  GtkWidget *iso_btn = gtk_button_new_with_label(_("Select ISO / IMG…"));
  g_signal_connect(iso_btn, "clicked", G_CALLBACK(on_iso), win);
  gtk_box_append(GTK_BOX(box), iso_btn);

  sum_label = gtk_label_new(_("SHA-256: (no ISO)"));
  gtk_label_set_xalign(GTK_LABEL(sum_label), 0.0f);
  gtk_box_append(GTK_BOX(box), sum_label);

  char sb_txt[128];
  snprintf(sb_txt, sizeof sb_txt, "Secure Boot: %s", lufus_sb_string(lufus_sb_state()));
  sb_label = gtk_label_new(sb_txt);
  gtk_label_set_xalign(GTK_LABEL(sb_label), 0.0f);
  gtk_box_append(GTK_BOX(box), sb_label);

  GtkWidget *opt_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  const char *modes[] = {"dd — direct image", "extract — UEFI files", NULL};
  const char *schemes[] = {"gpt", "dos", NULL};
  const char *fss[] = {"vfat", "ntfs", "exfat", "ext4", NULL};
  mode_drop = gtk_drop_down_new_from_strings(modes);
  scheme_drop = gtk_drop_down_new_from_strings(schemes);
  fs_drop = gtk_drop_down_new_from_strings(fss);
  gtk_widget_set_hexpand(mode_drop, TRUE);
  gtk_box_append(GTK_BOX(opt_row), mode_drop);
  gtk_box_append(GTK_BOX(opt_row), scheme_drop);
  gtk_box_append(GTK_BOX(opt_row), fs_drop);
  gtk_box_append(GTK_BOX(box), opt_row);

  progress = gtk_progress_bar_new();
  gtk_box_append(GTK_BOX(box), progress);

  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scroll, TRUE);
  log_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), log_view);
  gtk_box_append(GTK_BOX(box), scroll);

  GtkWidget *start = gtk_button_new_with_label(_("Start (dry-run, safe)"));
  g_signal_connect(start, "clicked", G_CALLBACK(on_start), NULL);
  gtk_box_append(GTK_BOX(box), start);

  gtk_window_present(GTK_WINDOW(win));
  on_refresh(NULL, NULL);
  gui_log("Lufus stable ready. Real block flows: CLI 'create' with --real --yes.");
}

int lufus_gui_run(int argc, char **argv) {
  // strip our --theme option before GTK parses argv
  const char *theme = getenv("LUFUS_THEME");
  char *filtered[128];
  int nf = 0;
  filtered[nf++] = argv[0];
  for (int i = 1; i < argc && nf < 127; i++) {
    if (!strcmp(argv[i], "--theme") && i + 1 < argc) { theme = argv[++i]; continue; }
    if (!strncmp(argv[i], "--theme=", 8)) { theme = argv[i] + 8; continue; }
    filtered[nf++] = argv[i];
  }
  if (theme) {
    if (!strcmp(theme, "dark")) opt_dark = 1;
    else if (!strcmp(theme, "light")) opt_dark = 0;
  }
  lufus_i18n_init();
  GtkApplication *app = gtk_application_new("io.github.hultwl.lufus", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int st = g_application_run(G_APPLICATION(app), nf, filtered);
  g_object_unref(app);
  return st;
}
#else
int lufus_gui_run(int argc, char **argv) {
  (void)argc; (void)argv;
  return 1;
}
#endif
