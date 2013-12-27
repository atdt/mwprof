/**
 * mwprof -- aggregate MediaWiki profiling samples
 *
 * Listen on a UDP port for profiling samples emitted by MediaWiki's
 * ProfilerSimpleUDP. Serve summary stats as XML via TCP on the same port
 * number. The port number is 3811 by default, but a different value may be
 * specified via the "--listen-port" command-line argument.
 *
 * Author: Domas Mituzas ( http://dammit.lt/ )
 * Author: Asher Feldman ( afeldman@wikimedia.org )
 * Author: Tim Starling ( tstarling@wikimedia.org )
 * Author: Ori Livneh ( ori@wikimedia.org)
 *
 * License: public domain (as if there's something to protect ;-)
 */

#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_32

#include <errno.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include "mwprof.h"

GHashTable *table;
G_LOCK_DEFINE(table);

static void listen_stats(gpointer data);
static gboolean serve_xml();

static gint listen_port = 3811;
static GOptionEntry entries[] = {
    { "listen-port", 'p', 0, G_OPTION_ARG_INT, &listen_port,
        "UDP & TCP listen port (default: 3811)", "PORT" },
    { NULL }
};

/* Answers incoming TCP connections with an XML stats dump. */
static gboolean
serve_xml(
    GThreadedSocketService  *service,
    GSocketConnection       *connection,
    GSocketListener         *listener,
    gpointer                user_data
) {
    FILE *temp;
    GInputStream *in;
    GOutputStream *out;

    temp = tmpfile();
    g_assert(temp != NULL);

    dumpData(temp);
    rewind(temp);

    in = g_unix_input_stream_new(fileno(temp), FALSE);
    out = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    g_output_stream_splice(out, in, G_OUTPUT_STREAM_SPLICE_NONE, NULL, NULL);

    g_object_unref(in);
    fclose(temp);

    return FALSE;
}

/* Process profiling samples coming in via UDP. */
static void
listen_stats(gpointer data) {
    GError *error = NULL;
    GSocket *stats_sock;
    GInetAddress *inet_address;
    GSocketAddress *address;
    gint port = GPOINTER_TO_INT(data);
    gchar buf[65535] = {0};
    gssize nbytes;

    stats_sock = g_socket_new(G_SOCKET_FAMILY_IPV6, G_SOCKET_TYPE_DATAGRAM,
                              G_SOCKET_PROTOCOL_UDP, &error);
    g_assert_no_error(error);

    inet_address = g_inet_address_new_any(G_SOCKET_FAMILY_IPV6);
    address = g_inet_socket_address_new(inet_address, port);
    g_object_unref(inet_address);

    g_socket_bind(stats_sock, address, TRUE, &error);
    g_assert_no_error(error);
    g_object_unref(address);

    while (TRUE) {
        nbytes = g_socket_receive(stats_sock, buf, sizeof(buf)-1, NULL, &error);
        g_assert_no_error(error);
        buf[nbytes] = '\0';
        handleMessage(buf);
    }
}

/* Parse command-line arguments. */
static void
parse_args(int argc, char **argv) {
    GOptionContext *context = g_option_context_new(NULL);
    GError *error = NULL;

    g_option_context_set_summary(context,
                                 "Aggregate MediaWiki profiling samples");
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_parse(context, &argc, &argv, &error);
    g_assert_no_error(error);
    g_option_context_free(context);
}

int
main(int argc, char **argv) {
    GThread *listener;
    GSocketService *service;
    GError *error = NULL;

    // Initialize GLib's type and threading system.
    g_type_init();

    parse_args(argc, argv);

    // Hash table used to store stats.
    table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    // Spawn a dedicated worker thread for processing incoming stats.
    listener = g_thread_new("stats listener", (GThreadFunc)listen_stats,
                            GINT_TO_POINTER(listen_port));
    g_thread_unref(listener);

    // Serve XML via a TCP socket service running on GLib's main event loop.
    service = g_threaded_socket_service_new(10);
    g_socket_listener_add_inet_port(G_SOCKET_LISTENER(service), listen_port,
                                    NULL, &error);
    g_assert_no_error(error);
    g_signal_connect(service, "run", G_CALLBACK(serve_xml), NULL);
    g_main_loop_run(g_main_loop_new(NULL, FALSE));

    g_assert_not_reached();
}
