#include "test.h"

#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-progress-info.h>
#include <libnemo-private/nemo-progress-info-manager.h>

static void
copy_done (GHashTable *debuting_uris, 
           gboolean success,
           gpointer data)
{
	g_print ("Copy done\n");
}

static void
changed_cb (NemoProgressInfo *info,
	    gpointer data)
{
	g_print ("Changed: %s -- %s\n",
		 nemo_progress_info_get_status (info),
		 nemo_progress_info_get_details (info));
}

static void
progress_changed_cb (NemoProgressInfo *info,
		     gpointer data)
{
	g_print ("Progress changed: %f\n",
		 nemo_progress_info_get_progress (info));
}

static void
finished_cb (NemoProgressInfo *info,
	     gpointer data)
{
	g_print ("Finished\n");
	gtk_main_quit ();
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GList *sources;
	GFile *dest;
	GFile *source;
	int i;
	GList *infos;
        NemoProgressInfoManager *manager;
	NemoProgressInfo *progress_info;
	
	test_init (&argc, &argv);

	if (argc < 3) {
		g_print ("Usage test-copy <sources...> <dest dir>\n");
		return 1;
	}

	sources = NULL;
	for (i = 1; i < argc - 1; i++) {
		source = g_file_new_for_commandline_arg (argv[i]);
		sources = g_list_prepend (sources, source);
	}
	sources = g_list_reverse (sources);
	
	dest = g_file_new_for_commandline_arg (argv[i]);
	
	window = test_window_new ("copy test", 5);
	
	gtk_widget_show (window);

        manager = nemo_progress_info_manager_new ();

	nemo_file_operations_copy (sources,
				       NULL /* GArray *relative_item_points */,
				       dest,
				       GTK_WINDOW (window),
				       copy_done, NULL);
        
	infos = nemo_progress_info_manager_get_all_infos (manager);

	if (infos == NULL) {
		g_object_unref (manager);
		return 0;
	}

	progress_info = NEMO_PROGRESS_INFO (infos->data);

	g_signal_connect (progress_info, "changed", (GCallback)changed_cb, NULL);
	g_signal_connect (progress_info, "progress-changed", (GCallback)progress_changed_cb, NULL);
	g_signal_connect (progress_info, "finished", (GCallback)finished_cb, NULL);
	
	gtk_main ();

        g_object_unref (manager);
	
	return 0;
}


