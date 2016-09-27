#define GOA_API_IS_SUBJECT_TO_CHANGE

#include <glib.h>
#include <goa/goa.h>
#include <gfbgraph/gfbgraph.h>
#include <gfbgraph/gfbgraph-goa-authorizer.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#undef GOA_API_IS_SUBJECT_TO_CHANGE

static gboolean
is_facebook_object (GoaObject *object)
{
  GoaAccount *account = goa_object_peek_account (object);
  const gchar *provider_type = goa_account_get_provider_type (account);

  return g_strcmp0 (provider_type, "facebook") == 0;
}

static gboolean
save_as_png (const gchar *filename,
	     GBytes *raw,
	     GError **error)
{
  GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
  GdkPixbuf *pixbuf;
  gboolean success = FALSE;

  if (!gdk_pixbuf_loader_write_bytes (loader, raw, error))
    {
      g_prefix_error (error, "failed to write bytes to pixbuf loader: ");
      goto out;
    }
  if (!gdk_pixbuf_loader_close (loader, error))
    {
      g_prefix_error (error, "failed to close a pixbuf loader: ");
      goto out;
    }
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (pixbuf == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "no pixbuf?");
      goto out;
    }
  if (!gdk_pixbuf_save (pixbuf, filename, "png", error, NULL))
    {
      g_prefix_error (error, "failed to save pixbuf as png: ");
      goto out;
    }
  success = TRUE;
 out:
  if (loader != NULL)
    g_object_unref (loader);
  return success;
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
      GBytes *raw = NULL;
      const gchar *friend_id = gfbgraph_node_get_id (GFBGRAPH_NODE (friend));
      const gchar *name = gfbgraph_user_get_name (friend);
      gchar *filename = NULL;
      gboolean ok = FALSE;

      g_print ("  - %s\n", name);
      raw = gfbgraph_user_get_picture (friend, auth, GFBGRAPH_PICTURE_LARGE, error);
      if (raw == NULL)
	{
	  g_prefix_error (error, "failed to get picture of a friend with name %s: ", name);
	  goto loop_out;
	}
      filename = g_strdup_printf("%s.png", friend_id);
      if (!save_as_png (filename, raw, error))
	{
	  g_prefix_error (error, "failed to save picture of a friend with name %s to png file %s: ", name, filename);
	  goto loop_out;
	}
      g_print ("    (PNG photo saved as %s)\n", filename);
      ok = TRUE;

    loop_out:
      if (raw != NULL)
	g_bytes_unref (raw);
      if (filename != NULL)
	g_free (filename);
      if (!ok)
	goto out;
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
