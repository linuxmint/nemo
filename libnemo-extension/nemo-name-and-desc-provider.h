/*
 *  nemo-name-and-desc-provider.h - Interface for Nemo extensions that 
 *  returns the extension's proper name and description for the plugin
 *  manager only - it is not necessary for extension functionality.
 *
 */

#ifndef NEMO_NAME_AND_DESC_PROVIDER_H
#define NEMO_NAME_AND_DESC_PROVIDER_H

#include <glib-object.h>
#include "nemo-extension-types.h"

G_BEGIN_DECLS

#define NEMO_TYPE_NAME_AND_DESC_PROVIDER           (nemo_name_and_desc_provider_get_type ())
#define NEMO_NAME_AND_DESC_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_NAME_AND_DESC_PROVIDER, NemoNameAndDescProvider))
#define NEMO_IS_NAME_AND_DESC_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_NAME_AND_DESC_PROVIDER))
#define NEMO_NAME_AND_DESC_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_NAME_AND_DESC_PROVIDER, NemoNameAndDescProviderIface))

typedef struct _NemoNameAndDescProvider       NemoNameAndDescProvider;
typedef struct _NemoNameAndDescProviderIface  NemoNameAndDescProviderIface;

struct _NemoNameAndDescProviderIface {
	GTypeInterface g_iface;

	GList *(*get_name_and_desc) (NemoNameAndDescProvider *provider);
};

/* Interface Functions */
GType                   nemo_name_and_desc_provider_get_type          (void);
GList                  *nemo_name_and_desc_provider_get_name_and_desc (NemoNameAndDescProvider *provider);

G_END_DECLS

#endif
