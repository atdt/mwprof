/*
 This is a daemon, that sits on port 3811 (overridable by
 setting a COLLECTOR_PORT environment variable)
 receives profiling events from mediawiki ProfilerSimpleUDP,
 and places them into BerkeleyDB file. \o/

 Author: Domas Mituzas ( http://dammit.lt/ )
 Author: Asher Feldman ( afeldman@wikimedia.org )
 Author: Tim Starling ( tstarling@wikimedia.org )
 License: public domain (as if there's something to protect ;-)

 $Id: collector.c 112227 2012-02-23 19:15:17Z midom $
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <glib.h>
#include "collector.h"

void child();
void truncatedb();

void handleMessage(char *,ssize_t );
void handleConnection(int);
void updateEntry(char *dbname, char *hostname, char *task, struct pfstats *incoming);

int main(int ac, char **av) {

    ssize_t l;
    char buf[2000];
    int r;

    /* Socket variables */
    int s, exp;
    u_int yes=1;
    int port;
    struct sockaddr_in me, them;
    socklen_t sl = sizeof(struct sockaddr_in);

    struct pollfd fds[2];

    /*Initialization*/{
        if (getenv("COLLECTOR_PORT") != NULL)
            port=atoi(getenv("COLLECTOR_PORT"));
        else
            port=3811;
        bzero(&me,sizeof(me));
        me.sin_family= AF_INET;
        me.sin_port=htons(port);

        s=socket(AF_INET,SOCK_DGRAM,0);
        bind(s,(struct sockaddr *)&me,sizeof(me));

        exp=socket(AF_INET,SOCK_STREAM,0);
        setsockopt(exp,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        bind(exp,(struct sockaddr *)&me,sizeof(me));
        listen(exp,10);

        bzero(&fds,sizeof(fds));

        fcntl(s,F_SETFL,O_NONBLOCK);
        fcntl(exp,F_SETFL,O_NONBLOCK);

        fds[0].fd = s; fds[0].events |= POLLIN;
        fds[1].fd = exp, fds[1].events |= POLLIN;

        table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

        signal(SIGCHLD,child);
        signal(SIGUSR1,truncatedb);
        daemon(1,0);
    }
    /* Loop! loop! loop! */
    for(;;) {
        r=poll(fds,2,-1);

        /* Process incoming UDP queue */
        while(( fds[0].revents & POLLIN ) &&
            !( fds[1].revents & POLLIN ) &&
            ((l=recvfrom(s,&buf,1500,0,NULL,NULL))!=-1)) {
                if (l==EAGAIN)
                    break;
                handleMessage((char *)&buf,l);
            }

        /* Process incoming TCP queue */
        if (fds[1].revents & POLLIN) {
            r = accept(exp, (struct sockaddr *)&them, &sl);
            if ( r !=-1 && r != EAGAIN ) {
                handleConnection(r);
            }
        }
    }
    return(0);
}

/* Decides what to do with incoming UDP message */
void handleMessage(char *buf,ssize_t l) {
    char *p,*pp;
    char hostname[128];
    char dbname[128];
    char task[1024];
    char stats[] = "stats/";
    int r;

    struct pfstats incoming;
    /* db host count cpu cpusq real realsq eventdescription */
    const char msgformat[]="%127s %127s %ld %lf %lf %lf %lf %1023[^\n]";



    buf[l]=0;
    pp=buf;

    while((p=strsep(&pp,"\r\n"))) {
        if (p[0]=='\0')
            continue;
        if (!strcmp("-truncate",p)) {
            truncatedb();
            return;
        }
        bzero(&incoming,sizeof(incoming));
        r=sscanf(p,msgformat,(char *)&dbname,(char *)&hostname,
            &incoming.pf_count,&incoming.pf_cpu,&incoming.pf_cpu_sq,
            &incoming.pf_real,&incoming.pf_real_sq, (char *)&task);
        if (r<7)
            continue;

        // Update the DB-specific entry
        updateEntry(dbname, hostname, task, &incoming);

        // Update the aggregate entry
        if (!strncmp(dbname, stats, sizeof(stats) - 1)) {
            updateEntry("stats/all", "-", task, &incoming);
        } else {
            updateEntry("all", "-", task, &incoming);
        }
    }
}

void updateEntry(char *dbname, char *hostname, char *task, struct pfstats *incoming) {
    char keytext[1500];
    struct pfstats *entry;

    snprintf(keytext,1499,"%s:%s:%s",dbname,hostname,task);

    /* Add new values if exists, put in fresh structure if not */

    entry = g_hash_table_lookup(table, keytext);
    if (entry == NULL) {
        entry = g_malloc0(sizeof(struct pfstats));
        g_hash_table_insert(table, g_strdup(keytext), entry);
    } else if (entry->pf_real_pointer == POINTS) {
        entry->pf_real_pointer = 0;
    }

    entry->pf_count   += incoming->pf_count;
    entry->pf_cpu     += incoming->pf_cpu;
    entry->pf_cpu_sq  += incoming->pf_cpu_sq;
    entry->pf_real    += incoming->pf_real;
    entry->pf_real_sq += incoming->pf_real_sq;
    entry->pf_reals[entry->pf_real_pointer] = incoming->pf_real;
    entry->pf_real_pointer++;
}

void handleConnection(int c) {
    FILE *tmp;
    char buf[1024];
    int r;

    shutdown(c,SHUT_RD);

    tmp=tmpfile();
    dumpData(table, tmp);
    rewind(tmp);
    if (fork()) {
        fclose(tmp);
        close(c);
    } else {
        while(!feof(tmp)) {
            r=fread((char *)&buf,1,1024,tmp);
            write(c,(char *)&buf,r);
        }
        close(c);
        fclose(tmp);
        exit(0);
    }
}

/* Event handling */
void child() {
    wait(0);
}

void truncatedb() {
    g_hash_table_remove_all(table);
}
