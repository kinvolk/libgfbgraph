/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-  */
/*
 * libgfbgraph - GObject library for Facebook Graph API
 * Copyright (C) 2013 Álvaro Peña <alvaropg@gmail.com>
 *
 * GFBGraph is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GFBGraph is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GFBGraph.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gfbgraph-user
 * @short_description: GFBGraph User node
 * @stability: Unstable
 * @include: gfbgraph/gfbgraph.h
 *
 * #GFBGraphUser represents a <ulink url="https://developers.facebook.com/docs/reference/api/user/">user in Facebook</ulink>.
 * With the "me" functions, (see gfbgraph_user_get_me()) you can query for the logged user node.
 **/

#include <json-glib/json-glib.h>

#include "gfbgraph-user.h"
#include "gfbgraph-album.h"
#include "gfbgraph-common.h"
#include "gfbgraph-connectable.h"

#define ME_FUNCTION "me"

enum {
        PROP_0,

        PROP_NAME
};

struct _GFBGraphUserPrivate {
        gchar *name;
};

typedef struct {
        GFBGraphUser *user;
} GFBGraphUserAsyncData;

typedef struct {
        GFBGraphAuthorizer *authorizer;
        GList *nodes;
} GFBGraphUserConnectionAsyncData;

typedef struct {
        GFBGraphAuthorizer *authorizer;
        GFBGraphPictureType picture_type;
        GBytes *picture;
} GFBGraphUserPictureAsyncData;

static void gfbgraph_user_init         (GFBGraphUser *obj);
static void gfbgraph_user_class_init   (GFBGraphUserClass *klass);
static void gfbgraph_user_finalize     (GObject *obj);
static void gfbgraph_user_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gfbgraph_user_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

/* Private functions */
static void gfbgraph_user_async_data_free (GFBGraphUserAsyncData *data);
static void gfbgraph_user_connection_async_data_free (GFBGraphUserConnectionAsyncData *data);
static void gfbgraph_user_picture_async_data_free (GFBGraphUserPictureAsyncData *data);
static void gfbgraph_user_get_me_async_thread (GSimpleAsyncResult *simple_async, GFBGraphAuthorizer *authorizer, GCancellable cancellable);
static void gfbgraph_user_get_albums_async_thread (GSimpleAsyncResult *simple_async, GFBGraphUser *user, GCancellable cancellable);
static void gfbgraph_user_get_friends_async_thread (GSimpleAsyncResult *simple_async, GFBGraphUser *user, GCancellable cancellable);
static void gfbgraph_user_get_picture_async_thread (GSimpleAsyncResult *simple_async, GFBGraphUser *user, GCancellable cancellable);
static void gfbgraph_user_connectable_iface_init (GFBGraphConnectableInterface *iface);
#define GFBGRAPH_USER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), GFBGRAPH_TYPE_USER, GFBGraphUserPrivate))

static GFBGraphNodeClass *parent_class = NULL;

G_DEFINE_TYPE_WITH_CODE (GFBGraphUser, gfbgraph_user, GFBGRAPH_TYPE_NODE,
                         G_IMPLEMENT_INTERFACE (GFBGRAPH_TYPE_CONNECTABLE, gfbgraph_user_connectable_iface_init));

static void
gfbgraph_user_init (GFBGraphUser *obj)
{
        obj->priv = GFBGRAPH_USER_GET_PRIVATE(obj);
}

