// Rufux GTK4 GUI — Rufus main-dialog clone (behavioral port of
// upstream Rufus IDD_DIALOG: drive properties, boot selection, image
// option, partition/target, format options, status, START/CLOSE, log).
#include "gui_gtk.h"
#include "../linux/device.h"
#include "../linux/iso_probe.h"
#include "../linux/checksum.h"
#include "../linux/create.h"
#include "../linux/secureboot.h"
#include "../linux/i18n.h"

#ifdef HAVE_GTK
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/stat.h>

// Invoking (non-root) user home: pkexec/sudo stash the uid for us.
static const char *invoking_home(void) {
  static char home[1024] = {0};
  if (home[0]) return home;
  const char *e = getenv("PKEXEC_UID");
  if (!e) e = getenv("SUDO_UID");
  if (e) {
    struct passwd *pw = getpwuid((uid_t)strtoul(e, NULL, 10));
    if (pw && pw->pw_dir && pw->pw_dir[0]) {
      snprintf(home, sizeof home, "%s", pw->pw_dir);
      return home;
    }
  }
  snprintf(home, sizeof home, "%s", g_get_home_dir());
  return home;
}

// gtk_dialog_run() is gone in GTK4: nested-loop modal helper.
typedef struct { GMainLoop *loop; int resp; } ModalCtx;
static void modal_response(GtkDialog *d, int r, gpointer u) {
  (void)d;
  ModalCtx *c = (ModalCtx *)u;
  c->resp = r;
  g_main_loop_quit(c->loop);
}
static int run_modal(GtkWindow *parent, GtkWidget *dlg) {
  ModalCtx c;
  c.loop = g_main_loop_new(NULL, FALSE);
  c.resp = GTK_RESPONSE_NONE;
  gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
  gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
  g_signal_connect(dlg, "response", G_CALLBACK(modal_response), &c);
  gtk_window_present(GTK_WINDOW(dlg));
  g_main_loop_run(c.loop);
  g_main_loop_unref(c.loop);
  gtk_window_destroy(GTK_WINDOW(dlg));
  return c.resp;
}

static GtkWidget *toplevel;
static GtkWidget *start_btn;
static GtkWidget *close_btn;
static GtkWidget *log_view;
static GtkWidget *log_box;
static GtkWidget *dev_drop;
static GtkWidget *boot_drop;
static GtkWidget *image_drop;
static GtkWidget *scheme_drop;
static GtkWidget *target_drop;
static GtkWidget *fs_drop;
static GtkWidget *cluster_drop;
static GtkWidget *passes_drop;
static GtkWidget *label_entry;
static GtkWidget *persist_spin;
static GtkWidget *persist_label;
static GtkWidget *progress;
static GtkWidget *status_label;
static GtkWidget *sum_label;
static GtkWidget *sb_label;
static GtkWidget *adv_drive_box;
static GtkWidget *adv_format_box;
static GtkWidget *check_hdd;
static GtkWidget *check_oldbios;
static GtkWidget *check_uefi;
static GtkWidget *check_quick;
static GtkWidget *check_extlabel;
static GtkWidget *check_badblocks;
static GtkWidget *select_btn;
static GtkWidget *hash_btn;
static int opt_dark = -1; // -1 system, 0 light, 1 dark
static int syncing = 0; // scheme<->target lock guard

static char sel_iso[1024] = {0};
static RufuxIsoInfo sel_info = {0};
static int has_iso = 0;
static RufuxDevice devs_cache[64];
static int devs_n = 0;

static void gui_log(const char *msg) {
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buf, &end);
  GDateTime *now = g_date_time_new_now_local();
  char *ts = g_date_time_format(now, "[%H:%M:%S] ");
  gtk_text_buffer_insert(buf, &end, ts ? ts : "", -1);
  g_free(ts);
  g_date_time_unref(now);
  gtk_text_buffer_insert(buf, &end, msg, -1);
  gtk_text_buffer_insert(buf, &end, "\n", -1);
}

static void gui_status(const char *msg) {
  gtk_label_set_text(GTK_LABEL(status_label), msg);
}

static GtkWidget *section(const char *title, GtkWidget *box) {
  GtkWidget *l = gtk_label_new(NULL);
  char m[128];
  snprintf(m, sizeof m, "<b>%s</b>", title);
  gtk_label_set_markup(GTK_LABEL(l), m);
  gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
  gtk_box_append(GTK_BOX(box), l);
  return l;
}

static GtkWidget *row_label(GtkWidget *box, const char *text) {
  GtkWidget *l = gtk_label_new(_(text));
  gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
  gtk_box_append(GTK_BOX(box), l);
  return l;
}

static const char *drop_text(GtkWidget *drop, const char *fallback) {
  GListModel *m = gtk_drop_down_get_model(GTK_DROP_DOWN(drop));
  guint s = gtk_drop_down_get_selected(GTK_DROP_DOWN(drop));
  if (!m || s == GTK_INVALID_LIST_POSITION) return fallback;
  GtkStringObject *o = GTK_STRING_OBJECT(g_list_model_get_object(m, s));
  return o ? gtk_string_object_get_string(o) : fallback;
}

