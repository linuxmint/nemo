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
nemo_name_and_desc_provider_default_init (NemoNameAndDescProviderInterface *klass)
{
}

G_DEFINE_INTERFACE (NemoNameAndDescProvider, nemo_name_and_desc_provider, G_TYPE_OBJECT)

/**
 * nemo_name_and_desc_provider_get_name_and_desc:
 * @provider: a #NemoNameAndDescProvider
 *
 * Returns: (element-type gchar) (transfer full): a list of name:::desc
 * strings.  Optionally, the name of a path executable can be appended as a
 * third component of the list ('name:::desc:::foo-bar-preferences')
 */
GList *
nemo_name_and_desc_provider_get_name_and_desc (NemoNameAndDescProvider *provider)
{
	g_return_val_if_fail (NEMO_IS_NAME_AND_DESC_PROVIDER (provider), NULL);
	g_return_val_if_fail (NEMO_NAME_AND_DESC_PROVIDER_GET_IFACE (provider)->get_name_and_desc != NULL, NULL);

	return NEMO_NAME_AND_DESC_PROVIDER_GET_IFACE (provider)->get_name_and_desc (provider);
}

