#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>

static GtkWidget *editor;

static void on_execute(GtkButton *btn, gpointer data) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    char *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
    FILE *f = fopen("execute.lua", "w");
    if (f) { fputs(text, f); fclose(f); }
    g_free(text);
}

static void on_attach(GtkButton *btn, gpointer data) {
    system("./deltoid_launcher");
    gtk_button_set_label(btn, "attached ( success )");
}

static void activate(GtkApplication *app, gpointer data) {
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "Deltoid KX");
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 400);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_window_set_child(GTK_WINDOW(win), box);

    editor = gtk_text_view_new();
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), editor);
    gtk_box_append(GTK_BOX(box), scroll);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(box), btn_box);

    GtkWidget *b1 = gtk_button_new_with_label("exec");
    g_signal_connect(b1, "clicked", G_CALLBACK(on_execute), NULL);
    gtk_box_append(GTK_BOX(btn_box), b1);

    GtkWidget *b2 = gtk_button_new_with_label("attatch");
    g_signal_connect(b2, "clicked", G_CALLBACK(on_attach), NULL);
    gtk_box_append(GTK_BOX(btn_box), b2);

    gtk_widget_show(win);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.deltoid.kx", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