// --- device scan (IDC_DEVICE) ---
static void on_refresh(GtkButton *btn, gpointer u) {
  (void)btn; (void)u;
  int include_fixed = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_hdd));
  devs_n = rufux_list_devices(devs_cache, 64, include_fixed);
  if (devs_n < 0) devs_n = 0;
  GtkStringList *sl = gtk_string_list_new(NULL);
  char tmp[300];
  for (int i = 0; i < devs_n; i++) {
    char hs[32];
    rufux_human_size(devs_cache[i].size_bytes, hs, sizeof hs);
    snprintf(tmp, sizeof tmp, "%s  %s %s (%s)%s", devs_cache[i].devnode,
             devs_cache[i].vendor, devs_cache[i].model, hs,
             devs_cache[i].mounted ? " [MOUNTED]" : "");
    gtk_string_list_append(sl, tmp);
  }
  if (devs_n == 0)
    gtk_string_list_append(sl, _("(no removable devices — insert USB)"));
  gtk_drop_down_set_model(GTK_DROP_DOWN(dev_drop), G_LIST_MODEL(sl));
  snprintf(tmp, sizeof tmp, "%d devices found", devs_n);
  gui_log(tmp);
  gui_status(_("READY"));
}

// --- boot selection (IDC_BOOT_SELECTION): Non bootable | Disk or ISO image ---
static int boot_is_iso(void) {
  const char *b = drop_text(boot_drop, "");
  return strstr(b, "ISO") != NULL;
}

// Rufus behavior: scheme and target track each other both ways.
static void on_scheme_changed(GtkDropDown *d, gpointer u) {
  (void)d; (void)u;
  if (syncing) return;
  syncing = 1;
  const char *s = drop_text(scheme_drop, "GPT");
  if (!strncmp(s, "GPT", 3))
    gtk_drop_down_set_selected(GTK_DROP_DOWN(target_drop), 2); // UEFI (non CSM)
  else
    gtk_drop_down_set_selected(GTK_DROP_DOWN(target_drop), 1); // BIOS (or UEFI-CSM)
  syncing = 0;
}

static void on_target_changed(GtkDropDown *d, gpointer u) {
  (void)d; (void)u;
  if (syncing) return;
  syncing = 1;
  guint t = gtk_drop_down_get_selected(GTK_DROP_DOWN(target_drop));
  if (t == 1)
    gtk_drop_down_set_selected(GTK_DROP_DOWN(scheme_drop), 1); // MBR
  else if (t == 2)
    gtk_drop_down_set_selected(GTK_DROP_DOWN(scheme_drop), 0); // GPT
  // "BIOS or UEFI" leaves the scheme alone
  syncing = 0;
}

static void update_sensitivities(void) {
  int iso = boot_is_iso();
  const char *img = drop_text(image_drop, "");
  int iso_mode = iso && !strncmp(img, "Write in ISO", 12);
  gtk_widget_set_sensitive(select_btn, iso);
  gtk_widget_set_sensitive(hash_btn, iso && has_iso);
  gtk_widget_set_sensitive(image_drop, iso);
  // Persistence only exists for ISO-image (file) installs, like casper-rw.
  gtk_widget_set_sensitive(persist_spin, iso_mode);
  gtk_widget_set_sensitive(persist_label, iso_mode);
}

static void on_boot_changed(GtkDropDown *d, gpointer u) {
  (void)d; (void)u;
  update_sensitivities();
}

static void on_image_changed(GtkDropDown *d, gpointer u) {
  (void)d; (void)u;
  update_sensitivities();
}

// Volume-label limits are per filesystem, not just FAT vs rest:
// vfat 11, exfat 15, ext2/3/4 16, ntfs/udf 32 (uppercased, like Rufus).
static size_t label_limit(const char *fs) {
  if (!strcmp(fs, "vfat") || !strcmp(fs, "fat32")) return 11;
  if (!strcmp(fs, "exfat")) return 15;
  if (!strcmp(fs, "ext4") || !strcmp(fs, "ext2") || !strcmp(fs, "ext3")) return 16;
  return 32;
}

static void sanitize_label(const char *in, const char *fs, char *out, size_t cap) {
  size_t n = 0;
  size_t max = label_limit(fs);
  for (size_t i = 0; in[i] && n + 1 < cap && n < max; i++) {
    char c = in[i];
    if (c == ' ') c = '_';
    if (isalnum((unsigned char)c) || c == '_' || c == '-') out[n++] = toupper((unsigned char)c);
  }
  out[n] = 0;
  if (!n) snprintf(out, cap, "RUFUX");
}

// --- SELECT (IDC_SELECT): pick image ---
static void select_finished(GObject *src, GAsyncResult *res, gpointer win) {
  (void)win;
  GtkFileDialog *dlg = GTK_FILE_DIALOG(src);
  GError *err = NULL;
  GFile *gf = gtk_file_dialog_open_finish(dlg, res, &err);
  if (!gf) {
    if (err && err->code != GTK_DIALOG_ERROR_DISMISSED)
      gui_log("File picker failed.");
    g_clear_error(&err);
    g_object_unref(dlg);
    return;
  }
  char *p = g_file_get_path(gf);
  if (p) {
      snprintf(sel_iso, sizeof sel_iso, "%s", p);
      char msg[1408];
      if (rufux_probe_iso_detail(p, &sel_info) == 0) {
        has_iso = 1;
        snprintf(msg, sizeof msg, "ISO: %s\n  label='%s' size=%.1f MB valid=%s boot=%s",
                 p, sel_info.label[0] ? sel_info.label : "(none)",
                 sel_info.size_bytes / 1048576.0,
                 sel_info.valid_iso ? "yes" : "no", sel_info.bootable ? "yes" : "no");
        gui_log(msg);
        // Rufus behavior: volume label defaults to the image label
        char lab[64];
        sanitize_label(sel_info.label[0] ? sel_info.label : "RUFUX", "vfat", lab, sizeof lab);
        gtk_editable_set_text(GTK_EDITABLE(label_entry), lab);
        // Default image mode: DD for bootable hybrids, ISO otherwise
        if (sel_info.bootable)
          gtk_drop_down_set_selected(GTK_DROP_DOWN(image_drop), 0);
        else
          gtk_drop_down_set_selected(GTK_DROP_DOWN(image_drop), 1);
        // Rufus behavior: EFI-capable ISO -> GPT/UEFI, else MBR
        // (scheme lock propagates to the target dropdown)
        if (sel_info.has_efi)
          gtk_drop_down_set_selected(GTK_DROP_DOWN(scheme_drop), 0);
        else if (sel_info.valid_iso)
          gtk_drop_down_set_selected(GTK_DROP_DOWN(scheme_drop), 1);
        if (sel_info.size_bytes < (256ull << 20)) {
          unsigned char sum[32];
          char err[256] = {0};
          if (rufux_sha256_file(p, sum, NULL, NULL, err, sizeof err) == 0) {
            char hex[65];
            rufux_hex32(sum, hex);
            char s[128];
            snprintf(s, sizeof s, "SHA-256: %.16s…", hex);
            gtk_label_set_text(GTK_LABEL(sum_label), s);
          }
        } else {
          gtk_label_set_text(GTK_LABEL(sum_label), _("SHA-256: (large file — use # button)"));
        }
        gtk_widget_set_sensitive(hash_btn, TRUE);
        update_sensitivities();
      } else {
        gui_log("Cannot probe selected file.");
      }
      g_free(p);
  }
  g_object_unref(gf);
  g_object_unref(dlg);
}