static void
gfbgraph_user_class_init (GFBGraphUserClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        parent_class            = g_type_class_peek_parent (klass);
        gobject_class->finalize = gfbgraph_user_finalize;
        gobject_class->set_property = gfbgraph_user_set_property;
        gobject_class->get_property = gfbgraph_user_get_property;

        g_type_class_add_private (gobject_class, sizeof(GFBGraphUserPrivate));

        /**
         * GFBGraphUser:name:
         *
         * The full name of the user
         **/
        g_object_class_install_property (gobject_class,
                                         PROP_NAME,
                                         g_param_spec_string ("name",
                                                              "User's full name", "The full name of the user",
                                                              "",
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gfbgraph_user_finalize (GObject *obj)
{
        G_OBJECT_CLASS(parent_class)->finalize (obj);
}

static void
gfbgraph_user_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        GFBGraphUserPrivate *priv;

        priv = GFBGRAPH_USER_GET_PRIVATE (object);

        switch (prop_id) {
                case PROP_NAME:
                        if (priv->name)
                                g_free (priv->name);
                        priv->name = g_strdup (g_value_get_string (value));
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
        }
}

static void
gfbgraph_user_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
        GFBGraphUserPrivate *priv;

        priv = GFBGRAPH_USER_GET_PRIVATE (object);

        switch (prop_id) {
                case PROP_NAME:
                        g_value_set_string (value, priv->name);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
        }
}

static void
gfbgraph_user_async_data_free (GFBGraphUserAsyncData *data)
{
        g_object_unref (data->user);

        g_slice_free (GFBGraphUserAsyncData, data);
}

static void
gfbgraph_user_connection_async_data_free (GFBGraphUserConnectionAsyncData *data)
{
        g_object_unref (data->authorizer);

        g_slice_free (GFBGraphUserConnectionAsyncData, data);
}

static void
gfbgraph_user_picture_async_data_free (GFBGraphUserPictureAsyncData *data)
{
        if (data->authorizer != NULL)
                g_object_unref (data->authorizer);
        if (data->picture != NULL)
                g_bytes_unref (data->picture);

        g_slice_free (GFBGraphUserPictureAsyncData, data);
}

static void
gfbgraph_user_get_me_async_thread (GSimpleAsyncResult *simple_async, GFBGraphAuthorizer *authorizer, GCancellable cancellable)
{
        GFBGraphUserAsyncData *data;
        GError *error;

        data = (GFBGraphUserAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);

        error = NULL;
        data->user = gfbgraph_user_get_me (authorizer, &error);
        if (error != NULL)
                g_simple_async_result_take_error (simple_async, error);
}

static void
gfbgraph_user_get_albums_async_thread (GSimpleAsyncResult *simple_async, GFBGraphUser *user, GCancellable cancellable)
{
        GFBGraphUserConnectionAsyncData *data;
        GError *error;

        data = (GFBGraphUserConnectionAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);

        error = NULL;
        data->nodes = gfbgraph_user_get_albums (user, data->authorizer, &error);
        if (error != NULL)
                g_simple_async_result_take_error (simple_async, error);
}

static void
gfbgraph_user_get_friends_async_thread (GSimpleAsyncResult *simple_async, GFBGraphUser *user, GCancellable cancellable)
{
        GFBGraphUserConnectionAsyncData *data;
        GError *error;

        data = (GFBGraphUserConnectionAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);

        error = NULL;
        data->nodes = gfbgraph_user_get_friends (user, data->authorizer, &error);
        if (error != NULL)
                g_simple_async_result_take_error (simple_async, error);
}

static void
gfbgraph_user_get_picture_async_thread (GSimpleAsyncResult *simple_async, GFBGraphUser *user, GCancellable cancellable)
{
        GFBGraphUserPictureAsyncData *data;
        GError *error;

        data = (GFBGraphUserPictureAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);

        error = NULL;
        data->picture = gfbgraph_user_get_picture (user, data->authorizer, data->picture_type, &error);
        if (error != NULL)
                g_simple_async_result_take_error (simple_async, error);
}

/**
 * gfbgraph_user_new:
 *
 * Creates a new #GFBGraphUser.
 *
 * Returns: a new #GFBGraphUser; unref with g_object_unref()
 **/
GFBGraphUser*
gfbgraph_user_new (void)
{
        return GFBGRAPH_USER (g_object_new (GFBGRAPH_TYPE_USER, NULL));
}

/**
 * gfbgraph_user_new_from_id:
 * @authorizer: a #GFBGraphAuthorizer.
 * @id: a const #gchar with the user ID.
 * @error: (allow-none): a #GError or %NULL.
 *
 * Retrieves a user from the Facebook Graph with the give ID.
 *
 * Returns: (transfer full): a new #GFBGraphUser; unref with g_object_unref()
 **/
GFBGraphUser*
gfbgraph_user_new_from_id (GFBGraphAuthorizer *authorizer, const gchar *id, GError **error)
{
        return GFBGRAPH_USER (gfbgraph_node_new_from_id (authorizer, id, GFBGRAPH_TYPE_USER, error));
}

/**
 * gfbgraph_user_get_me:
 * @authorizer: a #GFBGraphAuthorizer.
 * @error: (allow-none): a #GError or %NULL.
 *
 * Retrieve the current user logged using the https://graph.facebook.com/me Graph API function.
 * See gfbgraph_user_get_my_async() for the asynchronous version of this call.
 *
 * Returns: (transfer full): a #GFBGraphUser with the current user information.
 **/
GFBGraphUser*
gfbgraph_user_get_me (GFBGraphAuthorizer *authorizer, GError **error)
{
        GFBGraphUser *me;
        RestProxyCall *rest_call;
        const gchar *payload;
        gboolean result;

        g_return_val_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer), NULL);

        me = NULL;

        rest_call = gfbgraph_new_rest_call (authorizer);
        rest_proxy_call_set_function (rest_call, ME_FUNCTION);
        rest_proxy_call_set_method (rest_call, "GET");

        result = rest_proxy_call_sync (rest_call, error);
        if (result) {
                JsonParser *parser;
                JsonNode *node;

                payload = rest_proxy_call_get_payload (rest_call);
                parser = json_parser_new ();
                if (json_parser_load_from_data (parser, payload, -1, error)) {
                        node = json_parser_get_root (parser);
                        me = GFBGRAPH_USER (json_gobject_deserialize (GFBGRAPH_TYPE_USER, node));
                }

                g_object_unref (parser);
        }

        return me;
}

