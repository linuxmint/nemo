
/*
#include <config.h>
#include "nemo-window.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-slot.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib/gstdio.h>
*/



static gboolean SESSION_MGR = 1;

static void sessionmgr_get_state(char *buf, gint size){
	NemoApplication *app;
  char *current_uri;
  NemoWindowSlot *slot;

	GList *window_list, *node;
	NemoWindow *window;
	gint n;
	/*
  GtkWidget *page;
	GList *l;
	GList *list_copy;
	NemoWindowSlot *force_no_close_slot;
	GFile *root, *computer;
	gchar *uri;
	gint n_slots;
	*/
		
  app = NEMO_APPLICATION (g_application_get_default ());
  window_list = gtk_application_get_windows (GTK_APPLICATION (app));
  
  /* Construct a list of windows to be closed. Do not add the non-closable windows to the list. */
	for (node = window_list; node != NULL; node = node->next) { // iterate windows
		window = NEMO_WINDOW (node->data);
		if (window != NULL && window_can_be_closed (window)) {
			GList *l;
			GList *lp;
      n=g_snprintf(buf, size,"w\n"); // new window
      if(size>n) {buf+=n; size-=n;} else return;
			for (lp = window->details->panes; lp != NULL; lp = lp->next) { // iterate panes
				NemoWindowPane *pane;
				pane = (NemoWindowPane*) lp->data;
				n=g_snprintf(buf,size,"p\n"); // new pane
				if(size>n) {buf+=n; size-=n;} else return;
				for (l = pane->slots; l != NULL; l = l->next) { // iterate slots
					slot = l->data;
					current_uri = nemo_window_slot_get_location_uri (slot);
					n=g_snprintf(buf,size,"%s\n", current_uri);
					if(size>n) {buf+=n; size-=n;} else return;
				} /* for all slots */
			} /* for all panes */
		}
	}
	
  // iterate over nemo windows
  /*
	list_copy = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (self)));
	for (l = list_copy; l != NULL; l = l->next) {
		NemoWindow *window;
	
		window = NEMO_WINDOW (l->data);
		window->details->panes
		
	}
	g_list_free (list_copy);

  page = gtk_notebook_get_nth_page (
				gnotebook, page_num+1);
  slot = NEMO_WINDOW_SLOT (page);
  current_uri = nemo_window_slot_get_location_uri (pane->active_slot);
  */
  
}


static void sessionmgr_setstate(char* c){
  // c must be a zero terminated string.
	NemoApplication *app;
	NemoWindow *window;
	NemoWindowPane *pane;
	NemoWindowSlot *slot;
  NemoWindowOpenFlags flags;
  gint i,j,end;
  char* uri;
  GFile *location;
  GList *window_list;
  
	app = NEMO_APPLICATION (g_application_get_default ());
	i=0;
	while(end==0){
	  // get up to date current window and slot
  	window_list = gtk_application_get_windows (GTK_APPLICATION (app));
  	window = NEMO_WINDOW (window_list->data);
    slot = nemo_window_get_active_slot (window);
    // next character
	  if(c[i]=='\0') break;
	  flags = NEMO_WINDOW_OPEN_FLAG_NEW_TAB; // default open in new tab
    if(c[i]=='w' && c[i+1]=='\n'){ // new window?
      i+=2;
     	flags = NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
    }
    if(c[i]=='p' && c[i+1]=='\n'){
      i+=2; // don't know how to switch to new pane yet...
    }
    j=i; // read one string 
    while (c[i]!='\0' && c[i]!='\n'){ 
      i+=1;
    }
    uri=c+j; c[i]='\0'; // make the URI a zero-terminated string
    if(uri[0]=='\0') break; // blank? problem!
    location = g_file_new_for_uri (uri); // create file object
    nemo_window_slot_open_location (slot, location,
							    flags, NULL);	     /// open location
		i+=1; // skip terminal zero
	}
		/*
  gtk_window_set_position (GTK_WINDOW (new_window), GTK_WIN_POS_MOUSE);
  flags = NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
  flags = NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
  nemo_window_slot_open_location (slot, location,
							    flags, NULL);
							    */
}

void sessionmgr_win_close(){
  if(SESSION_MGR) sessionmgr_save_state();
}
void sessionmgr_save_state(){ 
  char *currstate;
  gint size;
  size = 2048*sizeof(char);
  currstate = g_malloc0 ( size );
  sessionmgr_get_state(currstate,size);
  currstate[size-1]='\0'; // ensure the string is terminated if premature end
  if(currstate[0]=='\0' || currstate[1]=='\0') return;
  g_settings_set_string
  	(nemo_window_state, NEMO_HISTORY_STATE, currstate);
  printf("writing\n");
  printf(currstate);
  g_free(currstate);
}

void sessionmgr_load_state(){
  gchar *oldstate;
  printf("reading\n");
	oldstate = g_settings_get_string
  	(nemo_window_state, NEMO_HISTORY_STATE);
  if (oldstate!=NULL){
    printf(oldstate);
    sessionmgr_setstate(oldstate);
  }	else printf("null!\n");
}
