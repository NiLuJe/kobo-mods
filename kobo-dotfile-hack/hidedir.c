#define _GNU_SOURCE // for RTLD_NEXT
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

#define constructor __attribute__((constructor))
#define strcasecmp  __builtin_strcasecmp
#define strncasecmp __builtin_strncasecmp

static bool wrap = true;

static struct dirent   *(*readdir_orig)(DIR *dir);
static struct dirent64 *(*readdir64_orig)(DIR *dir);
static int              (*readdir_r_orig)(DIR *dir, struct dirent *ent, struct dirent **res);
static int              (*readdir64_r_orig)(DIR *dir, struct dirent64 *ent, struct dirent64 **res);

#ifdef USE_FULL_PATH
static DIR  *(*opendir_orig)(const char *name);
static DIR  *(*fdopendir_orig)(int fd);
static int   (*closedir_orig)(DIR *dir);
static char *dirpaths[1024];    // c.f., cat /proc/$(pidof nickel)/limits
#endif

static bool isproc(const char* proc) {
    char buf[PATH_MAX];
    ssize_t len;
    if ((len = readlink("/proc/self/exe", buf, PATH_MAX)) != -1) {
        buf[len] = '\0';
        return strcmp(strrchr(buf, '/')+1, proc) == 0;
    }
    return false;
}

constructor static void init() {
    #ifdef NICKEL_ONLY
    wrap = isproc("nickel");
    #endif
    #ifdef LS_ONLY
    wrap = isproc("ls");
    #endif

    readdir_orig     = dlsym(RTLD_NEXT, "readdir");
    readdir64_orig   = dlsym(RTLD_NEXT, "readdir64");
    readdir_r_orig   = dlsym(RTLD_NEXT, "readdir_r");
    readdir64_r_orig = dlsym(RTLD_NEXT, "readdir64_r");

    #ifdef USE_FULL_PATH
    opendir_orig     = dlsym(RTLD_NEXT, "opendir");
    fdopendir_orig   = dlsym(RTLD_NEXT, "fdopendir");
    closedir_orig    = dlsym(RTLD_NEXT, "closedir");
    #endif
}

static bool should_hide(DIR *dir, const char *name, const unsigned char type);