/**
 * gfbgraph_user_get_me_async:
 * @authorizer: a #GFBGraphAuthorizer.
 * @cancellable: (allow-none): An optional #GCancellable object, or %NULL.
 * @callback: (scope async): A #GAsyncReadyCallback to call when the request is completed.
 * @user_data: (closure): The data to pass to @callback.
 *
 * Asynchronously retrieve the current user logged. See gfbgraph_user_get_me() for the
 * synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call gfbgraph_user_get_me_finish()
 * to get the #GFBGraphUser for to the current user logged.
 **/
void
gfbgraph_user_get_me_async (GFBGraphAuthorizer *authorizer, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserAsyncData *data;

        g_return_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer));
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
        g_return_if_fail (callback != NULL);

        simple_async = g_simple_async_result_new (G_OBJECT (authorizer), callback, user_data, gfbgraph_user_get_me_async);
        g_simple_async_result_set_check_cancellable (simple_async, cancellable);

        data = g_slice_new (GFBGraphUserAsyncData);
        data->user = NULL;

        g_simple_async_result_set_op_res_gpointer (simple_async, data, (GDestroyNotify) gfbgraph_user_async_data_free);
        g_simple_async_result_run_in_thread (simple_async, (GSimpleAsyncThreadFunc) gfbgraph_user_get_me_async_thread, G_PRIORITY_DEFAULT, cancellable);

        g_object_unref (simple_async);
}

/**
 * gfbgraph_user_get_me_async_finish:
 * @authorizer: a #GFBGraphAuthorizer.
 * @result: A #GAsyncResult.
 * @error: (allow-none): An optional #GError, or %NULL.
 *
 * Finishes an asynchronous operation started with
 * gfbgraph_user_get_me_async().
 *
 * Returns: (transfer full): a #GFBGraphUser for to the current user logged.
 **/
GFBGraphUser*
gfbgraph_user_get_me_async_finish (GFBGraphAuthorizer *authorizer, GAsyncResult *result, GError **error)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserAsyncData *data;

        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (authorizer), gfbgraph_user_get_me_async), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        simple_async = G_SIMPLE_ASYNC_RESULT (result);

        if (g_simple_async_result_propagate_error (simple_async, error))
                return NULL;

        data = (GFBGraphUserAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);
        return data->user;
}

/**
 * gfbgraph_user_get_albums:
 * @user: a #GFBGraphUser.
 * @authorizer: a #GFBGraphAuthorizer.
 * @error: (allow-none): An optional #GError, or %NULL.
 *
 * Retrieve the albums nodes owned by the @user. This functions call the function ID/albums.
 *
 * Returns: (element-type GFBGraphAlbum) (transfer full): a newly-allocated #GList with the albums nodes owned by the given user.
 **/
GList*
gfbgraph_user_get_albums (GFBGraphUser *user, GFBGraphAuthorizer *authorizer, GError **error)
{
        g_return_val_if_fail (GFBGRAPH_IS_USER (user), NULL);
        g_return_val_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer), NULL);

        return gfbgraph_node_get_connection_nodes (GFBGRAPH_NODE (user), GFBGRAPH_TYPE_ALBUM, authorizer, error);
}

