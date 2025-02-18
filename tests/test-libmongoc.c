/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <bson.h>
#include <mongoc.h>
#include <mongoc-host-list-private.h>

#include "mongoc-server-description.h"
#include "mongoc-server-description-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-client-private.h"
#include "mongoc-uri-private.h"
#include "mongoc-util-private.h"

#include "mongoc-tests.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"


extern void test_array_install                   (TestSuite *suite);
extern void test_async_install                   (TestSuite *suite);
extern void test_buffer_install                  (TestSuite *suite);
extern void test_bulk_install                    (TestSuite *suite);
extern void test_client_install                  (TestSuite *suite);
extern void test_client_pool_install             (TestSuite *suite);
extern void test_cluster_install                 (TestSuite *suite);
extern void test_collection_install              (TestSuite *suite);
extern void test_collection_find_install         (TestSuite *suite);
extern void test_command_monitoring_install      (TestSuite *suite);
extern void test_cursor_install                  (TestSuite *suite);
extern void test_database_install                (TestSuite *suite);
extern void test_exhaust_install                 (TestSuite *suite);
extern void test_find_and_modify_install         (TestSuite *suite);
extern void test_gridfs_file_page_install        (TestSuite *suite);
extern void test_gridfs_install                  (TestSuite *suite);
extern void test_list_install                    (TestSuite *suite);
extern void test_log_install                     (TestSuite *suite);
extern void test_matcher_install                 (TestSuite *suite);
extern void test_queue_install                   (TestSuite *suite);
extern void test_read_prefs_install              (TestSuite *suite);
extern void test_rpc_install                     (TestSuite *suite);
extern void test_sdam_install                    (TestSuite *suite);
extern void test_sasl_install                    (TestSuite *suite);
extern void test_server_selection_install        (TestSuite *suite);
extern void test_server_selection_errors_install (TestSuite *suite);
extern void test_set_install                     (TestSuite *suite);
extern void test_socket_install                  (TestSuite *suite);
extern void test_stream_install                  (TestSuite *suite);
extern void test_thread_install                  (TestSuite *suite);
extern void test_topology_install                (TestSuite *suite);
extern void test_topology_reconcile_install      (TestSuite *suite);
extern void test_topology_scanner_install        (TestSuite *suite);
extern void test_uri_install                     (TestSuite *suite);
extern void test_usleep_install                  (TestSuite *suite);
extern void test_util_install                    (TestSuite *suite);
extern void test_version_install                 (TestSuite *suite);
extern void test_write_command_install           (TestSuite *suite);
extern void test_write_concern_install           (TestSuite *suite);
#ifdef MONGOC_ENABLE_SSL
extern void test_x509_install                    (TestSuite *suite);
#endif
#ifdef MONGOC_ENABLE_OPENSSL
extern void test_stream_tls_install              (TestSuite *suite);
extern void test_stream_tls_error_install        (TestSuite *suite);
#endif


static int gSuppressCount;
#ifdef MONGOC_ENABLE_OPENSSL
static mongoc_ssl_opt_t gSSLOptions;
#endif


void
suppress_one_message (void)
{
   gSuppressCount++;
}


static void
log_handler (mongoc_log_level_t  log_level,
             const char         *log_domain,
             const char         *message,
             void               *user_data)
{
   if (log_level < MONGOC_LOG_LEVEL_INFO) {
      if (gSuppressCount) {
         gSuppressCount--;
         return;
      }
      mongoc_log_default_handler (log_level, log_domain, message, NULL);
   }
}


mongoc_database_t *
get_test_database (mongoc_client_t *client)
{
   return mongoc_client_get_database (client, "test");
}


char *
gen_collection_name (const char *str)
{
   return bson_strdup_printf ("%s_%u_%u",
                              str,
                              (unsigned)time(NULL),
                              (unsigned)gettestpid());

}


