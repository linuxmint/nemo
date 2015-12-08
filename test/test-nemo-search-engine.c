#include <libnemo-private/nemo-search-provider.h>
#include <libnemo-private/nemo-search-engine.h>
#include <gtk/gtk.h>

static void
hits_added_cb (NemoSearchEngine *engine, GSList *hits)
{      
	g_print ("hits added\n");
	while (hits) {
		g_print (" - %s\n", (char *)hits->data);
		hits = hits->next;
	}
}

static void
finished_cb (NemoSearchEngine *engine)
{
	g_print ("finished!\n");
	gtk_main_quit ();
}

int 
main (int argc, char* argv[])
{
	NemoSearchEngine *engine;
        NemoSearchEngineModel *model;
        NemoDirectory *directory;
	NemoQuery *query;
        GFile *location;
	
	gtk_init (&argc, &argv);

	engine = nemo_search_engine_new ();
	g_signal_connect (engine, "hits-added", 
			  G_CALLBACK (hits_added_cb), NULL);
	g_signal_connect (engine, "finished", 
			  G_CALLBACK (finished_cb), NULL);

	query = nemo_query_new ();
	nemo_query_set_text (query, "richard hult");
	nemo_search_provider_set_query (NEMO_SEARCH_PROVIDER (engine), query);
	g_object_unref (query);

        location = g_file_new_for_path (g_get_home_dir ());
        directory = nemo_directory_get (location);
        g_object_unref (location);

        model = nemo_search_engine_get_model_provider (engine);
        nemo_search_engine_model_set_model (model, directory);
        g_object_unref (directory);

	nemo_search_provider_start (NEMO_SEARCH_PROVIDER (engine));
	nemo_search_provider_stop (NEMO_SEARCH_PROVIDER (engine));
        g_object_unref (engine);

	gtk_main ();
	return 0;
}
