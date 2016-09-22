#define GOA_API_IS_SUBJECT_TO_CHANGE

#include <glib.h>
#include <goa/goa.h>
#include <gfbgraph/gfbgraph.h>
#include <gfbgraph/gfbgraph-goa-authorizer.h>

#undef GOA_API_IS_SUBJECT_TO_CHANGE

static gboolean
is_facebook_object (GoaObject *object)
{
  GoaAccount *account = goa_object_peek_account (object);
  const gchar *provider_type = goa_account_get_provider_type (account);

  return g_strcmp0 (provider_type, "facebook") == 0;
}

static gboolean
print_user_friends (GoaObject *facebook_account_object,
		    GError **error)
{
  GoaAccount *account = goa_object_peek_account (facebook_account_object);
  const gchar *id = goa_account_get_identity (account);
  GFBGraphAuthorizer *auth = NULL;
  GFBGraphUser *user = NULL;
  GList *friends = NULL;
  GList *iter;
  gboolean success = FALSE;

  auth = GFBGRAPH_AUTHORIZER (gfbgraph_goa_authorizer_new (facebook_account_object));
  if (!gfbgraph_authorizer_refresh_authorization (auth, NULL, error))
    {
      g_prefix_error (error, "failed to get an access token for user with id %s:", id);
      goto out;
    }

  user = gfbgraph_user_new_from_id (auth, id, error);
  if (user == NULL)
    {
      g_prefix_error (error, "failed to get facebook user with id %s: ", id);
      goto out;
    }

  friends = gfbgraph_user_get_friends (user, auth, error);
  if (friends == NULL)
    {
      g_prefix_error (error, "failed to get friends of user with id %s: ", id);
      goto out;
    }

  g_print ("Friends of %s:\n", gfbgraph_user_get_name (user));
  for (iter = friends; iter != NULL; iter = iter->next)
    {
      GFBGraphUser *friend = GFBGRAPH_USER (iter->data);

      g_print ("  - %s\n", gfbgraph_user_get_name (friend));
    }

  success = TRUE;

 out:
  g_list_free_full (friends, g_object_unref);
  if (user != NULL)
    g_object_unref (user);
  if (auth != NULL)
    g_object_unref (auth);

  return success;
}

static gboolean
print_facebook_friends (GList *accounts,
			GError **error)
{
  GList *iter;
  gboolean success = FALSE;

  for (iter = accounts; iter != NULL; iter = iter->next)
    {
      GoaObject *object = GOA_OBJECT (iter->data);

      if (!is_facebook_object (object))
	continue;

      if (!print_user_friends (object, error))
	{
	  g_prefix_error (error, "failed to print user's friends: ");
	  goto out;
	}
    }

  success = TRUE;

 out:
  return success;
}

int
main (void)
{
  GError *error = NULL;
  GList *accounts = NULL;
  GoaClient* client = goa_client_new_sync (NULL, &error);
  int status = 1;

  if (client == NULL)
    {
      g_warning ("Failed to get goa client: %s", error->message);
      goto out;
    }

  accounts = goa_client_get_accounts (client);
  if (!print_facebook_friends (accounts, &error))
    {
      g_warning ("Failed to print all facebook friends: %s", error->message);
      goto out;
    }

  status = 0;

 out:
  g_list_free_full (accounts, g_object_unref);
  if (client != NULL)
    g_object_unref (client);
  if (error != NULL)
    g_error_free (error);

  return status;
}
