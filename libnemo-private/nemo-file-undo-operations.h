/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-undo-operations.h - Manages undo/redo of file operations
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NEMO_FILE_UNDO_OPERATIONS_H__
#define __NEMO_FILE_UNDO_OPERATIONS_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

typedef enum {
	NEMO_FILE_UNDO_OP_COPY,
	NEMO_FILE_UNDO_OP_DUPLICATE,
	NEMO_FILE_UNDO_OP_MOVE,
	NEMO_FILE_UNDO_OP_RENAME,
	NEMO_FILE_UNDO_OP_CREATE_EMPTY_FILE,
	NEMO_FILE_UNDO_OP_CREATE_FILE_FROM_TEMPLATE,
	NEMO_FILE_UNDO_OP_CREATE_FOLDER,
	NEMO_FILE_UNDO_OP_MOVE_TO_TRASH,
	NEMO_FILE_UNDO_OP_RESTORE_FROM_TRASH,
	NEMO_FILE_UNDO_OP_CREATE_LINK,
	NEMO_FILE_UNDO_OP_RECURSIVE_SET_PERMISSIONS,
	NEMO_FILE_UNDO_OP_SET_PERMISSIONS,
	NEMO_FILE_UNDO_OP_CHANGE_GROUP,
	NEMO_FILE_UNDO_OP_CHANGE_OWNER,
	NEMO_FILE_UNDO_OP_NUM_TYPES,
} NemoFileUndoOp;

#define NEMO_TYPE_FILE_UNDO_INFO         (nemo_file_undo_info_get_type ())
#define NEMO_FILE_UNDO_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO, NemoFileUndoInfo))
#define NEMO_FILE_UNDO_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO, NemoFileUndoInfoClass))
#define NEMO_IS_FILE_UNDO_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO))
#define NEMO_IS_FILE_UNDO_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO))
#define NEMO_FILE_UNDO_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO, NemoFileUndoInfoClass))

typedef struct _NemoFileUndoInfo      NemoFileUndoInfo;
typedef struct _NemoFileUndoInfoClass NemoFileUndoInfoClass;
typedef struct _NemoFileUndoInfoDetails NemoFileUndoInfoDetails;

struct _NemoFileUndoInfo {
	GObject parent;
	NemoFileUndoInfoDetails *priv;
};

struct _NemoFileUndoInfoClass {
	GObjectClass parent_class;

	void (* undo_func) (NemoFileUndoInfo *self,
			    GtkWindow            *parent_window);
	void (* redo_func) (NemoFileUndoInfo *self,
			    GtkWindow            *parent_window);

	void (* strings_func) (NemoFileUndoInfo *self,
			       gchar **undo_label,
			       gchar **undo_description,
			       gchar **redo_label,
			       gchar **redo_description);
};

GType nemo_file_undo_info_get_type (void) G_GNUC_CONST;

void nemo_file_undo_info_apply_async (NemoFileUndoInfo *self,
					  gboolean undo,
					  GtkWindow *parent_window,
					  GAsyncReadyCallback callback,
					  gpointer user_data);
gboolean nemo_file_undo_info_apply_finish (NemoFileUndoInfo *self,
					       GAsyncResult *res,
					       gboolean *user_cancel,
					       GError **error);

void nemo_file_undo_info_get_strings (NemoFileUndoInfo *self,
					  gchar **undo_label,
					  gchar **undo_description,
					  gchar **redo_label,
					  gchar **redo_description);

/* copy/move/duplicate/link/restore from trash */
#define NEMO_TYPE_FILE_UNDO_INFO_EXT         (nemo_file_undo_info_ext_get_type ())
#define NEMO_FILE_UNDO_INFO_EXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO_EXT, NemoFileUndoInfoExt))
#define NEMO_FILE_UNDO_INFO_EXT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO_EXT, NemoFileUndoInfoExtClass))
#define NEMO_IS_FILE_UNDO_INFO_EXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO_EXT))
#define NEMO_IS_FILE_UNDO_INFO_EXT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO_EXT))
#define NEMO_FILE_UNDO_INFO_EXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO_EXT, NemoFileUndoInfoExtClass))

typedef struct _NemoFileUndoInfoExt      NemoFileUndoInfoExt;
typedef struct _NemoFileUndoInfoExtClass NemoFileUndoInfoExtClass;
typedef struct _NemoFileUndoInfoExtDetails NemoFileUndoInfoExtDetails;

struct _NemoFileUndoInfoExt {
	NemoFileUndoInfo parent;
	NemoFileUndoInfoExtDetails *priv;
};

struct _NemoFileUndoInfoExtClass {
	NemoFileUndoInfoClass parent_class;
};

GType nemo_file_undo_info_ext_get_type (void) G_GNUC_CONST;
NemoFileUndoInfo *nemo_file_undo_info_ext_new (NemoFileUndoOp op_type,
						       gint item_count,
						       GFile *src_dir,
						       GFile *target_dir);
void nemo_file_undo_info_ext_add_origin_target_pair (NemoFileUndoInfoExt *self,
							 GFile                   *origin,
							 GFile                   *target);

