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
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Maciej Stachowiak <mjs@noisehavoc.org>
*/

#ifndef NEMO_FILE_QUEUE_H
#define NEMO_FILE_QUEUE_H

#include <libnemo-private/nemo-file.h>

typedef struct NemoFileQueue NemoFileQueue;

NemoFileQueue *nemo_file_queue_new      (void);
void               nemo_file_queue_destroy  (NemoFileQueue *queue);

/* Add a file to the tail of the queue, unless it's already in the queue */
void               nemo_file_queue_enqueue  (NemoFileQueue *queue,
						 NemoFile      *file);

/* Return the file at the head of the queue after removing it from the
 * queue. This is dangerous unless you have another ref to the file,
 * since it will unref it.  
 */
NemoFile *     nemo_file_queue_dequeue  (NemoFileQueue *queue);

/* Remove a file from an arbitrary point in the queue in constant time. */
void               nemo_file_queue_remove   (NemoFileQueue *queue,
						 NemoFile      *file);

/* Get the file at the head of the queue without removing or unrefing it. */
NemoFile *     nemo_file_queue_head     (NemoFileQueue *queue);

gboolean           nemo_file_queue_is_empty (NemoFileQueue *queue);

#endif /* NEMO_FILE_CHANGES_QUEUE_H */