mongoc_collection_t *
get_test_collection (mongoc_client_t *client,
                     const char      *prefix)
{
   mongoc_collection_t *ret;
   char *str;

   str = gen_collection_name (prefix);
   ret = mongoc_client_get_collection (client, "test", str);
   bson_free (str);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_env --
 *
 *       Get the value of an environment variable.
 *
 * Returns:
 *       A string you must bson_free, or NULL if the variable is not set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_getenv (const char *name)
{
#ifdef _MSC_VER
   char buf[1024];
   size_t buflen;

   if ((0 == getenv_s (&buflen, buf, sizeof buf, name)) && buflen) {
      return bson_strdup (buf);
   } else {
      return NULL;
   }
#else

   if (getenv (name) && strlen (getenv (name))) {
      return bson_strdup (getenv (name));
   } else {
      return NULL;
   }

#endif
}

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_getenv_bool --
 *
 *       Check if an environment variable is set.
 *
 * Returns:
 *       True if the variable is set, or set to "on", false if it is not set
 *       or set to "off".
 *
 * Side effects:
 *       Logs and aborts if there is another value like "yes" or "true".
 *
 *--------------------------------------------------------------------------
 */
bool
test_framework_getenv_bool (const char *name)
{
   char *value = test_framework_getenv (name);
   bool ret = false;

   if (value) {
      if (!strcasecmp (value, "off")) {
         ret = false;
      } else if (!strcasecmp (value, "") || !strcasecmp (value, "on")) {
         ret  = true;
      } else {
         fprintf (stderr,
                  "Unrecognized value for %s: \"%s\". Use \"on\" or \"off\".\n",
                  name, value);
         abort ();
      }
   }

   bson_free (value);
   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_getenv_int64 --
 *
 *       Get a number from an environment variable.
 *
 * Returns:
 *       The number, or default.
 *
 * Side effects:
 *       Logs and aborts if there is a non-numeric value.
 *
 *--------------------------------------------------------------------------
 */
int64_t
test_framework_getenv_int64 (const char *name,
                             int64_t default_value)
{
   char *value = test_framework_getenv (name);
   char *endptr;
   int64_t ret;

   if (value) {
      errno = 0;
      ret = bson_ascii_strtoll (value, &endptr, 10);
      if (errno) {
         perror (bson_strdup_printf ("Parsing %s from environment", name));
         abort ();
      }

      bson_free (value);
      return ret;
   }

   return default_value;
}


static char *
test_framework_get_unix_domain_socket_path (void)
{
   char *path = test_framework_getenv ("MONGOC_TEST_UNIX_DOMAIN_SOCKET");

   if (path) {
      return path;
   }

   return bson_strdup_printf ("/tmp/mongodb-%d.sock",
         test_framework_get_port());
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_unix_domain_socket_path_escaped --
 *
 *       Get the path to Unix Domain Socket .sock of the test MongoDB server,
 *       URI-escaped ("/" is replaced with "%2F").
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_get_unix_domain_socket_path_escaped (void)
{
   char *path = test_framework_get_unix_domain_socket_path (), *c = path;
   bson_string_t *escaped = bson_string_new (NULL);

   /* Connection String Spec: "The host information cannot contain an unescaped
    * slash ("/"), if it does then an exception MUST be thrown informing users
    * that paths must be URL encoded."
    *
    * Even though the C Driver does not currently enforce the spec, let's pass
    * a correctly escaped URI.
    */
   do {
      if (*c == '/') {
         bson_string_append (escaped, "%2F");
      } else {
         bson_string_append_c (escaped, *c);
      }
   } while (*(++c));

   bson_string_append_c (escaped, '\0');
   bson_free (path);

   return bson_string_free (escaped, false /* free_segment */);
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_host --
 *
 *       Get the hostname of the test MongoDB server.
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_get_host (void)
{
   char *host = test_framework_getenv ("MONGOC_TEST_HOST");

   return host ? host : bson_strdup ("localhost");
}

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_port --
 *
 *       Get the port number of the test MongoDB server.
 *
 * Returns:
 *       The port number, 27017 by default.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
uint16_t
test_framework_get_port (void)
{
   char *port_str = test_framework_getenv ("MONGOC_TEST_PORT");
   unsigned long port = MONGOC_DEFAULT_PORT;

   if (port_str && strlen (port_str)) {
      port = strtoul (port_str, NULL, 10);
      if (port == 0 || port > UINT16_MAX) {
         /* parse err or port out of range -- mongod prohibits port 0 */
         port = MONGOC_DEFAULT_PORT;
      }
   }

   bson_free (port_str);

   return (uint16_t) port;
}

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_host_list --
 *
 *       Get the single host and port of the test server (not actually a
 *       list).
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
test_framework_get_host_list (mongoc_host_list_t *host_list)
{
   char *host = test_framework_get_host ();
   uint16_t port = test_framework_get_port ();
   char *host_and_port = bson_strdup_printf ("%s:%hu", host, port);

   _mongoc_host_list_from_string (host_list, host_and_port);

   bson_free (host_and_port);
   bson_free (host);
}

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_admin_user --
 *
 *       Get the username of an admin user on the test MongoDB server.
 *
 * Returns:
 *       A string you must bson_free, or NULL.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_get_admin_user (void)
{
   return test_framework_getenv ("MONGOC_TEST_USER");
}

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_admin_password --
 *
 *       Get the password of an admin user on the test MongoDB server.
 *
 * Returns:
 *       A string you must bson_free, or NULL.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_get_admin_password (void)
{
   return test_framework_getenv ("MONGOC_TEST_PASSWORD");
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_user_password --
 *
 *       Get the username and password of an admin user on the test MongoDB
 *       server.
 *
 * Returns:
 *       True if username and password environment variables are set.
 *
 * Side effects:
 *       Sets passed-in string pointers to strings you must free, or NULL.
 *       Logs and aborts if user or password is set in the environment
 *       but not both, or if user and password are set but SSL is not
 *       compiled in (SSL is required for SCRAM-SHA-1, see CDRIVER-520).
 *
 *--------------------------------------------------------------------------
 */
static bool
test_framework_get_user_password (char **user,
                                  char **password)
{
   /* TODO: uri-escape username and password */
   *user = test_framework_get_admin_user ();
   *password = test_framework_get_admin_password ();

   if ((*user && !*password) || (!*user && *password)) {
      fprintf (stderr, "Specify both MONGOC_TEST_USER and"
         " MONGOC_TEST_PASSWORD, or neither\n");
      abort ();
   }

#ifndef MONGOC_ENABLE_CRYPTO
   if (*user && *password) {
      fprintf (stderr, "You need to configure with --enable-ssl"
                       " when providing user+password (for SCRAM-SHA-1)\n");
      abort ();
   }
#endif

   return *user != NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_add_user_password --
 *
 *       Copy a connection string, with user and password added.
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_add_user_password (const char *uri_str,
                                  const char *user,
                                  const char *password)
{
   return bson_strdup_printf (
      "mongodb://%s:%s@%s",
      user, password, uri_str + strlen ("mongodb://"));
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_add_user_password_from_env --
 *
 *       Add password of an admin user on the test MongoDB server.
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       Same as test_framework_get_user_password.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_add_user_password_from_env (const char *uri_str)
{
   char *user;
   char *password;
   char *uri_str_auth;

   if (test_framework_get_user_password (&user, &password)) {
      uri_str_auth = test_framework_add_user_password (uri_str,
                                                       user,
                                                       password);

      bson_free (user);
      bson_free (password);
   } else {
      uri_str_auth = bson_strdup (uri_str);
   }

   return uri_str_auth;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_ssl --
 *
 *       Should we connect to the test MongoDB server over SSL?
 *
 * Returns:
 *       True if any MONGOC_TEST_SSL_* environment variables are set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
bool
test_framework_get_ssl (void)
{
   char *ssl_option_names[] = {
      "MONGOC_TEST_SSL_PEM_FILE",
      "MONGOC_TEST_SSL_PEM_PWD",
      "MONGOC_TEST_SSL_CA_FILE",
      "MONGOC_TEST_SSL_CA_DIR",
      "MONGOC_TEST_SSL_CRL_FILE",
      "MONGOC_TEST_SSL_WEAK_CERT_VALIDATION"
   };
   char *ssl_option_value;
   size_t i;

   for (i = 0; i < sizeof ssl_option_names / sizeof (char *); i++) {
      ssl_option_value = test_framework_getenv (ssl_option_names[i]);

      if (ssl_option_value) {
         bson_free (ssl_option_value);
         return true;
      }
   }

   return test_framework_getenv_bool ("MONGOC_TEST_SSL");
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_unix_domain_socket_uri_str --
 *
 *       Get the connection string (unix domain socket style) of the test
 *       MongoDB server based on the variables set in the environment.
 *       Does *not* call isMaster to discover your actual topology.
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       Same as test_framework_get_user_password.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_get_unix_domain_socket_uri_str ()
{
   char *path;
   char *test_uri_str;
   char *test_uri_str_auth;

   path = test_framework_get_unix_domain_socket_path_escaped ();
   test_uri_str = bson_strdup_printf (
      "mongodb://%s/%s",
      path,
      test_framework_get_ssl () ? "?ssl=true" : "");

   test_uri_str_auth = test_framework_add_user_password_from_env (test_uri_str);

   bson_free (path);
   bson_free (test_uri_str);

   return test_uri_str_auth;
}


/*
 *--------------------------------------------------------------------------
 *
 * call_ismaster_with_host_and_port --
 *
 *       Call isMaster on a server, possibly over SSL.
 *
 * Side effects:
 *       Fills reply with ismaster response. Logs and aborts on error.
 *
 *--------------------------------------------------------------------------
 */
static void
call_ismaster_with_host_and_port (char *host,
                                  uint16_t port,
                                  bson_t *reply)
{
   char *uri_str;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   bson_error_t error;

   uri_str = bson_strdup_printf (
      "mongodb://%s:%hu%s",
      host,
      port,
      test_framework_get_ssl () ? "?ssl=true" : "");

   uri = mongoc_uri_new (uri_str);
   assert (uri);
   mongoc_uri_set_option_as_int32 (uri, "connectTimeoutMS", 10000);
   mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 10000);
   mongoc_uri_set_option_as_bool (uri, "serverSelectionTryOnce", false);

   client = mongoc_client_new_from_uri (uri);
   test_framework_set_ssl_opts (client);

   if (!mongoc_client_command_simple (client, "admin",
                                      tmp_bson ("{'isMaster': 1}"),
                                      NULL, reply, &error)) {

      fprintf (stderr, "error calling ismaster: '%s'\n", error.message);
      fprintf (stderr, "URI = %s\n", uri_str);
      abort ();
   }

   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   bson_free (uri_str);
}

/*
 *--------------------------------------------------------------------------
 *
 * call_ismaster --
 *
 *       Call isMaster on the test server, possibly over SSL, using host
 *       and port from the environment.
 *
 * Side effects:
 *       Fills reply with ismaster response. Logs and aborts on error.
 *
 *--------------------------------------------------------------------------
 */
static void
call_ismaster (bson_t *reply)
{
   char *host;
   uint16_t port;

   host = test_framework_get_host ();
   port = test_framework_get_port ();

   call_ismaster_with_host_and_port (host, port, reply);

   bson_free (host);
}


static char *
set_name (bson_t *ismaster_response)
{
   bson_iter_t iter;

   if (bson_iter_init_find (&iter, ismaster_response, "setName")) {
      return bson_strdup (bson_iter_utf8 (&iter, NULL));
   } else {
      return NULL;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_uri_str_no_auth --
 *
 *       Get the connection string of the test MongoDB topology --
 *       standalone, replica set, mongos, or mongoses -- along with
 *       SSL options, but not username and password. Calls calls isMaster with
 *       that connection string to discover your topology, and
 *       returns an appropriate connection string for the topology
 *       type.
 *
 *       database_name is optional.
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       Same as test_framework_get_user_password.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_get_uri_str_no_auth (const char *database_name)
{
   bson_t ismaster_response;
   bson_string_t *uri_string;
   char *name;
   bson_iter_t iter;
   bson_iter_t hosts_iter;
   bool first;
   char *host;
   uint16_t port;

   call_ismaster (&ismaster_response);
   uri_string = bson_string_new ("mongodb://");

   if ((name = set_name (&ismaster_response))) {
      /* make a replica set URI */
      bson_iter_init_find (&iter, &ismaster_response, "hosts");
      bson_iter_recurse (&iter, &hosts_iter);
      first = true;

      /* append "host1,host2,host3" */
      while (bson_iter_next (&hosts_iter)) {
         assert (BSON_ITER_HOLDS_UTF8 (&hosts_iter));
         if (!first) {
            bson_string_append (uri_string, ",");
         }

         bson_string_append (uri_string, bson_iter_utf8 (&hosts_iter, NULL));
         first = false;
      }

      bson_string_append (uri_string, "/");
      if (database_name) {
         bson_string_append (uri_string, database_name);
      }

      bson_string_append_printf (uri_string, "?replicaSet=%s", name);
      bson_free (name);
   } else {
      host = test_framework_get_host ();
      port = test_framework_get_port ();
      bson_string_append_printf (uri_string, "%s:%hu", host, port);
      bson_string_append (uri_string, "/");
      if (database_name) {
         bson_string_append (uri_string, database_name);
      }

      bson_free (host);
   }

   if (test_framework_get_ssl ()) {
      if (name) {
         /* string ends with "?replicaSet=name" */
         bson_string_append (uri_string, "&ssl=true");
      } else {
         /* string ends with "/" or "/dbname" */
         bson_string_append (uri_string, "?ssl=true");
      }
   }

   bson_destroy (&ismaster_response);

   return bson_string_free (uri_string, false);
}

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_uri_str --
 *
 *       Get the connection string of the test MongoDB topology --
 *       standalone, replica set, mongos, or mongoses -- along with
 *       SSL options, username and password.
 *
 * Returns:
 *       A string you must bson_free.
 *
 * Side effects:
 *       Same as test_framework_get_user_password.
 *
 *--------------------------------------------------------------------------
 */
char *
test_framework_get_uri_str ()
{
   char *uri_str_no_auth;
   char *uri_str;

   uri_str_no_auth = test_framework_get_uri_str_no_auth (NULL);
   uri_str = test_framework_add_user_password_from_env (uri_str_no_auth);

   bson_free (uri_str_no_auth);

   return uri_str;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_uri --
 *
 *       Like test_framework_get_uri_str (). Get the URI of the test
 *       MongoDB server.
 *
 * Returns:
 *       A mongoc_uri_t* you must destroy.
 *
 * Side effects:
 *       Same as test_framework_get_user_password.
 *
 *--------------------------------------------------------------------------
 */
mongoc_uri_t *
test_framework_get_uri ()
{
   char *test_uri_str = test_framework_get_uri_str ();
   mongoc_uri_t *uri = mongoc_uri_new (test_uri_str);

   assert (uri);
   bson_free (test_uri_str);

   return uri;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_mongos_count --
 *
 *       Returns the number of servers in the test framework's MongoDB URI.
 *
 *--------------------------------------------------------------------------
 */

size_t
test_framework_mongos_count (void)
{
   mongoc_uri_t *uri = test_framework_get_uri ();
   const mongoc_host_list_t *h;
   size_t count = 0;

   assert (uri);
   h = mongoc_uri_get_hosts (uri);
   while (h) {
      ++count;
      h = h->next;
   }

   mongoc_uri_destroy (uri);

   return count;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_replset_member_count --
 *
 *       Returns the number replica set data members (including arbiters).
 *
 *--------------------------------------------------------------------------
 */

size_t
test_framework_replset_member_count (void)
{
   mongoc_client_t *client;
   bson_t reply;
   bson_error_t error;
   bool r;
   bson_iter_t iter, array;
   size_t count = 0;

   client = test_framework_client_new ();
   r = mongoc_client_command_simple (client, "admin",
                                     tmp_bson ("{'replSetGetStatus': 1}"), NULL,
                                     &reply, &error);

   if (r) {
      if (bson_iter_init_find (&iter, &reply, "members") &&
          BSON_ITER_HOLDS_ARRAY (&iter)) {
         bson_iter_recurse (&iter, &array);
         while (bson_iter_next (&array)) {
            ++count;
         }
      }
   } else if (!strstr (error.message, "not running with --replSet") &&
              !strstr (error.message, "replSetGetStatus is not supported through mongos"))
   {
      /* failed for some other reason */
      ASSERT_OR_PRINT (false, error);
   }

   bson_destroy (&reply);
   mongoc_client_destroy (client);

   return count;
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_server_count --
 *
 *       Returns the number of mongos servers or replica set members,
 *       or 1 if the server is standalone.
 *
 *--------------------------------------------------------------------------
 */
size_t
test_framework_server_count (void)
{
   size_t count = 0;

   count = test_framework_replset_member_count ();
   if (count > 0) {
      return count;
   }

   return test_framework_mongos_count ();
}

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_set_ssl_opts --
 *
 *       Configure a client to connect to the test MongoDB server.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Logs and aborts if any MONGOC_TEST_SSL_* environment variables are
 *       set but the driver is not built with SSL enabled.
 *
 *--------------------------------------------------------------------------
 */
void
test_framework_set_ssl_opts (mongoc_client_t *client)
{
   assert (client);

   if (test_framework_get_ssl ()) {
#ifndef MONGOC_ENABLE_OPENSSL
      fprintf (stderr,
               "SSL test config variables are specified in the environment, but"
               " SSL isn't enabled\n");
      abort ();
#else
      mongoc_client_set_ssl_opts (client, &gSSLOptions);
#endif
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_client_new --
 *
 *       Get a client connected to the test MongoDB topology.
 *
 * Returns:
 *       A client you must mongoc_client_destroy.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
mongoc_client_t *
test_framework_client_new ()
{
   char *test_uri_str = test_framework_get_uri_str ();
   mongoc_client_t *client = mongoc_client_new (test_uri_str);

   assert (client);
   test_framework_set_ssl_opts (client);

   bson_free (test_uri_str);

   return client;
}


#ifdef MONGOC_ENABLE_OPENSSL
/*
 *--------------------------------------------------------------------------
 *
 * test_framework_get_ssl_opts --
 *
 *       Get options for connecting to mongod over SSL (even if mongod
 *       isn't actually SSL-enabled).
 *
 * Returns:
 *       A pointer to constant global SSL-test options.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
const mongoc_ssl_opt_t *
test_framework_get_ssl_opts (void)
{
   return &gSSLOptions;
}
#endif

/*
 *--------------------------------------------------------------------------
 *
 * test_framework_set_pool_ssl_opts --
 *
 *       Configure a client pool to connect to the test MongoDB server.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Logs and aborts if any MONGOC_TEST_SSL_* environment variables are
 *       set but the driver is not built with SSL enabled.
 *
 *--------------------------------------------------------------------------
 */
void
test_framework_set_pool_ssl_opts (mongoc_client_pool_t *pool)
{
   assert (pool);

   if (test_framework_get_ssl ()) {
#ifndef MONGOC_ENABLE_OPENSSL
      fprintf (stderr,
               "SSL test config variables are specified in the environment, but"
                     " SSL isn't enabled\n");
      abort ();
#else
      mongoc_client_pool_set_ssl_opts (pool, &gSSLOptions);
#endif
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * test_framework_pool_new --
 *
 *       Get a client pool connected to the test MongoDB topology.
 *
 * Returns:
 *       A pool you must destroy.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
mongoc_client_pool_t *
test_framework_client_pool_new ()
{
   mongoc_uri_t *test_uri = test_framework_get_uri ();
   mongoc_client_pool_t *pool = mongoc_client_pool_new (test_uri);

   assert (pool);
   test_framework_set_pool_ssl_opts (pool);

   mongoc_uri_destroy (test_uri);
   assert (pool);
   return pool;
}

#ifdef MONGOC_ENABLE_OPENSSL
static void
test_framework_global_ssl_opts_init (void)
{
   memcpy (&gSSLOptions, mongoc_ssl_opt_get_default (), sizeof gSSLOptions);

   gSSLOptions.pem_file = test_framework_getenv ("MONGOC_TEST_SSL_PEM_FILE");
   gSSLOptions.pem_pwd = test_framework_getenv ("MONGOC_TEST_SSL_PEM_PWD");
   gSSLOptions.ca_file = test_framework_getenv ("MONGOC_TEST_SSL_CA_FILE");
   gSSLOptions.ca_dir = test_framework_getenv ("MONGOC_TEST_SSL_CA_DIR");
   gSSLOptions.crl_file = test_framework_getenv ("MONGOC_TEST_SSL_CRL_FILE");
   gSSLOptions.weak_cert_validation = test_framework_getenv_bool (
      "MONGOC_TEST_SSL_WEAK_CERT_VALIDATION");
}

static void
test_framework_global_ssl_opts_cleanup (void)
{
   bson_free ((void *)gSSLOptions.pem_file);
   bson_free ((void *)gSSLOptions.pem_pwd);
   bson_free ((void *)gSSLOptions.ca_file);
   bson_free ((void *)gSSLOptions.ca_dir);
   bson_free ((void *)gSSLOptions.crl_file);
}
#endif


bool
test_framework_is_mongos (void)
{
   bson_t reply;
   bson_iter_t iter;
   bool is_mongos;

   call_ismaster (&reply);

   is_mongos = (bson_iter_init_find (&iter, &reply, "msg")
                && BSON_ITER_HOLDS_UTF8 (&iter)
                && !strcasecmp (bson_iter_utf8 (&iter, NULL), "isdbgrid"));

   bson_destroy (&reply);

   return is_mongos;
}


bool
test_framework_is_replset (void)
{
   return test_framework_replset_member_count() > 0;
}

bool
test_framework_server_is_secondary (mongoc_client_t *client,
                                    uint32_t server_id)
{
   bson_t reply;
   bson_iter_t iter;
   mongoc_server_description_t *sd;
   bson_error_t error;
   bool ret;

   sd = mongoc_topology_server_by_id (client->topology, server_id, &error);
   ASSERT_OR_PRINT (sd, error);

   call_ismaster_with_host_and_port (sd->host.host, sd->host.port, &reply);

   ret = bson_iter_init_find (&iter, &reply, "secondary") &&
         bson_iter_as_bool (&iter);

   mongoc_server_description_destroy (sd);

   return ret;
}


int
test_framework_skip_if_windows (void)
{
#ifdef _WIN32
   return false;
#else
   return true;
#endif
}


/* skip if no Unix domain socket */
int
test_framework_skip_if_no_uds (void)
{
#ifdef _WIN32
   return 0;
#else
   char *path;
   int ret;

   path = test_framework_get_unix_domain_socket_path ();
   ret = access (path, R_OK|W_OK) == 0 ? 1 : 0;

   bson_free (path);

   return ret;
#endif
}


bool
test_framework_max_wire_version_at_least (int version)
{
   bson_t reply;
   bson_iter_t iter;
   bool at_least;

   call_ismaster (&reply);

   at_least = (bson_iter_init_find (&iter, &reply, "maxWireVersion")
                && bson_iter_as_int64 (&iter) >= version);

   bson_destroy (&reply);

   return at_least;
}

#define N_SERVER_VERSION_PARTS 4

static server_version_t
_parse_server_version (const bson_t *buildinfo)
{
   bson_iter_t iter;
   bson_iter_t array_iter;
   int i;
   server_version_t ret = 0;

   ASSERT (bson_iter_init_find (&iter, buildinfo, "versionArray"));
   ASSERT (bson_iter_recurse (&iter, &array_iter));

   /* Server returns a 4-part version like [3, 2, 0, 0], or like [3, 2, 0, -49]
    * for an RC. Bail if number of parts is ever not 4. */
   i = 0;
   while (bson_iter_next (&array_iter)) {
      ret *= 1000;
      ret += 100 + bson_iter_as_int64 (&array_iter);
      i++;
      ASSERT_CMPINT (i, <=, N_SERVER_VERSION_PARTS);
   }

   ASSERT_CMPINT (i, ==, N_SERVER_VERSION_PARTS);

   return ret;
}

server_version_t
test_framework_get_server_version (void)
{
   mongoc_client_t *client;
   bson_t reply;
   bson_error_t error;
   server_version_t ret = 0;

   client = test_framework_client_new ();
   ASSERT_OR_PRINT (mongoc_client_command_simple (
      client, "admin", tmp_bson ("{'buildinfo': 1}"),
      NULL, &reply, &error), error);

   ret = _parse_server_version (&reply);

   bson_destroy (&reply);
   mongoc_client_destroy (client);

   return ret;
}

server_version_t
test_framework_str_to_version (const char *version_str)
{
   char *str_copy;
   char *part;
   char *end;
   int i;
   server_version_t ret = 0;

   str_copy = bson_strdup (version_str);
   i = 0;
   part = strtok (str_copy, ".");
   while (part) {
      ret *= 1000;

      /* add 100 since release candidates have versions like "3.2.0.-49" */
      ret += 100 + bson_ascii_strtoll (part, &end, 10);
      i++;
      ASSERT_CMPINT (i, <=, N_SERVER_VERSION_PARTS);
      part = strtok (NULL, ".");
   }

   /* pad out a short version like "3.0" */
   for (; i < N_SERVER_VERSION_PARTS; i++) {
      ret *= 1000;
      ret += 100;
   }

   bson_free (str_copy);

   return ret;
}

/* self-tests for a test framework feature */
static void
test_version_cmp (void)
{
   server_version_t v2_6_12        = 102106112100;
   server_version_t v3_0_0         = 103100100100;
   server_version_t v3_0_1         = 103100101100;
   server_version_t v3_0_10        = 103100110100;
   server_version_t v3_2_0_rc1_pre = 103102100051;

   ASSERT (v2_6_12 == test_framework_str_to_version ("2.6.12"));
   ASSERT (v2_6_12 == _parse_server_version (
      tmp_bson ("{'versionArray': [2, 6, 12, 0]}")));

   ASSERT (v3_0_0 == test_framework_str_to_version ("3"));
   ASSERT (v3_0_0 == _parse_server_version (
      tmp_bson ("{'versionArray': [3, 0, 0, 0]}")));

   ASSERT (v3_0_1 == test_framework_str_to_version ("3.0.1"));
   ASSERT (v3_0_1 == _parse_server_version (
      tmp_bson ("{'versionArray': [3, 0, 1, 0]}")));

   ASSERT (v3_0_10 == test_framework_str_to_version ("3.0.10"));
   ASSERT (v3_0_10 == _parse_server_version (
      tmp_bson ("{'versionArray': [3, 0, 10, 0]}")));

   ASSERT (v3_2_0_rc1_pre == test_framework_str_to_version ("3.2.0.-49"));
   ASSERT (v3_2_0_rc1_pre == _parse_server_version (
      tmp_bson ("{'versionArray': [3, 2, 0, -49]}")));

   ASSERT (v3_2_0_rc1_pre > test_framework_str_to_version ("3.1.9"));
   ASSERT (v3_2_0_rc1_pre < test_framework_str_to_version ("3.2"));
}

int
test_framework_skip_if_single (void)
{
   return (test_framework_is_mongos () || test_framework_is_replset());
}

int
test_framework_skip_if_mongos (void)
{
   return test_framework_is_mongos() ? 0 : 1;
}

int
test_framework_skip_if_replset (void)
{
   return test_framework_is_replset() ? 0 : 1;
}

int
test_framework_skip_if_not_single (void)
{
   return !test_framework_skip_if_single ();
}

int
test_framework_skip_if_not_mongos (void)
{
   return !test_framework_skip_if_mongos ();
}

int
test_framework_skip_if_not_replset (void)
{
   return !test_framework_skip_if_replset ();
}

int test_framework_skip_if_max_version_version_less_than_2 (void)
{
   return test_framework_max_wire_version_at_least (2);
}

int test_framework_skip_if_max_version_version_less_than_4 (void)
{
   return test_framework_max_wire_version_at_least (4);
}

static char MONGOC_TEST_UNIQUE [32];

int
main (int   argc,
      char *argv[])
{
   TestSuite suite;
   int ret;

   mongoc_init ();

   bson_snprintf (MONGOC_TEST_UNIQUE, sizeof MONGOC_TEST_UNIQUE,
                  "test_%u_%u", (unsigned)time (NULL),
                  (unsigned)gettestpid ());

   mongoc_log_set_handler (log_handler, NULL);

#ifdef MONGOC_ENABLE_OPENSSL
   test_framework_global_ssl_opts_init ();
   atexit (test_framework_global_ssl_opts_cleanup);
#endif

   TestSuite_Init (&suite, "", argc, argv);
   TestSuite_Add (&suite, "/TestSuite/version_cmp", test_version_cmp);

   test_array_install (&suite);
   test_async_install (&suite);
   test_buffer_install (&suite);
   test_client_install (&suite);
   test_client_pool_install (&suite);
   test_write_command_install (&suite);
   test_bulk_install (&suite);
   test_cluster_install (&suite);
   test_collection_install (&suite);
   test_collection_find_install (&suite);
   test_command_monitoring_install (&suite);
   test_cursor_install (&suite);
   test_database_install (&suite);
   test_exhaust_install (&suite);
   test_find_and_modify_install (&suite);
   test_gridfs_install (&suite);
   test_gridfs_file_page_install (&suite);
   test_list_install (&suite);
   test_log_install (&suite);
   test_matcher_install (&suite);
   test_queue_install (&suite);
   test_read_prefs_install (&suite);
   test_rpc_install (&suite);
   test_sasl_install (&suite);
   test_socket_install (&suite);
   test_topology_scanner_install (&suite);
   test_topology_reconcile_install (&suite);
   test_sdam_install (&suite);
   test_server_selection_install (&suite);
   test_server_selection_errors_install (&suite);
   test_set_install (&suite);
   test_stream_install (&suite);
   test_thread_install (&suite);
   test_topology_install (&suite);
   test_uri_install (&suite);
   test_usleep_install (&suite);
   test_util_install (&suite);
   test_version_install (&suite);
   test_write_concern_install (&suite);
#ifdef MONGOC_ENABLE_SSL
   test_x509_install (&suite);
#endif
#ifdef MONGOC_ENABLE_OPENSSL
   test_stream_tls_install (&suite);
   test_stream_tls_error_install (&suite);
#endif

   ret = TestSuite_Run (&suite);

   TestSuite_Destroy (&suite);

   mongoc_cleanup();

   return ret;
}
