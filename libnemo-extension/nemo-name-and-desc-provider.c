/*
 *  nemo-name-and-desc-provider.c - Interface for Nemo extensions that 
 *  returns the extension's proper name and description for the plugin
 *  manager only - it is not necessary for extension functionality.
 *
 */

#include <config.h>
#include "nemo-name-and-desc-provider.h"

#include <glib-object.h>

static void
nemo_name_and_desc_provider_base_init (gpointer g_class)
{
}

GType                   
nemo_name_and_desc_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info = {
			sizeof (NemoNameAndDescProviderIface),
			nemo_name_and_desc_provider_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NemoNameAndDescProvider",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

/**
 * nemo_name_and_desc_provider_get_name_and_desc:
 * @provider: a #NemoNameAndDescProvider
 *
 * Returns: (element-type gchar) (transfer full): a list of name:::desc
 * strings.
 */
GList *
nemo_name_and_desc_provider_get_name_and_desc (NemoNameAndDescProvider *provider)
{
	g_return_val_if_fail (NEMO_IS_NAME_AND_DESC_PROVIDER (provider), NULL);
	g_return_val_if_fail (NEMO_NAME_AND_DESC_PROVIDER_GET_IFACE (provider)->get_name_and_desc != NULL, NULL);

	return NEMO_NAME_AND_DESC_PROVIDER_GET_IFACE (provider)->get_name_and_desc (provider);
}

