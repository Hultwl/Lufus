// GTK4 GUI scaffold for Lufus. Device list + ISO picker + log view.
// Full parity with Rufus ui.c comes in later milestones.
#include "gui_gtk.h"
#include "../linux/device.h"
#include "../linux/iso_probe.h"

#ifdef HAVE_GTK
#include <gtk/gtk.h>
#include <stdio.h>

static GtkWidget *log_view;
static void gui_log(const char *msg) {
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buf, &end);
  gtk_text_buffer_insert(buf, &end, msg, -1);
  gtk_text_buffer_insert(buf, &end, "\n", -1);
}

static void on_refresh(GtkButton *btn, gpointer list) {
  (void)btn;
  LufusDevice devs[64];
  int n = lufus_list_devices(devs, 64, 0);
  GtkStringList *sl = gtk_string_list_new(NULL);
  char tmp[256];
  for (int i = 0; i < n; i++) {
    snprintf(tmp, sizeof tmp, "%s  %s %s (%.1f GB)", devs[i].devnode,
             devs[i].vendor, devs[i].model, devs[i].size_bytes / 1073741824.0);
    gtk_string_list_append(sl, tmp);
  }
  gtk_drop_down_set_model(GTK_DROP_DOWN(list), G_LIST_MODEL(sl));
  snprintf(tmp, sizeof tmp, "Found %d removable device(s).", n);
  gui_log(tmp);
}

static void on_iso_response(GtkNativeDialog *d, int r, gpointer w) {
  (void)w;
  if (r == GTK_RESPONSE_ACCEPT) {
    GListModel *files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(d));
    GFile *gf = G_FILE(g_list_model_get_object(files, 0));
    char *p = gf ? g_file_get_path(gf) : NULL;
    if (p) {
      char label[64] = {0};
      int rc = lufus_probe_iso(p, label, sizeof label);
      char msg[512];
      snprintf(msg, sizeof msg, "ISO: %s  label='%s' rc=%d", p, rc == 0 ? label : "?", rc);
      gui_log(msg);
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

static void on_start(GtkButton *b, gpointer u) {
  (void)b; (void)u;
  gui_log("Scaffold: writing not enabled yet. See PORTING.md milestone 3.");
}

static void activate(GtkApplication *app, gpointer u) {
  (void)u;
  GtkWidget *win = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(win), "Lufus — USB Creator (Linux)");
  gtk_window_set_default_size(GTK_WINDOW(win), 560, 420);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top(box, 12); gtk_widget_set_margin_bottom(box, 12);
  gtk_widget_set_margin_start(box, 12); gtk_widget_set_margin_end(box, 12);
  gtk_window_set_child(GTK_WINDOW(win), box);

  GtkWidget *dev_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *drop = gtk_drop_down_new(NULL, NULL);
  gtk_widget_set_hexpand(drop, TRUE);
  GtkWidget *ref = gtk_button_new_with_label("Refresh");
  g_signal_connect(ref, "clicked", G_CALLBACK(on_refresh), drop);
  gtk_box_append(GTK_BOX(dev_row), drop);
  gtk_box_append(GTK_BOX(dev_row), ref);
  gtk_box_append(GTK_BOX(box), dev_row);

  GtkWidget *iso_btn = gtk_button_new_with_label("Select ISO / IMG…");
  g_signal_connect(iso_btn, "clicked", G_CALLBACK(on_iso), win);
  gtk_box_append(GTK_BOX(box), iso_btn);

  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scroll, TRUE);
  log_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), log_view);
  gtk_box_append(GTK_BOX(box), scroll);

  GtkWidget *start = gtk_button_new_with_label("Start (dry-run only in scaffold)");
  g_signal_connect(start, "clicked", G_CALLBACK(on_start), NULL);
  gtk_box_append(GTK_BOX(box), start);

  gtk_window_present(GTK_WINDOW(win));
  // populate once
  on_refresh(NULL, drop);
  gui_log("Lufus scaffold ready. Safe: no writes yet.");
}

int lufus_gui_run(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("io.github.hultwl.lufus", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int st = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return st;
}
#else
int lufus_gui_run(int argc, char **argv) {
  (void)argc; (void)argv;
  return 1;
}
#endif