/* create new file/folder */
#define NEMO_TYPE_FILE_UNDO_INFO_CREATE         (nemo_file_undo_info_create_get_type ())
#define NEMO_FILE_UNDO_INFO_CREATE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO_CREATE, NemoFileUndoInfoCreate))
#define NEMO_FILE_UNDO_INFO_CREATE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO_CREATE, NemoFileUndoInfoCreateClass))
#define NEMO_IS_FILE_UNDO_INFO_CREATE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO_CREATE))
#define NEMO_IS_FILE_UNDO_INFO_CREATE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO_CREATE))
#define NEMO_FILE_UNDO_INFO_CREATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO_CREATE, NemoFileUndoInfoCreateClass))

typedef struct _NemoFileUndoInfoCreate      NemoFileUndoInfoCreate;
typedef struct _NemoFileUndoInfoCreateClass NemoFileUndoInfoCreateClass;
typedef struct _NemoFileUndoInfoCreateDetails NemoFileUndoInfoCreateDetails;

struct _NemoFileUndoInfoCreate {
	NemoFileUndoInfo parent;
	NemoFileUndoInfoCreateDetails *priv;
};

struct _NemoFileUndoInfoCreateClass {
	NemoFileUndoInfoClass parent_class;
};

GType nemo_file_undo_info_create_get_type (void) G_GNUC_CONST;
NemoFileUndoInfo *nemo_file_undo_info_create_new (NemoFileUndoOp op_type);
void nemo_file_undo_info_create_set_data (NemoFileUndoInfoCreate *self,
					      GFile                      *file,
					      const char                 *template,
					      gint                        length);

/* rename */
#define NEMO_TYPE_FILE_UNDO_INFO_RENAME         (nemo_file_undo_info_rename_get_type ())
#define NEMO_FILE_UNDO_INFO_RENAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO_RENAME, NemoFileUndoInfoRename))
#define NEMO_FILE_UNDO_INFO_RENAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO_RENAME, NemoFileUndoInfoRenameClass))
#define NEMO_IS_FILE_UNDO_INFO_RENAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO_RENAME))
#define NEMO_IS_FILE_UNDO_INFO_RENAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO_RENAME))
#define NEMO_FILE_UNDO_INFO_RENAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO_RENAME, NemoFileUndoInfoRenameClass))

typedef struct _NemoFileUndoInfoRename      NemoFileUndoInfoRename;
typedef struct _NemoFileUndoInfoRenameClass NemoFileUndoInfoRenameClass;
typedef struct _NemoFileUndoInfoRenameDetails NemoFileUndoInfoRenameDetails;

struct _NemoFileUndoInfoRename {
	NemoFileUndoInfo parent;
	NemoFileUndoInfoRenameDetails *priv;
};

struct _NemoFileUndoInfoRenameClass {
	NemoFileUndoInfoClass parent_class;
};

GType nemo_file_undo_info_rename_get_type (void) G_GNUC_CONST;
NemoFileUndoInfo *nemo_file_undo_info_rename_new (void);
void nemo_file_undo_info_rename_set_data (NemoFileUndoInfoRename *self,
					      GFile                      *old_file,
					      GFile                      *new_file);

/* trash */
#define NEMO_TYPE_FILE_UNDO_INFO_TRASH         (nemo_file_undo_info_trash_get_type ())
#define NEMO_FILE_UNDO_INFO_TRASH(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO_TRASH, NemoFileUndoInfoTrash))
#define NEMO_FILE_UNDO_INFO_TRASH_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO_TRASH, NemoFileUndoInfoTrashClass))
#define NEMO_IS_FILE_UNDO_INFO_TRASH(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO_TRASH))
#define NEMO_IS_FILE_UNDO_INFO_TRASH_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO_TRASH))
#define NEMO_FILE_UNDO_INFO_TRASH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO_TRASH, NemoFileUndoInfoTrashClass))

typedef struct _NemoFileUndoInfoTrash      NemoFileUndoInfoTrash;
typedef struct _NemoFileUndoInfoTrashClass NemoFileUndoInfoTrashClass;
typedef struct _NemoFileUndoInfoTrashDetails NemoFileUndoInfoTrashDetails;

struct _NemoFileUndoInfoTrash {
	NemoFileUndoInfo parent;
	NemoFileUndoInfoTrashDetails *priv;
};

struct _NemoFileUndoInfoTrashClass {
	NemoFileUndoInfoClass parent_class;
};

GType nemo_file_undo_info_trash_get_type (void) G_GNUC_CONST;
NemoFileUndoInfo *nemo_file_undo_info_trash_new (gint item_count);
void nemo_file_undo_info_trash_add_file (NemoFileUndoInfoTrash *self,
					     GFile                     *file);