/**
 * gfbgraph_user_get_albums_async:
 * @user: a #GFBGraphUser.
 * @authorizer: a #GFBGraphAuthorizer.
 * @cancellable: (allow-none): An optional #GCancellable object, or %NULL.
 * @callback: (scope async): A #GAsyncReadyCallback to call when the request is completed.
 * @user_data: (closure): The data to pass to @callback.
 *
 * Asynchronously retrieve the albums nodes owned by the @user. See gfbgraph_user_get_albums() for the
 * synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call gfbgraph_user_get_albums_async_finish()
 * to get the #GList of #GFBGraphAlbum owned by the @user.
 **/
void
gfbgraph_user_get_albums_async (GFBGraphUser *user, GFBGraphAuthorizer *authorizer, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserConnectionAsyncData *data;

        g_return_if_fail (GFBGRAPH_IS_USER (user));
        g_return_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer));
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
        g_return_if_fail (callback != NULL);

        simple_async = g_simple_async_result_new (G_OBJECT (user), callback, user_data, gfbgraph_user_get_albums_async);
        g_simple_async_result_set_check_cancellable (simple_async, cancellable);

        data = g_slice_new (GFBGraphUserConnectionAsyncData);
        data->nodes = NULL;
        data->authorizer = authorizer;
        g_object_ref (data->authorizer);

        g_simple_async_result_set_op_res_gpointer (simple_async, data, (GDestroyNotify) gfbgraph_user_connection_async_data_free);
        g_simple_async_result_run_in_thread (simple_async, (GSimpleAsyncThreadFunc) gfbgraph_user_get_albums_async_thread, G_PRIORITY_DEFAULT, cancellable);

        g_object_unref (simple_async);
}

/**
 * gfbgraph_user_get_albums_async_finish:
 * @user: a #GFBGraphUser.
 * @result: A #GAsyncResult.
 * @error: (allow-none): An optional #GError, or %NULL.
 *
 * Finishes an asynchronous operation started with
 * gfbgraph_user_get_albums_async().
 *
 * Returns: (element-type GFBGraphAlbum) (transfer full): a newly-allocated #GList of albums owned by the @user.
 **/
GList*
gfbgraph_user_get_albums_async_finish (GFBGraphUser *user, GAsyncResult *result, GError **error)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserConnectionAsyncData *data;

        g_return_val_if_fail (GFBGRAPH_IS_USER (user), NULL);
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (user), gfbgraph_user_get_albums_async), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        simple_async = G_SIMPLE_ASYNC_RESULT (result);

        if (g_simple_async_result_propagate_error (simple_async, error))
                return NULL;

        data = (GFBGraphUserConnectionAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);
        return data->nodes;
}

static GHashTable*
gfbgraph_user_get_connection_post_params (GFBGraphConnectable *self, GType node_type)
{
        return g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gfbgraph_user_connectable_iface_init (GFBGraphConnectableInterface *iface)
{
        GHashTable *connections;

        connections = g_hash_table_new (g_str_hash, g_str_equal);
        g_hash_table_insert (connections, (gpointer) g_type_name (GFBGRAPH_TYPE_USER), (gpointer) "friends");

        iface->connections = connections;
        iface->get_connection_post_params = gfbgraph_user_get_connection_post_params;
        iface->parse_connected_data = gfbgraph_connectable_default_parse_connected_data;
}

/**
 * gfbgraph_user_get_friends:
 * @user: a #GFBGraphUser.
 * @authorizer: a #GFBGraphAuthorizer.
 * @error: (allow-none): An optional #GError, or %NULL.
 *
 * Retrieve friends of the @user. This function calls the function
 * ID/friends.
 *
 * Returns: (element-type GFBGraphUser) (transfer full): a
 * newly-allocated #GList with friends owned by the given user.
 **/
GList*
gfbgraph_user_get_friends (GFBGraphUser *user, GFBGraphAuthorizer *authorizer, GError **error)
{
        g_return_val_if_fail (GFBGRAPH_IS_USER (user), NULL);
        g_return_val_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer), NULL);

        return gfbgraph_node_get_connection_nodes (GFBGRAPH_NODE (user), GFBGRAPH_TYPE_USER, authorizer, error);
}

