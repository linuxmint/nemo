/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   Copyright (C) 2001 Maciej Stachowiak
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.

   Author: Maciej Stachowiak <mjs@noisehavoc.org>
*/

#include <config.h>
#include "nemo-file-queue.h"

#include <glib.h>

struct NemoFileQueue {
	GList *head;
	GList *tail;
	GHashTable *item_to_link_map;
};

NemoFileQueue *
nemo_file_queue_new (void)
{
	NemoFileQueue *queue;
	
	queue = g_new0 (NemoFileQueue, 1);
	queue->item_to_link_map = g_hash_table_new (g_direct_hash, g_direct_equal);

	return queue;
}

void
nemo_file_queue_destroy (NemoFileQueue *queue)
{
	g_hash_table_destroy (queue->item_to_link_map);
	nemo_file_list_free (queue->head);
	g_free (queue);
}

void
nemo_file_queue_enqueue (NemoFileQueue *queue,
			     NemoFile      *file)
{
	if (g_hash_table_lookup (queue->item_to_link_map, file) != NULL) {
		/* It's already on the queue. */
		return;
	}

	if (queue->tail == NULL) {
		queue->head = g_list_append (NULL, file);
		queue->tail = queue->head;
	} else {
		queue->tail = g_list_append (queue->tail, file);
		queue->tail = queue->tail->next;
	}

	nemo_file_ref (file);
	g_hash_table_insert (queue->item_to_link_map, file, queue->tail);
}

NemoFile *
nemo_file_queue_dequeue (NemoFileQueue *queue)
{
	NemoFile *file;

	file = nemo_file_queue_head (queue);
	nemo_file_queue_remove (queue, file);

	return file;
}


void
nemo_file_queue_remove (NemoFileQueue *queue,
			    NemoFile *file)
{
	GList *link;

	link = g_hash_table_lookup (queue->item_to_link_map, file);

	if (link == NULL) {
		/* It's not on the queue */
		return;
	}

	if (link == queue->tail) {
		/* Need to special-case removing the tail. */
		queue->tail = queue->tail->prev;
	}

	queue->head =  g_list_remove_link (queue->head, link);
	g_list_free (link);
	g_hash_table_remove (queue->item_to_link_map, file);

	nemo_file_unref (file);
}

NemoFile *
nemo_file_queue_head (NemoFileQueue *queue)
{
	if (queue->head == NULL) {
		return NULL;
	}

	return NEMO_FILE (queue->head->data);
}

gboolean
nemo_file_queue_is_empty (NemoFileQueue *queue)
{
	return (queue->head == NULL);
}