static void on_select(GtkButton *btn, gpointer win) {
  (void)btn;
  // GtkFileDialog is portal-native: opens the system file manager
  // (COSMIC Files, Dolphin, Nautilus) whenever the session bus is up.
  GtkFileDialog *dlg = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dlg, "Select image");
  GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
  GtkFileFilter *f = gtk_file_filter_new();
  gtk_file_filter_set_name(f, "Disk images (iso, img, vhd)");
  gtk_file_filter_add_pattern(f, "*.iso");
  gtk_file_filter_add_pattern(f, "*.img");
  gtk_file_filter_add_pattern(f, "*.vhd");
  g_list_store_append(filters, f);
  GtkFileFilter *all = gtk_file_filter_new();
  gtk_file_filter_set_name(all, "All files");
  gtk_file_filter_add_pattern(all, "*");
  g_list_store_append(filters, all);
  gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(filters));
  gtk_file_dialog_set_default_filter(dlg, f);
  g_object_unref(filters);
  // Start where the images live: the invoking user's Downloads.
  char dl[1152];
  snprintf(dl, sizeof dl, "%s/Downloads", invoking_home());
  if (access(dl, R_OK | X_OK) == 0) {
    GFile *dir = g_file_new_for_path(dl);
    gtk_file_dialog_set_initial_folder(dlg, dir);
    g_object_unref(dir);
  }
  gtk_file_dialog_open(dlg, GTK_WINDOW(win), NULL, select_finished, win);
}

// --- checksum (IDC_HASH): SHA-256 dialog ---
static void on_hash(GtkButton *btn, gpointer win) {
  (void)btn;
  if (!has_iso) return;
  // Rufus-style checksum dialog: all four hashes, computed in order.
  static const RufuxHashAlg algs[] = {RUFUX_MD5, RUFUX_SHA1, RUFUX_SHA256, RUFUX_SHA512};
  char body[1024] = {0};
  size_t off = 0;
  for (unsigned i = 0; i < 4; i++) {
    char msg[128];
    snprintf(msg, sizeof msg, "Computing %s...", rufux_alg_name(algs[i]));
    gui_log(msg);
    while (g_main_context_iteration(NULL, FALSE)) {}
    unsigned char sum[64] = {0};
    unsigned len = 0;
    char err[256] = {0};
    if (rufux_hash_file(sel_iso, algs[i], sum, &len, NULL, NULL, err, sizeof err) != 0) {
      GtkWidget *e = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Checksum failed: %s", err);
      g_signal_connect(e, "response", G_CALLBACK(gtk_window_destroy), NULL);
      gtk_window_present(GTK_WINDOW(e));
      return;
    }
    char hex[129];
    rufux_hex(sum, len, hex);
    off += (size_t)snprintf(body + off, sizeof body - off, "%s:\n%s\n\n",
                            rufux_alg_name(algs[i]), hex);
    if (off >= sizeof body - 1) break;
  }
  gui_log(body);
  GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", body);
  g_signal_connect(dlg, "response", G_CALLBACK(gtk_window_destroy), NULL);
  gtk_window_present(GTK_WINDOW(dlg));
}

// --- advanced toggles (IDC_ADVANCED_*) ---
static void on_adv_drive(GtkCheckButton *b, gpointer u) {
  (void)u;
  gtk_widget_set_visible(adv_drive_box, gtk_check_button_get_active(b));
}

static void on_adv_format(GtkCheckButton *b, gpointer u) {
  (void)u;
  gtk_widget_set_visible(adv_format_box, gtk_check_button_get_active(b));
}

static void on_hdd_toggled(GtkCheckButton *b, gpointer u) {
  (void)b; (void)u;
  on_refresh(NULL, NULL); // rescan with/without fixed disks
}

static int parse_cluster_sectors(const char *s) {  unsigned v = 0;
  if (!s || strstr(s, "Default")) return 0;
  if (sscanf(s, "%u", &v) != 1 || v < 512) return 0;
  return (int)(v / 512);
}

