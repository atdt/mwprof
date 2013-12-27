/**
 * Author: Domas Mituzas ( http://dammit.lt/ )
 * Author: Asher Feldman ( afeldman@wikimedia.org )
 * Author: Tim Starling ( tstarling@wikimedia.org )
 * Author: Ori Livneh ( ori@wikimedia.org)
 *
 * License: public domain (as if there's something to protect ;-)
 */

#ifndef MWPROF_H_
#define MWPROF_H_

#include <stdio.h>
#include <glib.h>
#define POINTS 300

extern GHashTable *table;
G_LOCK_EXTERN(table);

typedef struct CallStats {
    GMutex mutex;
    gulong count;
    gdouble cpu;
    gdouble cpu_sq;
    gdouble real;
    gdouble real_sq;
    gint real_pointer;
    gdouble reals[POINTS];
} CallStats;

void dumpData(FILE *fd);
void truncateData();
void handleMessage(gchar *buffer);
void updateEntry(char *db, char *host, char *task, CallStats *sample);

#endif  // MWPROF_H_
