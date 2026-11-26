#include "native_internal.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct ThreadPeer {
    pthread_t thread;
    int started;
    int alive;
    pthread_mutex_t mu;
    pthread_cond_t done;
};

struct ThreadLaunch {
    Runtime *rt;
    JavaObject *runnable;
    ArtMethod *run;
    struct ThreadPeer *peer;
};

static struct ThreadPeer *thread_peer(JavaObject *thread)
{
    JValue value;
    struct ThreadPeer *peer;

    if (java_object_get_field(thread, "peer", &value) != 0) {
        return NULL;
    }
    if (value.l != NULL) {
        return value.l;
    }
    peer = calloc(1, sizeof(*peer));
    if (peer == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&peer->mu, NULL) != 0) {
        free(peer);
        return NULL;
    }
    if (pthread_cond_init(&peer->done, NULL) != 0) {
        pthread_mutex_destroy(&peer->mu);
        free(peer);
        return NULL;
    }
    memset(&value, 0, sizeof(value));
    value.l = peer;
    if (java_object_set_field(thread, "peer", value) != 0) {
        pthread_cond_destroy(&peer->done);
        pthread_mutex_destroy(&peer->mu);
        free(peer);
        return NULL;
    }
    return peer;
}

static void *thread_main(void *arg)
{
    struct ThreadLaunch *launch = arg;
    JValue args;
    int rc;

    memset(&args, 0, sizeof(args));
    args.l = launch->runnable;
    rc = runtime_invoke(launch->rt, launch->run, &args, NULL);
    if (rc != 0) {
        fprintf(stderr, "Thread.run failed\n");
    }
    pthread_mutex_lock(&launch->peer->mu);
    launch->peer->alive = 0;
    pthread_cond_broadcast(&launch->peer->done);
    pthread_mutex_unlock(&launch->peer->mu);
    free(launch);
    return NULL;
}

static int native_thread_init_runnable(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue target;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l) || !java_object_check(args[1].l)) {
        return -1;
    }
    if (thread_peer(args[0].l) == NULL) {
        return -1;
    }
    memset(&target, 0, sizeof(target));
    target.l = args[1].l;
    if (java_object_set_field(args[0].l, "target", target) != 0) {
        return -1;
    }
    fprintf(stderr, "Thread.<init> %s\n", java_object_class(args[1].l));
    return 0;
}

static int native_thread_start(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *thread;
    JavaObject *runnable;
    struct ThreadPeer *peer;
    struct ThreadLaunch *launch;
    JValue target;
    ArtMethod *run;

    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    thread = args[0].l;
    peer = thread_peer(thread);
    if (peer == NULL || java_object_get_field(thread, "target", &target) != 0 ||
        !java_object_check(target.l)) {
        return -1;
    }
    runnable = target.l;
    run = runtime_find_method(rt, java_object_class(runnable), "run", "()V");
    if (run == NULL) {
        fprintf(stderr, "no run()V on %s\n", java_object_class(runnable));
        return -1;
    }
    launch = calloc(1, sizeof(*launch));
    if (launch == NULL) {
        return -1;
    }
    launch->rt = rt;
    launch->runnable = runnable;
    launch->run = run;
    launch->peer = peer;

    pthread_mutex_lock(&peer->mu);
    if (peer->started) {
        pthread_mutex_unlock(&peer->mu);
        free(launch);
        return -1;
    }
    peer->alive = 1;
    if (pthread_create(&peer->thread, NULL, thread_main, launch) != 0) {
        peer->alive = 0;
        pthread_mutex_unlock(&peer->mu);
        free(launch);
        return -1;
    }
    peer->started = 1;
    pthread_mutex_unlock(&peer->mu);
    fprintf(stderr, "Thread.start %s\n", java_object_class(runnable));
    return 0;
}

static int native_thread_join(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    struct ThreadPeer *peer;

    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    peer = thread_peer(args[0].l);
    if (peer == NULL) {
        return -1;
    }
    pthread_mutex_lock(&peer->mu);
    while (peer->alive) {
        pthread_cond_wait(&peer->done, &peer->mu);
    }
    pthread_mutex_unlock(&peer->mu);
    fprintf(stderr, "Thread.join\n");
    return 0;
}

static int native_thread_sleep(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    struct timespec ts;
    int64_t millis;

    (void)rt;
    (void)result;
    if (args == NULL || argc < 1) {
        return -1;
    }
    millis = args[0].j;
    if (millis < 0) {
        return -1;
    }
    ts.tv_sec = (time_t)(millis / 1000);
    ts.tv_nsec = (long)(millis % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) != 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

int native_register_thread(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Ljava/lang/Thread;", "<init>", "(Ljava/lang/Runnable;)V", native_thread_init_runnable},
        {"Ljava/lang/Thread;", "start", "()V", native_thread_start},
        {"Ljava/lang/Thread;", "join", "()V", native_thread_join},
        {"Ljava/lang/Thread;", "sleep", "(J)V", native_thread_sleep},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