// Non-blocking stream drain into a line accumulator. For worker stderr
// (is_err) the last % in each chunk drives the bar + status; complete
// \n lines are logged except pure progress lines. Never blocks the UI.
static void drain_stream(GInputStream *s, char *acc, size_t *len, size_t cap, int is_err) {
  if (!G_IS_POLLABLE_INPUT_STREAM(s)) return;
  char buf[4096];
  GError *e = NULL;
  gssize n = g_pollable_input_stream_read_nonblocking(G_POLLABLE_INPUT_STREAM(s),
                                                      buf, sizeof buf - 1, NULL, &e);
  if (n <= 0) { g_clear_error(&e); return; }
  buf[n] = 0;
  if (is_err) {
    char *pct = NULL, *q = buf;
    while ((q = strchr(q, '%')) != NULL) { pct = q; q++; }
    if (pct) {
      int p = 0;
      char *st = pct - 1;
      while (st >= buf && *st != '\r' && *st != '\n') st--;
      if (sscanf(st + 1, "%d%%", &p) == 1 && p >= 0 && p <= 100) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), p / 100.0);
        char msg[64];
        snprintf(msg, sizeof msg, "Working… %d%%", p);
        gui_status(msg);
      }
    }
  }
  if (*len + (size_t)n >= cap) return; // accumulator full: drop (bounded)
  memcpy(acc + *len, buf, (size_t)n);
  *len += (size_t)n;
  acc[*len] = 0;
  char *line = acc, *nl;
  while ((nl = strchr(line, '\n')) != NULL) {
    *nl = 0;
    char *t = line;
    while (*t == '\r' || *t == ' ') t++;
    if (t[0] && !strchr(t, '%')) gui_log(t);
    line = nl + 1;
  }
  size_t rest = *len - (size_t)(line - acc);
  memmove(acc, line, rest);
  *len = rest;
  acc[*len] = 0;
}

// Log a trailing fragment that never got its newline (true EOF only).
static void flush_tail(char *acc, size_t *len) {
  char *t = acc;
  while (*t == '\r' || *t == ' ') t++;
  if (t[0] && !strchr(t, '%')) gui_log(t);
  *len = 0;
  acc[0] = 0;
}

