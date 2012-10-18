#include <libnautilus-private/nautilus-search-provider.h>
#include <libnautilus-private/nautilus-search-engine.h>
#include <gtk/gtk.h>

static void
hits_added_cb (NautilusSearchEngine *engine, GSList *hits)
{      
	g_print ("hits added\n");
	while (hits) {
		g_print (" - %s\n", (char *)hits->data);
		hits = hits->next;
	}
}

static void
finished_cb (NautilusSearchEngine *engine)
{
	g_print ("finished!\n");
	gtk_main_quit ();
}

int 
main (int argc, char* argv[])
{
	NautilusSearchEngine *engine;
        NautilusSearchEngineModel *model;
        NautilusDirectory *directory;
	NautilusQuery *query;
        GFile *location;
	
	gtk_init (&argc, &argv);

	engine = nautilus_search_engine_new ();
	g_signal_connect (engine, "hits-added", 
			  G_CALLBACK (hits_added_cb), NULL);
	g_signal_connect (engine, "finished", 
			  G_CALLBACK (finished_cb), NULL);

	query = nautilus_query_new ();
	nautilus_query_set_text (query, "richard hult");
	nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (engine), query);
	g_object_unref (query);

        location = g_file_new_for_path (g_get_home_dir ());
        directory = nautilus_directory_get (location);
        g_object_unref (location);

        model = nautilus_search_engine_get_model_provider (engine);
        nautilus_search_engine_model_set_model (model, directory);
        g_object_unref (directory);

	nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine));
	nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine));
        g_object_unref (engine);

	gtk_main ();
	return 0;
}
