#include <gtk/gtk.h>
#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-search-directory.h>
#include <libnemo-private/nemo-file.h>
#include <unistd.h>

void *client1, *client2;

static void
files_added (NemoDirectory *directory,
	     GList *added_files)
{
#if 0
	GList *list;

	for (list = added_files; list != NULL; list = list->next) {
		NemoFile *file = list->data;

		g_print (" - %s\n", nemo_file_get_uri (file));
	}
#endif

	g_print ("files added: %d files\n",
		 g_list_length (added_files));
}

static void
files_changed (NemoDirectory *directory,
	       GList *changed_files)
{
#if 0
	GList *list;

	for (list = changed_files; list != NULL; list = list->next) {
		NemoFile *file = list->data;

		g_print (" - %s\n", nemo_file_get_uri (file));
	}
#endif
	g_print ("files changed: %d\n",
		 g_list_length (changed_files));
}

static void
done_loading (NemoDirectory *directory)
{
	g_print ("done loading\n");
	gtk_main_quit ();
}

int
main (int argc, char **argv)
{
	NemoDirectory *directory;
	NemoFileAttributes attributes;
	const char *uri;

	client1 = g_new0 (int, 1);
	client2 = g_new0 (int, 1);

	gtk_init (&argc, &argv);

	if (argv[1] == NULL) {
		uri = "file:///tmp";
	} else {
		uri = argv[1];
	}
	g_print ("loading %s", uri);
	directory = nemo_directory_get_by_uri (uri);

	g_signal_connect (directory, "files-added", G_CALLBACK (files_added), NULL);
	g_signal_connect (directory, "files-changed", G_CALLBACK (files_changed), NULL);
	g_signal_connect (directory, "done-loading", G_CALLBACK (done_loading), NULL);

	attributes =
		NEMO_FILE_ATTRIBUTES_FOR_ICON |
		NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NEMO_FILE_ATTRIBUTE_INFO |
		NEMO_FILE_ATTRIBUTE_LINK_INFO |
		NEMO_FILE_ATTRIBUTE_MOUNT |
		NEMO_FILE_ATTRIBUTE_EXTENSION_INFO;

	nemo_directory_file_monitor_add (directory, client1, TRUE,
                                             attributes,
					     NULL, NULL);


	gtk_main ();
	return 0;
}