// --- START (IDC_START), Rufus MSG_003 warning included ---
static void on_start(GtkButton *b, gpointer win) {
  (void)b;
  if (devs_n == 0) { gui_log(_("No removable device. Insert USB and Refresh.")); return; }
  guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(dev_drop));
  if (sel >= (guint)devs_n) { gui_log(_("Select a device first.")); return; }
  const char *dst = devs_cache[sel].devnode;
  int iso_mode = boot_is_iso();
  if (iso_mode && !has_iso) { gui_log(_("Select an ISO first.")); return; }

  const char *scheme_s = drop_text(scheme_drop, "GPT");
  const char *target_s = drop_text(target_drop, "BIOS or UEFI");
  const char *fs_s = drop_text(fs_drop, "FAT32");
  const char *img_s = drop_text(image_drop, "Write in DD Image mode");
  // Rufus behavior: UEFI (non CSM) forces GPT
  char scheme[16] = {0};
  if (!strncmp(scheme_s, "GPT", 3)) snprintf(scheme, sizeof scheme, "gpt");
  else snprintf(scheme, sizeof scheme, "dos");
  if (strstr(target_s, "UEFI (non CSM)") && !strcmp(scheme, "dos")) {
    snprintf(scheme, sizeof scheme, "gpt");
    gtk_drop_down_set_selected(GTK_DROP_DOWN(scheme_drop), 0);
    gui_log("Target is UEFI (non CSM): partition scheme forced to GPT.");
  }
  char fs[16] = {0};
  if (!strcmp(fs_s, "FAT32")) snprintf(fs, sizeof fs, "vfat");
  else if (!strcmp(fs_s, "NTFS")) snprintf(fs, sizeof fs, "ntfs");
  else if (!strcmp(fs_s, "exFAT")) snprintf(fs, sizeof fs, "exfat");
  else if (!strcmp(fs_s, "UDF")) snprintf(fs, sizeof fs, "udf");
  else snprintf(fs, sizeof fs, "ext4");
  char label[64];
  const char *entry = gtk_editable_get_text(GTK_EDITABLE(label_entry));
  sanitize_label(entry[0] ? entry : "RUFUX", fs, label, sizeof label);

  RufuxCreateOpts o;
  rufux_create_defaults(&o);
  o.scheme = scheme;
  o.fs = fs;
  o.label = label;
  o.persist_mb = (unsigned long)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(persist_spin));
  o.cluster_sectors = parse_cluster_sectors(drop_text(cluster_drop, "Default"));
  o.quick_format = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_quick));
  o.extended_label = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_extlabel));
  o.uefi_validate = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_uefi));
  o.badblock_passes = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_badblocks))
      ? (int)gtk_drop_down_get_selected(GTK_DROP_DOWN(passes_drop)) + 1 : 0;
  o.allow_fixed = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_hdd));
  o.verify = 1;
  o.dry_run = 0;
  o.yes = 1;
  if (!iso_mode) {
    o.mode = "format";
  } else if (!strncmp(img_s, "Write in ISO", 12)) {
    o.mode = "extract";
  } else {
    o.mode = "dd";
  }
  const char *src = iso_mode ? sel_iso : NULL;

  // MSG_003: the Rufus point-of-no-return warning
  char warn[1152];
  snprintf(warn, sizeof warn,
           "WARNING: ALL DATA ON DEVICE '%s' WILL BE DESTROYED.\n"
           "To continue with this operation, click OK. To quit click CANCEL.", dst);
  GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
      GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL, "%s", warn);
  int answer = run_modal(GTK_WINDOW(win), dlg);
  if (answer != GTK_RESPONSE_OK) {
    gui_log("Cancelled.");
    gui_status(_("READY"));
    return;
  }

  gui_status("Working...");
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.0);

  // Escalate per-operation, not per-app: spawn pkexec <self> create ...
  // The GUI stays on the user's session (theme, portals, display all
  // intact); only the display-free CLI worker runs as root. pkexec shows
  // the normal desktop password prompt; no terminal needed.
  char exe[1024] = {0};
  const char *self = NULL;
  const char *ai = getenv("APPIMAGE");
  if (ai && ai[0] && access(ai, X_OK) == 0) {
    snprintf(exe, sizeof exe, "%s", ai);
    self = exe;
  } else {
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    self = (n > 0) ? exe : "rufux";
  }
  char persist_s[32], cluster_s[32], passes_s[32];
  snprintf(persist_s, sizeof persist_s, "%lu", o.persist_mb);
  snprintf(cluster_s, sizeof cluster_s, "%d", o.cluster_sectors);
  snprintf(passes_s, sizeof passes_s, "%d", o.badblock_passes);
  const char *args[40];
  int k = 0;
  args[k++] = "pkexec";
  args[k++] = (char *)self;
  args[k++] = "create";
  args[k++] = iso_mode ? sel_iso : "none";
  args[k++] = (char *)dst;
  args[k++] = "--mode"; args[k++] = (char *)o.mode;
  args[k++] = "--scheme"; args[k++] = (char *)o.scheme;
  args[k++] = "--fs"; args[k++] = (char *)o.fs;
  args[k++] = "--label"; args[k++] = label;
  args[k++] = "--persist-mb"; args[k++] = persist_s;
  args[k++] = "--cluster-sectors"; args[k++] = cluster_s;
  args[k++] = "--badblock-passes"; args[k++] = passes_s;
  args[k++] = o.quick_format ? "--quick" : "--full";
  if (!o.extended_label) args[k++] = "--no-autorun";
  if (o.uefi_validate) args[k++] = "--uefi-validate";
  if (o.verify) args[k++] = "--verify";
  if (o.allow_fixed) args[k++] = "--allow-fixed";
  args[k++] = "--real";
  args[k++] = "--yes";
  args[k] = NULL;

  GError *gerr = NULL;
  GSubprocess *proc = g_subprocess_newv(args,
      G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &gerr);
  if (!proc) {
    char m[1152];
    snprintf(m, sizeof m, "Failed to launch helper: %s", gerr ? gerr->message : "?");
    g_clear_error(&gerr);
    gui_log(m);
    gui_status("Failed");
    gtk_widget_set_sensitive(start_btn, TRUE);
    gtk_widget_set_sensitive(close_btn, TRUE);
    return;
  }
  // No double burns, no closing mid-write (Rufus locks its buttons too).
  gtk_widget_set_sensitive(start_btn, FALSE);
  gtk_widget_set_sensitive(close_btn, FALSE);
  // Stream worker output without ever blocking the UI loop below:
  // both pipes are drained non-blocking; complete lines go to the log.
  GInputStream *outs = g_subprocess_get_stdout_pipe(proc);
  GInputStream *errs = g_subprocess_get_stderr_pipe(proc);
  char out_acc[65536] = {0};
  size_t out_len = 0;
  char err_acc[8192] = {0};
  size_t err_len = 0;
  gboolean done = FALSE;
  // Worker stderr carries both \r progress and real error text (pkexec
  // auth failures, refusal reasons). Forward completed text lines to
  // the log so failures are never silent; parse % for the bar.
  while (!done) {
    drain_stream(outs, out_acc, &out_len, sizeof out_acc, 0);
    drain_stream(errs, err_acc, &err_len, sizeof err_acc, 1);
    if (g_subprocess_get_if_exited(proc)) {
      // Final drain until both pipes are quiet, then flush tails.
      for (int i = 0; i < 40; i++) {
        size_t before = out_len + err_len;
        drain_stream(outs, out_acc, &out_len, sizeof out_acc, 0);
        drain_stream(errs, err_acc, &err_len, sizeof err_acc, 1);
        if (out_len + err_len == before) break;
        g_usleep(20000);
      }
      flush_tail(out_acc, &out_len);
      flush_tail(err_acc, &err_len);
      done = TRUE;
    } else {
      while (g_main_context_iteration(NULL, FALSE)) {}
      g_usleep(50000);
    }
  }
  gboolean ok = g_subprocess_get_successful(proc);
  int code = -1;
  if (g_subprocess_get_if_exited(proc)) code = g_subprocess_get_exit_status(proc);
  g_object_unref(proc);
  gtk_widget_set_sensitive(start_btn, TRUE);
  gtk_widget_set_sensitive(close_btn, TRUE);
  if (!ok) {
    char m[256];
    snprintf(m, sizeof m, "Failed: worker exited with code %d (see log above).", code);
    gui_log(m);
    gui_status("Failed");
    GtkWidget *e = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Failed. See the log for details.");
    g_signal_connect(e, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(e));
    return;
  }
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 1.0);
  gui_status(_("READY"));
}

static void on_close(GtkButton *b, gpointer u) {
  (void)b;
  GApplication *app = G_APPLICATION(u);
  g_application_quit(app);
}

static void on_log_toggle(GtkToggleButton *b, gpointer u) {
  (void)u;
  gtk_widget_set_visible(log_box, gtk_toggle_button_get_active(b));
}

static void on_log_clear(GtkButton *b, gpointer u) {
  (void)b; (void)u;
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
  gtk_text_buffer_set_text(buf, "", -1);
}

static void save_finished(GObject *src, GAsyncResult *res, gpointer w) {
  (void)w;
  GtkFileDialog *dlg = GTK_FILE_DIALOG(src);
  GError *err = NULL;
  GFile *gf = gtk_file_dialog_save_finish(dlg, res, &err);
  if (!gf) {
    if (err && err->code != GTK_DIALOG_ERROR_DISMISSED)
      gui_log("Save failed.");
    g_clear_error(&err);
    g_object_unref(dlg);
    return;
  }
  char *p = g_file_get_path(gf);
  if (p) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
    GtkTextIter a, z;
    gtk_text_buffer_get_bounds(buf, &a, &z);
    char *txt = gtk_text_buffer_get_text(buf, &a, &z, FALSE);
    FILE *f = fopen(p, "w");
    if (f) { fputs(txt, f); fclose(f); gui_log("Log saved."); }
    else gui_log("Cannot save log.");
    g_free(txt);
    g_free(p);
  }
  g_object_unref(gf);
  g_object_unref(dlg);
}

