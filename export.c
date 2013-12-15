/*
 Poor man's XML exporter. :)

 Author: Domas Mituzas ( http://dammit.lt/ )

 License: public domain (as if there's something to protect ;-)

 $Id: export.c 111618 2012-02-16 04:08:32Z tstarling $

*/
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <glib.h>
#include "collector.h"


void dumpData(GHashTable *table, FILE *fd) {
    GHashTableIter iter;

    char *p, oldhost[128]="",olddb[128]="",*pp;
    int indb=0,inhost=0;
    int i, points;

    struct pfstats *entry;

    fprintf(fd,"<pfdump>\n");

    g_hash_table_iter_init(&iter, table);
    while(g_hash_table_iter_next(&iter, (gpointer) &p, (gpointer) &entry)) {
        /* Get DB */
        pp=strsep(&p,":");
        if (strcmp(pp,olddb)) {
            if (indb) {
                fprintf(fd,"</host></db>");
                inhost=0;
                oldhost[0]=0;
            }
            fprintf(fd,"<db name=\"%s\">\n",pp);
            g_strlcpy(olddb,pp,128);
            indb++;
        }
        /* Get Host/Context */
        pp=strsep(&p,":");
        if (strcmp(pp,oldhost)) {
            if (inhost)
                fprintf(fd,"</host>\n");
            fprintf(fd,"<host name=\"%s\">\n",pp);
            g_strlcpy(oldhost,pp,128);
            inhost++;
        }
        /* Get EVENT */
        fprintf(fd,"<event>\n" \
                "<eventname><![CDATA[%s]]></eventname>\n" \
                "<stats count=\"%lu\">\n" \
                "<cputime total=\"%lf\" totalsq=\"%lf\" />\n" \
                "<realtime total=\"%lf\" totalsq=\"%lf\" />\n" \
                "<samples real=\"",
                p,
                entry->pf_count, entry->pf_cpu, entry->pf_cpu_sq,
                entry->pf_real, entry->pf_real_sq);
        if (entry->pf_count >= POINTS) {
            points = POINTS;
        } else {
            points = entry->pf_count;
        }
        for (i=0; i<points-1; i++) {
            fprintf(fd,"%lf ", entry->pf_reals[i]);
        }
        fprintf(fd,"%lf", entry->pf_reals[points-1]);
        fprintf(fd,"\" />\n" \
                "</stats></event>\n");
    }
    fprintf(fd,"</host>\n</db>\n</pfdump>\n");
}