#define WRAP_READDIR(dirent, readdir)                 \
struct dirent *readdir(DIR *dir) {                    \
    if (!wrap) return readdir##_orig(dir);            \
    struct dirent *res;                               \
    int err;                                          \
    for (;;) {                                        \
        res = readdir##_orig(dir);                    \
        err = errno;                                  \
        syslog(LOG_DEBUG, "%s: ", #readdir);  \
        if (!res || !should_hide(dir, res->d_name, res->d_type)) { \
            errno = err;                              \
            return res;                               \
        }                                             \
    }                                                 \
    errno = 0;                                        \
    return NULL;                                      \
}

WRAP_READDIR(dirent,   readdir)
WRAP_READDIR(dirent64, readdir64)

#define WRAP_READDIR_R(dirent, readdir_r)                               \
int readdir_r(DIR *dir, struct dirent *ent, struct dirent **res) {      \
    if (!wrap) return readdir_r##_orig(dir, ent, res);                  \
    int ret, err;                                                       \
    for (;;) {                                                          \
        ret = readdir_r##_orig(dir, ent, res);                          \
        err = errno;                                                    \
        syslog(LOG_DEBUG, "%s: ", #readdir_r);                            \
        if (ret || !*res || !should_hide(dir, (*res)->d_name, (*res)->d_type)) {        \
            errno = err;                                                \
            return ret;                                                 \
        }                                                               \
    }                                                                   \
    *res = NULL;                                                        \
    errno = 0;                                                          \
    return 0;                                                           \
}

WRAP_READDIR_R(dirent,   readdir_r)
WRAP_READDIR_R(dirent64, readdir64_r)

#ifndef USE_FULL_PATH

static bool should_hide(DIR *dir, const char *name, const unsigned char type) {
    syslog(LOG_DEBUG, "should_hide\t\t\t\t`%s`\t?\t", name);
    if (type != DT_DIR) { // show anything that's not a directory
        syslog(LOG_DEBUG, "NO (0)\n");
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { // show **/. **/..
        syslog(LOG_DEBUG, "NO (1)\n");
        return false;
    }
    if (strncasecmp(name, ".kobo", 5) == 0) { // show **/.kobo*
        syslog(LOG_DEBUG, "NO (3)\n");
        return false;
    }
    if (strcasecmp(name, ".adobe-digital-editions") == 0) { // show **/.adobe-digital-editions
        syslog(LOG_DEBUG, "NO (4)\n");
        return false;
    }
    if (name[0] == '.') { // hide **/.* (but not everything underneath)
        syslog(LOG_DEBUG, "YES (5)\n");
        return true;
    } else {
        syslog(LOG_DEBUG, "NO (5)\n");
        return false;
    }
}

#else

static bool should_hide(DIR *dir, const char *name, const unsigned char type) {
    int fd;
    char *path = "";
    if ((fd = dirfd(dir)) > 0 && dirpaths[fd]) {
        path = dirpaths[fd];
        syslog(LOG_DEBUG, "should_hide\t\t\t\t`%s`\t\ton\t\t`%s`\t?\t", name, path);
    }
    if (type != DT_DIR) { // show anything that's not a directory
        syslog(LOG_DEBUG, "NO (0)\n");
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { // show **/. **/..
        syslog(LOG_DEBUG, "NO (1)\n");
        return false;
    }
    if (strncasecmp(path, "/mnt/", 5) != 0) { // show !/mnt/**
        syslog(LOG_DEBUG, "NO (2)\n");
        return false;
    }
    if (strncasecmp(name, ".kobo", 5) == 0 || strcasestr(path, "/.kobo")) { // show **/.kobo*/**
        syslog(LOG_DEBUG, "NO (3)\n");
        return false;
    }
    if (strcasecmp(name, ".adobe-digital-editions") == 0 || strcasestr(path, "/.adobe-digital-editions")) { // show **/.adobe-digital-editions/**
        syslog(LOG_DEBUG, "NO (4)\n");
        return false;
    }
    if (name[0] == '.') { // hide **/.* (preventing it from being traversed)
        syslog(LOG_DEBUG, "YES (5)\n");
        return true;
    } else {
        syslog(LOG_DEBUG, "NO (5)\n");
        return false;
    }
}

DIR *opendir(const char *name) {
    if (!wrap) return opendir_orig(name);
    int err, fd;
    DIR *dir = opendir_orig(name);
    err = errno;
    if (dir && (fd = dirfd(dir)) > 0) {
        syslog(LOG_DEBUG, "opendir: `%s`\n", name);
        if (!(dirpaths[fd] = realpath(name, NULL))) {
            dirpaths[fd] = strdup(name);
            syslog(LOG_DEBUG, "cached %d -> `%s` (as-is)\n", fd, name);
        } else {
            syslog(LOG_DEBUG, "cached %d -> `%s` (canonicalized)\n", fd, name);
        }
    }
    errno = err;
    return dir;
}

DIR *fdopendir(int fd) {
    if (!wrap) return fdopendir_orig(fd);
    // NOTE: If Kobo uses this (it doesn't currently), most of the should_hide stuff won't work.
    //       We'd probably need something like an extra fstat call to pickup the path?
    //       That'd feel moderately less hacky than a readlink on /proc/self/fd/$fd ;).
    dirpaths[fd] = NULL;
    return fdopendir_orig(fd);
}

int closedir(DIR* dir) {
    if (!wrap) return closedir_orig(dir);
    int fd;
    if ((fd = dirfd(dir)) > 0 && dirpaths[fd]) {
        syslog(LOG_DEBUG, "closedir: `%s`\n", dirpaths[fd]);
        syslog(LOG_DEBUG, "clearing %d -> `%s`\n", fd, dirpaths[fd]);
        free(dirpaths[fd]);
        dirpaths[fd] = NULL;
    }
    return closedir_orig(dir);
}
#endif