static void on_log_save(GtkButton *b, gpointer win) {
  (void)b;
  GtkFileDialog *dlg = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dlg, "Save log");
  gtk_file_dialog_set_initial_name(dlg, "rufux.log");
  gtk_file_dialog_save(dlg, GTK_WINDOW(win), NULL, save_finished, win);
}

static GtkWidget *hrow(GtkWidget *box) {
  GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append(GTK_BOX(box), r);
  return r;
}

// Follow the desktop theme: ask the xdg Settings portal for the system
// color-scheme (1 = dark). Falls back to the GTK settings.ini, then to
// dark on dark-first desktops (COSMIC) that expose neither.
static void apply_system_theme(void) {
  if (opt_dark >= 0) return; // explicit --theme wins
  GtkSettings *st = gtk_settings_get_default();
  if (!st) return;
  GError *e = NULL;
  GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
  if (bus) {
    // Modern namespace first, legacy draft second (COSMIC answers there).
    static const char *ns[] = {"org.freedesktop.desktop.interface",
                               "org.freedesktop.appearance", NULL};
    for (int i = 0; ns[i]; i++) {
      GVariant *ret = g_dbus_connection_call_sync(
          bus, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
          "org.freedesktop.portal.Settings", "Read",
          g_variant_new("(ss)", ns[i], "color-scheme"),
          G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &e);
      if (!ret) { g_clear_error(&e); continue; }
      GVariant *inner = NULL;
      g_variant_get(ret, "(v)", &inner);
      // The payload may itself be a variant wrapping the uint32.
      GVariant *val = inner;
      GVariant *unwrapped = NULL;
      if (val && g_variant_is_of_type(val, G_VARIANT_TYPE_VARIANT)) {
        unwrapped = g_variant_get_variant(val);
        val = unwrapped;
      }
      if (val && g_variant_is_of_type(val, G_VARIANT_TYPE_UINT32)) {
        guint32 scheme = g_variant_get_uint32(val);
        g_object_set(st, "gtk-application-prefer-dark-theme",
                     (scheme == 1 || scheme == 2) ? TRUE : FALSE, NULL);
        if (unwrapped) g_variant_unref(unwrapped);
        g_variant_unref(inner);
        g_variant_unref(ret);
        g_object_unref(bus);
        return; // portal answered: done
      }
      if (unwrapped) g_variant_unref(unwrapped);
      if (inner) g_variant_unref(inner);
      g_variant_unref(ret);
    }
    g_object_unref(bus);
  }
  // No portal answer: GTK config files, then COSMIC default.
  char path[1152];
  snprintf(path, sizeof path, "%s/.config/gtk-4.0/settings.ini", invoking_home());
  GKeyFile *kf = g_key_file_new();
  if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
    snprintf(path, sizeof path, "%s/.config/gtk-3.0/settings.ini", invoking_home());
    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
      g_key_file_free(kf);
      snprintf(path, sizeof path, "%s/.config/cosmic", invoking_home());
      if (access(path, R_OK | X_OK) == 0)
        g_object_set(st, "gtk-application-prefer-dark-theme", TRUE, NULL);
      return;
    }
  }
  char *t = NULL;
  if (g_key_file_has_key(kf, "Settings", "gtk-theme-name", NULL))
    t = g_key_file_get_string(kf, "Settings", "gtk-theme-name", NULL);
  if (t && t[0]) {
    g_object_set(st, "gtk-theme-name", t, NULL);
    if (strstr(t, "dark") || strstr(t, "Dark"))
      g_object_set(st, "gtk-application-prefer-dark-theme", TRUE, NULL);
  }
  g_free(t);
  g_key_file_free(kf);
}

