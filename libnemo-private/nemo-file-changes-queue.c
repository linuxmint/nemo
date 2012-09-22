/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>
#include "nemo-file-changes-queue.h"

#include "nemo-directory-notify.h"

typedef enum {
	CHANGE_FILE_INITIAL,
	CHANGE_FILE_ADDED,
	CHANGE_FILE_CHANGED,
	CHANGE_FILE_REMOVED,
	CHANGE_FILE_MOVED,
	CHANGE_POSITION_SET,
	CHANGE_POSITION_REMOVE
} NemoFileChangeKind;

typedef struct {
	NemoFileChangeKind kind;
	GFile *from;
	GFile *to;
	GdkPoint point;
	int screen;
} NemoFileChange;

typedef struct {
	GList *head;
	GList *tail;
	GMutex mutex;
} NemoFileChangesQueue;

static NemoFileChangesQueue *
nemo_file_changes_queue_new (void)
{
	NemoFileChangesQueue *result;

	result = g_new0 (NemoFileChangesQueue, 1);
	g_mutex_init (&result->mutex);

	return result;
}

static NemoFileChangesQueue *
nemo_file_changes_queue_get (void)
{
	static NemoFileChangesQueue *file_changes_queue;

	if (file_changes_queue == NULL) {
		file_changes_queue = nemo_file_changes_queue_new ();
	}

	return file_changes_queue;
}

static void
nemo_file_changes_queue_add_common (NemoFileChangesQueue *queue, 
	NemoFileChange *new_item)
{
	/* enqueue the new queue item while locking down the list */
	g_mutex_lock (&queue->mutex);

	queue->head = g_list_prepend (queue->head, new_item);
	if (queue->tail == NULL)
		queue->tail = queue->head;

	g_mutex_unlock (&queue->mutex);
}

void
nemo_file_changes_queue_file_added (GFile *location)
{
	NemoFileChange *new_item;
	NemoFileChangesQueue *queue;

	queue = nemo_file_changes_queue_get();

	new_item = g_new0 (NemoFileChange, 1);
	new_item->kind = CHANGE_FILE_ADDED;
	new_item->from = g_object_ref (location);
	nemo_file_changes_queue_add_common (queue, new_item);
}

void
nemo_file_changes_queue_file_changed (GFile *location)
{
	NemoFileChange *new_item;
	NemoFileChangesQueue *queue;

	queue = nemo_file_changes_queue_get();

	new_item = g_new0 (NemoFileChange, 1);
	new_item->kind = CHANGE_FILE_CHANGED;
	new_item->from = g_object_ref (location);
	nemo_file_changes_queue_add_common (queue, new_item);
}

void
nemo_file_changes_queue_file_removed (GFile *location)
{
	NemoFileChange *new_item;
	NemoFileChangesQueue *queue;

	queue = nemo_file_changes_queue_get();

	new_item = g_new0 (NemoFileChange, 1);
	new_item->kind = CHANGE_FILE_REMOVED;
	new_item->from = g_object_ref (location);
	nemo_file_changes_queue_add_common (queue, new_item);
}

void
nemo_file_changes_queue_file_moved (GFile *from,
					GFile *to)
{
	NemoFileChange *new_item;
	NemoFileChangesQueue *queue;

	queue = nemo_file_changes_queue_get ();

	new_item = g_new (NemoFileChange, 1);
	new_item->kind = CHANGE_FILE_MOVED;
	new_item->from = g_object_ref (from);
	new_item->to = g_object_ref (to);
	nemo_file_changes_queue_add_common (queue, new_item);
}

void
nemo_file_changes_queue_schedule_position_set (GFile *location, 
						   GdkPoint point,
						   int screen)
{
	NemoFileChange *new_item;
	NemoFileChangesQueue *queue;

	queue = nemo_file_changes_queue_get ();

	new_item = g_new (NemoFileChange, 1);
	new_item->kind = CHANGE_POSITION_SET;
	new_item->from = g_object_ref (location);
	new_item->point = point;
	new_item->screen = screen;
	nemo_file_changes_queue_add_common (queue, new_item);
}

void
nemo_file_changes_queue_schedule_position_remove (GFile *location)
{
	NemoFileChange *new_item;
	NemoFileChangesQueue *queue;

	queue = nemo_file_changes_queue_get ();

	new_item = g_new (NemoFileChange, 1);
	new_item->kind = CHANGE_POSITION_REMOVE;
	new_item->from = g_object_ref (location);
	nemo_file_changes_queue_add_common (queue, new_item);
}

static NemoFileChange *
nemo_file_changes_queue_get_change (NemoFileChangesQueue *queue)
{
	GList *new_tail;
	NemoFileChange *result;

	g_assert (queue != NULL);
	
	/* dequeue the tail item while locking down the list */
	g_mutex_lock (&queue->mutex);

	if (queue->tail == NULL) {
		result = NULL;
	} else {
		new_tail = queue->tail->prev;
		result = queue->tail->data;
		queue->head = g_list_remove_link (queue->head,
						  queue->tail);
		g_list_free_1 (queue->tail);
		queue->tail = new_tail;
	}

	g_mutex_unlock (&queue->mutex);

	return result;
}

enum {
	CONSUME_CHANGES_MAX_CHUNK = 20
};