/**
 * gfbgraph_user_get_friends_async:
 * @user: a #GFBGraphUser.
 * @authorizer: a #GFBGraphAuthorizer.
 * @cancellable: (allow-none): An optional #GCancellable object, or %NULL.
 * @callback: (scope async): A #GAsyncReadyCallback to call when the request is completed.
 * @user_data: (closure): The data to pass to @callback.
 *
 * Asynchronously retrieve friends of the @user. See
 * gfbgraph_user_get_friends() for the synchronous version of this
 * call.
 *
 * When the operation is finished, @callback will be called. You can
 * then call gfbgraph_user_get_friends_async_finish() to get the
 * #GList of #GFBGraphUser owned by the @user.
 **/
void
gfbgraph_user_get_friends_async (GFBGraphUser *user, GFBGraphAuthorizer *authorizer, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserConnectionAsyncData *data;

        g_return_if_fail (GFBGRAPH_IS_USER (user));
        g_return_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer));
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
        g_return_if_fail (callback != NULL);

        simple_async = g_simple_async_result_new (G_OBJECT (user), callback, user_data, gfbgraph_user_get_friends_async);
        g_simple_async_result_set_check_cancellable (simple_async, cancellable);

        data = g_slice_new (GFBGraphUserConnectionAsyncData);
        data->nodes = NULL;
        data->authorizer = authorizer;
        g_object_ref (data->authorizer);

        g_simple_async_result_set_op_res_gpointer (simple_async, data, (GDestroyNotify) gfbgraph_user_connection_async_data_free);
        g_simple_async_result_run_in_thread (simple_async, (GSimpleAsyncThreadFunc) gfbgraph_user_get_friends_async_thread, G_PRIORITY_DEFAULT, cancellable);

        g_object_unref (simple_async);
}

/**
 * gfbgraph_user_get_friends_async_finish:
 * @user: a #GFBGraphUser.
 * @result: A #GAsyncResult.
 * @error: (allow-none): An optional #GError, or %NULL.
 *
 * Finishes an asynchronous operation started with
 * gfbgraph_user_get_friends_async().
 *
 * Returns: (element-type GFBGraphUser) (transfer full): a newly-allocated #GList of friends owned by the @user.
 **/
GList*
gfbgraph_user_get_friends_async_finish (GFBGraphUser *user, GAsyncResult *result, GError **error)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserConnectionAsyncData *data;

        g_return_val_if_fail (GFBGRAPH_IS_USER (user), NULL);
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (user), gfbgraph_user_get_friends_async), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        simple_async = G_SIMPLE_ASYNC_RESULT (result);

        if (g_simple_async_result_propagate_error (simple_async, error))
                return NULL;

        data = (GFBGraphUserConnectionAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);
        return data->nodes;
}

static const gchar *
type_param_from_picture_type (GFBGraphPictureType picture_type)
{
        switch (picture_type) {
                case GFBGRAPH_PICTURE_SMALL:
                        return "small";
                case GFBGRAPH_PICTURE_NORMAL:
                        return "normal";
                case GFBGRAPH_PICTURE_ALBUM:
                        return "album";
                case GFBGRAPH_PICTURE_LARGE:
                        return "large";
                case GFBGRAPH_PICTURE_SQUARE:
                        return "square";
        }
        g_return_val_if_reached (NULL);
}

/**
 * gfbgraph_user_get_picture:
 * @user: a #GFBGraphUser.
 * @authorizer: a #GFBGraphAuthorizer.
 * @picture_type: a #GFBGraphPictureType.
 * @error: (allow-none): An optional #GError, or %NULL.
 *
 * Retrieve an appriopriately sized picture (or an avatar) of the
 * @user. This function calls the function ID/picture?redirect=1.
 *
 * Returns: (transfer full): a newly-allocated #GBytes with the raw
 * picture data (likely a JPEG data). You could use
 * gdk_pixbuf_loader_write() to get the #GdkPixbuf out of it.
 **/
GBytes*
gfbgraph_user_get_picture (GFBGraphUser *user, GFBGraphAuthorizer *authorizer, GFBGraphPictureType picture_type, GError **error)
{
        RestProxyCall *rest_call;
        gchar *function_path;
        const gchar *payload;
        goffset payload_len;

        g_return_val_if_fail (GFBGRAPH_IS_USER (user), NULL);
        g_return_val_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer), NULL);

        rest_call = gfbgraph_new_rest_call (authorizer);
        rest_proxy_call_set_method (rest_call, "GET");
        function_path = g_strdup_printf ("%s/%s",
                                         gfbgraph_node_get_id (GFBGRAPH_NODE (user)),
                                         "picture");
        rest_proxy_call_set_function (rest_call, function_path);
        g_free (function_path);
        rest_proxy_call_add_param (rest_call, "redirect", "1");
        rest_proxy_call_add_param (rest_call, "type", type_param_from_picture_type (picture_type));
        if (!rest_proxy_call_sync (rest_call, error)) {
                g_object_unref (rest_call);
                return NULL;
        }

        payload = rest_proxy_call_get_payload (rest_call);
        payload_len = rest_proxy_call_get_payload_length (rest_call);
        /* a goffset (a signed 64bit int) for data length? meh.
         */
        if (payload_len < 0) {
                g_object_unref (rest_call);
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "negative payload length (%" G_GOFFSET_FORMAT ")",
                             payload_len);
                return NULL;
        }

        return g_bytes_new_with_free_func (payload, payload_len, g_object_unref, rest_call);
}