static void activate(GtkApplication *app, gpointer u) {  (void)u;
  toplevel = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(toplevel), _("Rufux — USB Creator (Linux)"));
  gtk_window_set_default_size(GTK_WINDOW(toplevel), 520, 720);
  // The GUI always runs as the invoking user now (privilege lives in the
  // pkexec'd worker), so the desktop theme/settings apply naturally.
  // --theme remains as an explicit override.
  if (opt_dark >= 0) {
    GtkSettings *st = gtk_settings_get_default();
    if (st) g_object_set(st, "gtk-application-prefer-dark-theme", opt_dark ? TRUE : FALSE, NULL);
  } else {
    apply_system_theme();
  }
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_top(box, 10); gtk_widget_set_margin_bottom(box, 10);
  gtk_widget_set_margin_start(box, 10); gtk_widget_set_margin_end(box, 10);
  gtk_window_set_child(GTK_WINDOW(toplevel), box);

  // ---- Drive Properties ----
  section("Drive Properties", box);
  row_label(box, "Device");
  {
    GtkWidget *r = hrow(box);
    dev_drop = gtk_drop_down_new(NULL, NULL);
    gtk_widget_set_hexpand(dev_drop, TRUE);
    GtkWidget *ref = gtk_button_new_with_label(_("Refresh"));
    g_signal_connect(ref, "clicked", G_CALLBACK(on_refresh), NULL);
    gtk_box_append(GTK_BOX(r), dev_drop);
    gtk_box_append(GTK_BOX(r), ref);
  }
  row_label(box, "Boot selection");
  {
    GtkWidget *r = hrow(box);
    const char *opts[] = {"Disk or ISO image (Please select)", "Non bootable", "Disk or ISO image", NULL};
    boot_drop = gtk_drop_down_new_from_strings(opts);
    gtk_widget_set_hexpand(boot_drop, TRUE);
    g_signal_connect(boot_drop, "notify::selected", G_CALLBACK(on_boot_changed), NULL);
    select_btn = gtk_button_new_with_label("SELECT");
    g_signal_connect(select_btn, "clicked", G_CALLBACK(on_select), toplevel);
    hash_btn = gtk_button_new_with_label("#");
    gtk_widget_set_sensitive(hash_btn, FALSE);
    g_signal_connect(hash_btn, "clicked", G_CALLBACK(on_hash), toplevel);
    gtk_box_append(GTK_BOX(r), boot_drop);
    gtk_box_append(GTK_BOX(r), select_btn);
    gtk_box_append(GTK_BOX(r), hash_btn);
  }
  row_label(box, "Image option");
  {
    GtkWidget *r = hrow(box);
    const char *opts[] = {"Write in DD Image mode", "Write in ISO Image mode", NULL};
    image_drop = gtk_drop_down_new_from_strings(opts);
    gtk_widget_set_hexpand(image_drop, TRUE);
    g_signal_connect(image_drop, "notify::selected", G_CALLBACK(on_image_changed), NULL);
    gtk_box_append(GTK_BOX(r), image_drop);
    persist_label = gtk_label_new("Persistence (MB, 0 = off):");
    gtk_widget_set_tooltip_text(persist_label,
        "Extra ext4 casper-rw partition for Ubuntu-like live USBs.\n"
        "Only used in ISO Image mode. 0 means no persistence.");
    persist_spin = gtk_spin_button_new_with_range(0, 16384, 256);
    gtk_widget_set_tooltip_text(persist_spin,
        "Extra ext4 casper-rw partition for Ubuntu-like live USBs.\n"
        "Only used in ISO Image mode. 0 means no persistence.");
    gtk_box_append(GTK_BOX(r), persist_label);
    gtk_box_append(GTK_BOX(r), persist_spin);
  }
  {
    GtkWidget *r = hrow(box);
    GtkWidget *bl = gtk_label_new("Partition scheme");
    gtk_widget_set_hexpand(bl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(bl), 0.0f);
    GtkWidget *tl = gtk_label_new("Target system");
    gtk_widget_set_hexpand(tl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(tl), 0.0f);
    gtk_box_append(GTK_BOX(r), bl);
    gtk_box_append(GTK_BOX(r), tl);
  }
  {
    GtkWidget *r = hrow(box);
    const char *ps[] = {"GPT", "MBR", NULL};
    scheme_drop = gtk_drop_down_new_from_strings(ps);
    gtk_widget_set_hexpand(scheme_drop, TRUE);
    g_signal_connect(scheme_drop, "notify::selected", G_CALLBACK(on_scheme_changed), NULL);
    const char *ts[] = {"BIOS or UEFI", "BIOS (or UEFI-CSM)", "UEFI (non CSM)", NULL};
    target_drop = gtk_drop_down_new_from_strings(ts);
    gtk_widget_set_hexpand(target_drop, TRUE);
    g_signal_connect(target_drop, "notify::selected", G_CALLBACK(on_target_changed), NULL);
    gtk_box_append(GTK_BOX(r), scheme_drop);
    gtk_box_append(GTK_BOX(r), target_drop);
  }
  {
    GtkWidget *adv = gtk_check_button_new_with_label("Show advanced drive properties");
    g_signal_connect(adv, "toggled", G_CALLBACK(on_adv_drive), NULL);
    gtk_box_append(GTK_BOX(box), adv);
    adv_drive_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_visible(adv_drive_box, FALSE);
    check_hdd = gtk_check_button_new_with_label("List USB Hard Drives");
    g_signal_connect(check_hdd, "toggled", G_CALLBACK(on_hdd_toggled), NULL);
    check_oldbios = gtk_check_button_new_with_label("Add fixes for old BIOSes (extra partition, align, etc.)");
    check_uefi = gtk_check_button_new_with_label("Enable runtime UEFI media validation");
    gtk_box_append(GTK_BOX(adv_drive_box), check_hdd);
    gtk_box_append(GTK_BOX(adv_drive_box), check_oldbios);
    gtk_box_append(GTK_BOX(adv_drive_box), check_uefi);
    gtk_box_append(GTK_BOX(box), adv_drive_box);
  }

  // ---- Format Options ----
  section("Format Options", box);
  row_label(box, "Volume label");
  label_entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(label_entry), "RUFUX");
  gtk_entry_set_max_length(GTK_ENTRY(label_entry), 32);
  gtk_box_append(GTK_BOX(box), label_entry);
  {
    GtkWidget *r = hrow(box);
    GtkWidget *fl = gtk_label_new("File system");
    gtk_widget_set_hexpand(fl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(fl), 0.0f);
    GtkWidget *cl = gtk_label_new("Cluster size");
    gtk_widget_set_hexpand(cl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(cl), 0.0f);
    gtk_box_append(GTK_BOX(r), fl);
    gtk_box_append(GTK_BOX(r), cl);
  }
  {
    GtkWidget *r = hrow(box);
    const char *fss[] = {"FAT32", "NTFS", "exFAT", "UDF", "ext4", NULL};
    fs_drop = gtk_drop_down_new_from_strings(fss);
    gtk_widget_set_hexpand(fs_drop, TRUE);
    const char *cs[] = {"Default", "512 bytes", "1024 bytes", "2048 bytes",
                        "4096 bytes", "8192 bytes", "16 kilobytes",
                        "32 kilobytes", "64 kilobytes", NULL};
    cluster_drop = gtk_drop_down_new_from_strings(cs);
    gtk_widget_set_hexpand(cluster_drop, TRUE);
    gtk_box_append(GTK_BOX(r), fs_drop);
    gtk_box_append(GTK_BOX(r), cluster_drop);
  }
  {
    GtkWidget *adv = gtk_check_button_new_with_label("Show advanced format options");
    g_signal_connect(adv, "toggled", G_CALLBACK(on_adv_format), NULL);
    gtk_box_append(GTK_BOX(box), adv);
    adv_format_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_visible(adv_format_box, FALSE);
    check_quick = gtk_check_button_new_with_label("Quick format");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(check_quick), TRUE);
    check_extlabel = gtk_check_button_new_with_label("Create extended label and icon files");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(check_extlabel), TRUE);
    GtkWidget *r = hrow(adv_format_box);
    check_badblocks = gtk_check_button_new_with_label("Check device for bad blocks");
    const char *np[] = {"1 pass", "2 passes", "3 passes", "4 passes", NULL};
    passes_drop = gtk_drop_down_new_from_strings(np);
    gtk_box_append(GTK_BOX(r), check_badblocks);
    gtk_box_append(GTK_BOX(r), passes_drop);
    gtk_box_append(GTK_BOX(adv_format_box), check_quick);
    gtk_box_append(GTK_BOX(adv_format_box), check_extlabel);
    gtk_box_append(GTK_BOX(box), adv_format_box);
  }

  // ---- Status ----
  section("Status", box);
  sum_label = gtk_label_new(_("SHA-256: (no ISO)"));
  gtk_label_set_xalign(GTK_LABEL(sum_label), 0.0f);
  gtk_box_append(GTK_BOX(box), sum_label);
  {
    char sb_txt[128];
    snprintf(sb_txt, sizeof sb_txt, "Secure Boot: %s", rufux_sb_string(rufux_sb_state()));
    sb_label = gtk_label_new(sb_txt);
    gtk_label_set_xalign(GTK_LABEL(sb_label), 0.0f);
    gtk_box_append(GTK_BOX(box), sb_label);
  }
  status_label = gtk_label_new(_("READY"));
  gtk_label_set_xalign(GTK_LABEL(status_label), 0.0f);
  gtk_box_append(GTK_BOX(box), status_label);
  progress = gtk_progress_bar_new();
  gtk_box_append(GTK_BOX(box), progress);

  {
    GtkWidget *r = hrow(box);
    GtkWidget *logbtn = gtk_toggle_button_new_with_label("Log");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(logbtn), TRUE);
    g_signal_connect(logbtn, "toggled", G_CALLBACK(on_log_toggle), NULL);
    GtkWidget *start = gtk_button_new_with_label("START");
    start_btn = start;
    gtk_widget_set_hexpand(start, TRUE);
    g_signal_connect(start, "clicked", G_CALLBACK(on_start), toplevel);
    GtkWidget *close = gtk_button_new_with_label("CLOSE");
    close_btn = close;
    g_signal_connect(close, "clicked", G_CALLBACK(on_close), app);
    gtk_box_append(GTK_BOX(r), logbtn);
    gtk_box_append(GTK_BOX(r), start);
    gtk_box_append(GTK_BOX(r), close);
  }

  log_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scroll, TRUE);
  gtk_widget_set_size_request(scroll, -1, 140);
  log_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(log_view), TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), log_view);
  gtk_box_append(GTK_BOX(log_box), scroll);
  {
    GtkWidget *r = hrow(log_box);
    GtkWidget *sp = gtk_label_new(NULL);
    gtk_widget_set_hexpand(sp, TRUE);
    GtkWidget *clr = gtk_button_new_with_label("Clear");
    g_signal_connect(clr, "clicked", G_CALLBACK(on_log_clear), NULL);
    GtkWidget *sav = gtk_button_new_with_label("Save");
    g_signal_connect(sav, "clicked", G_CALLBACK(on_log_save), toplevel);
    gtk_box_append(GTK_BOX(r), sp);
    gtk_box_append(GTK_BOX(r), clr);
    gtk_box_append(GTK_BOX(r), sav);
  }
  gtk_box_append(GTK_BOX(box), log_box);

  gtk_window_present(GTK_WINDOW(toplevel));
  on_refresh(NULL, NULL);
  on_boot_changed(NULL, NULL);
  on_scheme_changed(NULL, NULL); // startup lock: GPT -> UEFI (non CSM)
  gui_log("Rufux ready. Select a device and an image, then press START.");
}