static void
pairs_list_free (GList *pairs)
{
	GList *p;
	GFilePair *pair;

	/* deep delete the list of pairs */

	for (p = pairs; p != NULL; p = p->next) {
		/* delete the strings in each pair */
		pair = p->data;
		g_object_unref (pair->from);
		g_object_unref (pair->to);
	}

	/* delete the list and the now empty pair structs */
	g_list_free_full (pairs, g_free);
}

static void
position_set_list_free (GList *list)
{
	GList *p;
	NemoFileChangesQueuePosition *item;

	for (p = list; p != NULL; p = p->next) {
		item = p->data;
		g_object_unref (item->location);
	}
	/* delete the list and the now empty structs */
	g_list_free_full (list, g_free);
}

/* go through changes in the change queue, send ones with the same kind
 * in a list to the different nemo_directory_notify calls
 */ 
void
nemo_file_changes_consume_changes (gboolean consume_all)
{
	NemoFileChange *change;
	GList *additions, *changes, *deletions, *moves;
	GList *position_set_requests;
	GFilePair *pair;
	NemoFileChangesQueuePosition *position_set;
	guint chunk_count;
	NemoFileChangesQueue *queue;
	gboolean flush_needed;
	

	additions = NULL;
	changes = NULL;
	deletions = NULL;
	moves = NULL;
	position_set_requests = NULL;

	queue = nemo_file_changes_queue_get();
		
	/* Consume changes from the queue, stuffing them into one of three lists,
	 * keep doing it while the changes are of the same kind, then send them off.
	 * This is to ensure that the changes get sent off in the same order that they 
	 * arrived.
	 */
	for (chunk_count = 0; ; chunk_count++) {
		change = nemo_file_changes_queue_get_change (queue);

		/* figure out if we need to flush the pending changes that we collected sofar */

		if (change == NULL) {
			flush_needed = TRUE;
			/* no changes left, flush everything */
		} else {
			flush_needed = additions != NULL
				&& change->kind != CHANGE_FILE_ADDED
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE;
			
			flush_needed |= changes != NULL
				&& change->kind != CHANGE_FILE_CHANGED;
			
			flush_needed |= moves != NULL
				&& change->kind != CHANGE_FILE_MOVED
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE;
			
			flush_needed |= deletions != NULL
				&& change->kind != CHANGE_FILE_REMOVED;
			
			flush_needed |= position_set_requests != NULL
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE
				&& change->kind != CHANGE_FILE_ADDED
				&& change->kind != CHANGE_FILE_MOVED;
			
			flush_needed |= !consume_all && chunk_count >= CONSUME_CHANGES_MAX_CHUNK;
				/* we have reached the chunk maximum */
		}
		
		if (flush_needed) {
			/* Send changes we collected off. 
			 * At one time we may only have one of the lists
			 * contain changes.
			 */
			
			if (deletions != NULL) {
				deletions = g_list_reverse (deletions);
				nemo_directory_notify_files_removed (deletions);
				g_list_free_full (deletions, g_object_unref);
				deletions = NULL;
			}
			if (moves != NULL) {
				moves = g_list_reverse (moves);
				nemo_directory_notify_files_moved (moves);
				pairs_list_free (moves);
				moves = NULL;
			}
			if (additions != NULL) {
				additions = g_list_reverse (additions);
				nemo_directory_notify_files_added (additions);
				g_list_free_full (additions, g_object_unref);
				additions = NULL;
			}
			if (changes != NULL) {
				changes = g_list_reverse (changes);
				nemo_directory_notify_files_changed (changes);
				g_list_free_full (changes, g_object_unref);
				changes = NULL;
			}
			if (position_set_requests != NULL) {
				position_set_requests = g_list_reverse (position_set_requests);
				nemo_directory_schedule_position_set (position_set_requests);
				position_set_list_free (position_set_requests);
				position_set_requests = NULL;
			}
		}

		if (change == NULL) {
			/* we are done */
			return;
		}
		
		/* add the new change to the list */
		switch (change->kind) {
		case CHANGE_FILE_ADDED:
			additions = g_list_prepend (additions, change->from);
			break;

		case CHANGE_FILE_CHANGED:
			changes = g_list_prepend (changes, change->from);
			break;

		case CHANGE_FILE_REMOVED:
			deletions = g_list_prepend (deletions, change->from);
			break;

		case CHANGE_FILE_MOVED:
			pair = g_new (GFilePair, 1);
			pair->from = change->from;
			pair->to = change->to;
			moves = g_list_prepend (moves, pair);
			break;

		case CHANGE_POSITION_SET:
			position_set = g_new (NemoFileChangesQueuePosition, 1);
			position_set->location = change->from;
			position_set->set = TRUE;
			position_set->point = change->point;
			position_set->screen = change->screen;
			position_set_requests = g_list_prepend (position_set_requests,
								position_set);
			break;

		case CHANGE_POSITION_REMOVE:
			position_set = g_new (NemoFileChangesQueuePosition, 1);
			position_set->location = change->from;
			position_set->set = FALSE;
			position_set_requests = g_list_prepend (position_set_requests,
								position_set);
			break;

		default:
			g_assert_not_reached ();
			break;
		}

		g_free (change);
	}	
}
