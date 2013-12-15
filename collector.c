/**
 * Author: Domas Mituzas ( http://dammit.lt/ )
 * Author: Asher Feldman ( afeldman@wikimedia.org )
 * Author: Tim Starling ( tstarling@wikimedia.org )
 * Author: Ori Livneh ( ori@wikimedia.org)
 *
 * License: public domain (as if there's something to protect ;-)
 */
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "mwprof.h"

/* Decides what to do with sample UDP message */
void
handleMessage(gchar *buffer, gssize length) {
    gchar *p, *pp;
    gchar host[128];
    gchar db[128];
    gchar task[1024];
    gchar stats[] = "stats/";
    gint n_fields;

    CallStats sample;
    /* db host count cpu cpusq real realsq eventdescription */
    const char msgformat[]="%127s %127s %ld %lf %lf %lf %lf %1023[^\n]";

    buffer[length] = 0;
    pp = buffer;

    while (TRUE) {
        p = strsep(&pp, "\r\n");

        if (p == NULL) {
            break;
        } else if (p[0] == '\0') {
            continue;
        }

        if (strcmp("-truncate", p) == 0) {
            truncateData();
            return;
        }

        memset(&sample, '\0', sizeof(sample));
        n_fields = sscanf(p, msgformat, (char *) &db, (char *) &host,
                          &sample.count, &sample.cpu, &sample.cpu_sq,
                          &sample.real, &sample.real_sq, (char *) &task);

        if (n_fields < 7) {
            continue;
        }

        // Update the DB-specific entry
        updateEntry(db, host, task, &sample);

        // Update the aggregate entry
        if (strncmp(db, stats, sizeof(stats)-1) == 0) {
            updateEntry("stats/all", "-", task, &sample);
        } else {
            updateEntry("all", "-", task, &sample);
        }
    }
}

void
updateEntry(gchar *db, gchar *host, gchar *task, CallStats *sample) {
    gchar key[1500];
    CallStats *entry;

    snprintf(key, sizeof(key) - 1, "%s:%s:%s", db, host, task);

    entry = g_hash_table_lookup(table, key);
    if (entry == NULL) {
        entry = g_malloc0(sizeof(CallStats));
        g_mutex_init(&entry->mutex);
        g_mutex_lock(&entry->mutex);
        G_LOCK(table);
        g_hash_table_insert(table, g_strdup(key), entry);
        G_UNLOCK(table);
    } else if (entry->real_pointer == POINTS) {
        g_mutex_lock(&entry->mutex);
        entry->real_pointer = 0;
    }

    entry->count += sample->count;
    entry->cpu += sample->cpu;
    entry->cpu_sq += sample->cpu_sq;
    entry->real += sample->real;
    entry->real_sq += sample->real_sq;
    entry->reals[entry->real_pointer] = sample->real;
    entry->real_pointer++;

    g_mutex_unlock(&entry->mutex);
}

void
truncateData() {
    G_LOCK(table);
    g_hash_table_remove_all(table);
    G_UNLOCK(table);
}

void
dumpData(FILE *fd) {
    GList *keys;
    gchar **tokens;
    gchar *key;
    gchar db[128] = "", prev_db[128] = "", host[128] = "", prev_host[128] = "";
    gint in_db = 0, in_host = 0;
    gint i, points;
    CallStats *entry;

    fprintf(fd, "<pfdump>\n");

    G_LOCK(table);
    keys = g_hash_table_get_keys(table);
    keys = g_list_sort(keys, (GCompareFunc) strcmp);
    for (; keys != NULL; keys = g_list_next(keys)) {
        key = keys->data;
        entry = g_hash_table_lookup(table, key);
        g_mutex_lock(&entry->mutex);

        tokens = g_strsplit(key, ":", 3);
        g_assert(g_strv_length(tokens) == 3);
        g_strlcpy(db, tokens[0], 128);
        g_strlcpy(host, tokens[1], 128);
        g_strfreev(tokens);

        /* Get DB */
        if (g_strcmp0(db, prev_db)) {
            if (in_db) {
                fprintf(fd, "</host></db>");
                in_host = 0;
                prev_host[0] = 0;
            }
            fprintf(fd, "<db name=\"%s\">\n", db);
            g_strlcpy(prev_db, db, 128);
            in_db++;
        }

        /* Get Host/Context */
        if (g_strcmp0(host, prev_host)) {
            if (in_host) {
                fprintf(fd, "</host>\n");
            }
            fprintf(fd, "<host name=\"%s\">\n", host);
            g_strlcpy(prev_host, host, 128);
            in_host++;
        }

        /* Get EVENT */
        fprintf(fd, "<event>\n" \
                "<eventname><![CDATA[%s]]></eventname>\n" \
                "<stats count=\"%lu\">\n" \
                "<cputime total=\"%lf\" totalsq=\"%lf\" />\n" \
                "<realtime total=\"%lf\" totalsq=\"%lf\" />\n" \
                "<samples real=\"",
                key,
                entry->count, entry->cpu, entry->cpu_sq,
                entry->real, entry->real_sq);

        if (entry->count >= POINTS) {
            points = POINTS;
        } else {
            points = entry->count;
        }

        for (i = 0; i < points - 1; i++) {
            fprintf(fd, "%lf ", entry->reals[i]);
        }

        fprintf(fd, "%lf", entry->reals[points-1]);
        fprintf(fd, "\" />\n</stats></event>\n");
        g_mutex_unlock(&entry->mutex);
    }
    g_list_free(keys);
    fprintf(fd, "</host>\n</db>\n</pfdump>\n");
    G_UNLOCK(table);
}