int rufux_gui_run(int argc, char **argv) {
  // The GUI always runs as the invoking user; privilege lives in the
  // pkexec'd CLI worker spawned by START. No self-escalation here.
  // strip our own options before GTK parses argv
  const char *theme = getenv("RUFUX_THEME");
  char *filtered[128];
  int nf = 0;
  filtered[nf++] = argv[0];
  for (int i = 1; i < argc && nf < 127; i++) {
    if (!strcmp(argv[i], "--gui") || !strcmp(argv[i], "gui")) continue;
    if (!strcmp(argv[i], "--theme") && i + 1 < argc) { theme = argv[++i]; continue; }
    if (!strncmp(argv[i], "--theme=", 8)) { theme = argv[i] + 8; continue; }
    filtered[nf++] = argv[i];
  }
  if (theme) {
    if (!strcmp(theme, "dark")) opt_dark = 1;
    else if (!strcmp(theme, "light")) opt_dark = 0;
  }
  rufux_i18n_init();
  GtkApplication *app = gtk_application_new("io.github.hultwl.rufux", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int st = g_application_run(G_APPLICATION(app), nf, filtered);
  g_object_unref(app);
  return st;
}
#else
int rufux_gui_run(int argc, char **argv) {
  (void)argc; (void)argv;
  return 1;
}
#endif