/**
 * gfbgraph_user_get_picture_async:
 * @user: a #GFBGraphUser.
 * @authorizer: a #GFBGraphAuthorizer.
 * @picture_type: a #GFBGraphPictureType.
 * @cancellable: (allow-none): An optional #GCancellable object, or %NULL.
 * @callback: (scope async): A #GAsyncReadyCallback to call when the request is completed.
 * @user_data: (closure): The data to pass to @callback.
 *
 * Asynchronously retrieve an appriopriately sized picture (or an
 * avatar) of the @user. See gfbgraph_user_get_picture() for the
 * synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can
 * then call gfbgraph_user_get_picture_async_finish() to get the
 * #GList of #GFBGraphUser owned by the @user.
 **/
void
gfbgraph_user_get_picture_async (GFBGraphUser *user, GFBGraphAuthorizer *authorizer, GFBGraphPictureType picture_type, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserPictureAsyncData *data;

        g_return_if_fail (GFBGRAPH_IS_USER (user));
        g_return_if_fail (GFBGRAPH_IS_AUTHORIZER (authorizer));
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
        g_return_if_fail (callback != NULL);

        simple_async = g_simple_async_result_new (G_OBJECT (user), callback, user_data, gfbgraph_user_get_picture_async);
        g_simple_async_result_set_check_cancellable (simple_async, cancellable);

        data = g_slice_new (GFBGraphUserPictureAsyncData);
        data->picture = NULL;
        data->authorizer = authorizer;
        data->picture_type = picture_type;
        g_object_ref (data->authorizer);

        g_simple_async_result_set_op_res_gpointer (simple_async, data, (GDestroyNotify) gfbgraph_user_picture_async_data_free);
        g_simple_async_result_run_in_thread (simple_async, (GSimpleAsyncThreadFunc) gfbgraph_user_get_picture_async_thread, G_PRIORITY_DEFAULT, cancellable);

        g_object_unref (simple_async);
}

/**
 * gfbgraph_user_get_picture_async_finish:
 * @user: a #GFBGraphUser.
 * @result: A #GAsyncResult.
 * @error: (allow-none): An optional #GError, or %NULL.
 *
 * Finishes an asynchronous operation started with
 * gfbgraph_user_get_friends_async().
 *
 * Returns: (transfer full): a newly-allocated #GBytes with the raw
 * picture data (likely a JPEG data). You could use
 * gdk_pixbuf_loader_write() to get the #GdkPixbuf out of it.
 **/
GBytes*
gfbgraph_user_get_picture_async_finish (GFBGraphUser *user, GAsyncResult *result, GError **error)
{
        GSimpleAsyncResult *simple_async;
        GFBGraphUserPictureAsyncData *data;
        GBytes *picture;

        g_return_val_if_fail (GFBGRAPH_IS_USER (user), NULL);
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (user), gfbgraph_user_get_picture_async), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        simple_async = G_SIMPLE_ASYNC_RESULT (result);

        if (g_simple_async_result_propagate_error (simple_async, error))
                return NULL;

        data = (GFBGraphUserPictureAsyncData *) g_simple_async_result_get_op_res_gpointer (simple_async);
        picture = data->picture;
        data->picture = NULL;
        return picture;
}

/**
 * gfbgraph_user_get_name:
 * @user: a #GFBGraphUser.
 *
 * Get the user full name.
 *
 * Returns: (transfer none): a const #gchar with the user full name, or %NULL.
 **/
const gchar*
gfbgraph_user_get_name (GFBGraphUser *user)
{
        g_return_val_if_fail (GFBGRAPH_IS_USER (user), NULL);

        return user->priv->name;
}
