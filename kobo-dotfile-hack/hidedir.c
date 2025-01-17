#define _GNU_SOURCE    // for RTLD_NEXT, program_invocation_short_name
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define constructor __attribute__((constructor))

static bool wrap = true;

static __typeof__(readdir)* readdir_orig         = NULL;
static __typeof__(readdir64)* readdir64_orig     = NULL;
static __typeof__(readdir_r)* readdir_r_orig     = NULL;
static __typeof__(readdir64_r)* readdir64_r_orig = NULL;

#ifdef USE_FULL_PATH
static __typeof__(opendir)* opendir_orig     = NULL;
static __typeof__(fdopendir)* fdopendir_orig = NULL;
static __typeof__(closedir)* closedir_orig   = NULL;
static char* dirpaths[1024]                  = { 0 };    // Linux default is 1024, c.f., cat /proc/$(pidof nickel)/limits
#endif

#ifdef NICKEL_ONLY
#	define CHECKPROC "nickel"
#else
#	ifdef LS_ONLY
#		define CHECKPROC "ls"
#	endif
#endif

#ifdef CHECKPROC
static bool
    isproc(const char* proc)
{
	return strcmp(program_invocation_short_name, proc) == 0;
}
#endif

constructor static void
    init()
{
#ifdef CHECKPROC
	wrap = isproc(CHECKPROC);
#endif

	readdir_orig     = dlsym(RTLD_NEXT, "readdir");
	readdir64_orig   = dlsym(RTLD_NEXT, "readdir64");
	readdir_r_orig   = dlsym(RTLD_NEXT, "readdir_r");
	readdir64_r_orig = dlsym(RTLD_NEXT, "readdir64_r");

#ifdef USE_FULL_PATH
	opendir_orig   = dlsym(RTLD_NEXT, "opendir");
	fdopendir_orig = dlsym(RTLD_NEXT, "fdopendir");
	closedir_orig  = dlsym(RTLD_NEXT, "closedir");
#endif

#ifdef USE_LOCAL_PRELOAD
	// Clear the env so that what Nickel spawns doesn't inherit this hack...
	unsetenv("LD_PRELOAD");
#endif
}

static bool should_hide(DIR* dir, const char* name, const unsigned char type);

#define WRAP_READDIR(dirent, readdir)                                                                                    \
	struct dirent* readdir(DIR* dir)                                                                                 \
	{                                                                                                                \
		if (!wrap)                                                                                               \
			return readdir##_orig(dir);                                                                      \
		struct dirent* res;                                                                                      \
		int            err;                                                                                      \
		for (;;) {                                                                                               \
			res = readdir##_orig(dir);                                                                       \
			err = errno;                                                                                     \
			if (!res || !should_hide(dir, res->d_name, res->d_type)) {                                       \
				errno = err;                                                                             \
				return res;                                                                              \
			}                                                                                                \
		}                                                                                                        \
		errno = 0;                                                                                               \
		return NULL;                                                                                             \
	}

WRAP_READDIR(dirent, readdir)
WRAP_READDIR(dirent64, readdir64)

#define WRAP_READDIR_R(dirent, readdir_r)                                                                                \
	int readdir_r(DIR* dir, struct dirent* ent, struct dirent** res)                                                 \
	{                                                                                                                \
		if (!wrap)                                                                                               \
			return readdir_r##_orig(dir, ent, res);                                                          \
		int ret, err;                                                                                            \
		for (;;) {                                                                                               \
			ret = readdir_r##_orig(dir, ent, res);                                                           \
			err = errno;                                                                                     \
			if (ret || !*res || !should_hide(dir, (*res)->d_name, (*res)->d_type)) {                         \
				errno = err;                                                                             \
				return ret;                                                                              \
			}                                                                                                \
		}                                                                                                        \
		*res  = NULL;                                                                                            \
		errno = 0;                                                                                               \
		return 0;                                                                                                \
	}

WRAP_READDIR_R(dirent, readdir_r)
WRAP_READDIR_R(dirent64, readdir64_r)

#ifndef USE_FULL_PATH

static bool
    should_hide(DIR* dir __attribute__((unused)), const char* name, const unsigned char type)
{
	if (type != DT_DIR) {    // show anything that's not a directory
		return false;
	}
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {    // show **/. **/..
		return false;
	}
	if (strncasecmp(name, ".kobo", 5) == 0) {    // show **/.kobo*
		return false;
	}
	if (strcasecmp(name, ".adobe-digital-editions") == 0) {    // show **/.adobe-digital-editions
		return false;
	}
	if (name[0] == '.') {    // hide **/.* (preventing it from being traversed)
#	ifdef CHECKPROC
		syslog(LOG_DEBUG, "(kobo-dotfile-hack) Hid `%s` from " CHECKPROC "!", name);
#	else
		syslog(LOG_DEBUG, "(kobo-dotfile-hack) Hid `%s`", name);
#	endif
		return true;
	} else {
		return false;
	}
}

#else

static bool
    should_hide(DIR* dir, const char* name, const unsigned char type)
{
	int   fd;
	char* path = "";
	if ((fd = dirfd(dir)) > 0 && dirpaths[fd]) {
		path = dirpaths[fd];
	}
	if (type != DT_DIR) {    // show anything that's not a directory
		return false;
	}
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {    // show **/. **/..
		return false;
	}
	if (strncasecmp(path, "/mnt/", 5) != 0) {    // show !/mnt/**
		return false;
	}
	if (strncasecmp(name, ".kobo", 5) == 0 || strcasestr(path, "/.kobo")) {    // show **/.kobo*/**
		return false;
	}
	if (strcasecmp(name, ".adobe-digital-editions") == 0 ||
	    strcasestr(path, "/.adobe-digital-editions")) {    // show **/.adobe-digital-editions/**
		return false;
	}
	if (name[0] == '.') {    // hide **/.* (preventing it from being traversed)
#	ifdef CHECKPROC
		syslog(LOG_DEBUG, "(kobo-dotfile-hack) Hid `%s/%s` from " CHECKPROC "!", path, name);
#	else
		syslog(LOG_DEBUG, "(kobo-dotfile-hack) Hid `%s/%s`!", path, name);
#	endif
		return true;
	} else {
		return false;
	}
}

DIR*
    opendir(const char* name)
{
	if (!wrap)
		return opendir_orig(name);
	int  err, fd;
	DIR* dir = opendir_orig(name);
	err      = errno;
	if (dir && (fd = dirfd(dir)) > 0) {
		if (!(dirpaths[fd] = realpath(name, NULL))) {
			dirpaths[fd] = strdup(name);
		}
	}
	errno = err;
	return dir;
}

DIR*
    fdopendir(int fd)
{
	if (!wrap)
		return fdopendir_orig(fd);
	// NOTE: If Kobo uses this (it doesn't currently), most of the should_hide stuff won't work.
	//       We'd probably need something like an extra fstat call to pickup the path?
	//       That'd feel moderately less hacky than a readlink on /proc/self/fd/$fd ;).
	// NOTE: Currently, Qt seems content with readdir64_r calls sandwiched between opendir/closedir ;).
	dirpaths[fd] = NULL;
	return fdopendir_orig(fd);
}

int
    closedir(DIR* dir)
{
	if (!wrap)
		return closedir_orig(dir);
	int fd;
	if ((fd = dirfd(dir)) > 0 && dirpaths[fd]) {
		free(dirpaths[fd]);
		dirpaths[fd] = NULL;
	}
	return closedir_orig(dir);
}

#endif    // !USE_FULL_PATH