/* recursive permissions */
#define NEMO_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS         (nemo_file_undo_info_rec_permissions_get_type ())
#define NEMO_FILE_UNDO_INFO_REC_PERMISSIONS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NemoFileUndoInfoRecPermissions))
#define NEMO_FILE_UNDO_INFO_REC_PERMISSIONS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NemoFileUndoInfoRecPermissionsClass))
#define NEMO_IS_FILE_UNDO_INFO_REC_PERMISSIONS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS))
#define NEMO_IS_FILE_UNDO_INFO_REC_PERMISSIONS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS))
#define NEMO_FILE_UNDO_INFO_REC_PERMISSIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NemoFileUndoInfoRecPermissionsClass))

typedef struct _NemoFileUndoInfoRecPermissions      NemoFileUndoInfoRecPermissions;
typedef struct _NemoFileUndoInfoRecPermissionsClass NemoFileUndoInfoRecPermissionsClass;
typedef struct _NemoFileUndoInfoRecPermissionsDetails NemoFileUndoInfoRecPermissionsDetails;

struct _NemoFileUndoInfoRecPermissions {
	NemoFileUndoInfo parent;
	NemoFileUndoInfoRecPermissionsDetails *priv;
};

struct _NemoFileUndoInfoRecPermissionsClass {
	NemoFileUndoInfoClass parent_class;
};

GType nemo_file_undo_info_rec_permissions_get_type (void) G_GNUC_CONST;
NemoFileUndoInfo *nemo_file_undo_info_rec_permissions_new (GFile   *dest,
								   guint32 file_permissions,
								   guint32 file_mask,
								   guint32 dir_permissions,
								   guint32 dir_mask);
void nemo_file_undo_info_rec_permissions_add_file (NemoFileUndoInfoRecPermissions *self,
						       GFile                              *file,
						       guint32                             permission);

/* single file change permissions */
#define NEMO_TYPE_FILE_UNDO_INFO_PERMISSIONS         (nemo_file_undo_info_permissions_get_type ())
#define NEMO_FILE_UNDO_INFO_PERMISSIONS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO_PERMISSIONS, NemoFileUndoInfoPermissions))
#define NEMO_FILE_UNDO_INFO_PERMISSIONS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO_PERMISSIONS, NemoFileUndoInfoPermissionsClass))
#define NEMO_IS_FILE_UNDO_INFO_PERMISSIONS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO_PERMISSIONS))
#define NEMO_IS_FILE_UNDO_INFO_PERMISSIONS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO_PERMISSIONS))
#define NEMO_FILE_UNDO_INFO_PERMISSIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO_PERMISSIONS, NemoFileUndoInfoPermissionsClass))

typedef struct _NemoFileUndoInfoPermissions      NemoFileUndoInfoPermissions;
typedef struct _NemoFileUndoInfoPermissionsClass NemoFileUndoInfoPermissionsClass;
typedef struct _NemoFileUndoInfoPermissionsDetails NemoFileUndoInfoPermissionsDetails;

struct _NemoFileUndoInfoPermissions {
	NemoFileUndoInfo parent;
	NemoFileUndoInfoPermissionsDetails *priv;
};

struct _NemoFileUndoInfoPermissionsClass {
	NemoFileUndoInfoClass parent_class;
};

GType nemo_file_undo_info_permissions_get_type (void) G_GNUC_CONST;
NemoFileUndoInfo *nemo_file_undo_info_permissions_new (GFile   *file,
							       guint32  current_permissions,
							       guint32  new_permissions);

/* group and owner change */
#define NEMO_TYPE_FILE_UNDO_INFO_OWNERSHIP         (nemo_file_undo_info_ownership_get_type ())
#define NEMO_FILE_UNDO_INFO_OWNERSHIP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_UNDO_INFO_OWNERSHIP, NemoFileUndoInfoOwnership))
#define NEMO_FILE_UNDO_INFO_OWNERSHIP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_UNDO_INFO_OWNERSHIP, NemoFileUndoInfoOwnershipClass))
#define NEMO_IS_FILE_UNDO_INFO_OWNERSHIP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_UNDO_INFO_OWNERSHIP))
#define NEMO_IS_FILE_UNDO_INFO_OWNERSHIP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_UNDO_INFO_OWNERSHIP))
#define NEMO_FILE_UNDO_INFO_OWNERSHIP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_UNDO_INFO_OWNERSHIP, NemoFileUndoInfoOwnershipClass))

typedef struct _NemoFileUndoInfoOwnership      NemoFileUndoInfoOwnership;
typedef struct _NemoFileUndoInfoOwnershipClass NemoFileUndoInfoOwnershipClass;
typedef struct _NemoFileUndoInfoOwnershipDetails NemoFileUndoInfoOwnershipDetails;

struct _NemoFileUndoInfoOwnership {
	NemoFileUndoInfo parent;
	NemoFileUndoInfoOwnershipDetails *priv;
};

struct _NemoFileUndoInfoOwnershipClass {
	NemoFileUndoInfoClass parent_class;
};

GType nemo_file_undo_info_ownership_get_type (void) G_GNUC_CONST;
NemoFileUndoInfo *nemo_file_undo_info_ownership_new (NemoFileUndoOp  op_type,
							     GFile              *file,
							     const char         *current_data,
							     const char         *new_data);

#endif /* __NEMO_FILE_UNDO_OPERATIONS_H__ */
