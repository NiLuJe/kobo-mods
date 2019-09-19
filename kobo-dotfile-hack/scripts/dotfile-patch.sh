#!/bin/sh -x

# In the (possibly unreachable, depending on timings) case there's an update pending, don't do anything,
# we wouldn't want to reboot *during* an update...
if [ -e /mnt/onboard/.kobo/Kobo.tgz ] || [ -e /mnt/onboard/.kobo/KoboRoot.tgz ] ; then
	exit 0
fi

# The code snippet we want to inject *before* nickel's startup
PRE_PATT='# dotfile-hack-pre-start\nif [ -f /usr/local/geek1011/lib/libhidedir_kobo.so ] && [ ! -f /etc/ld.so.preload ] ; then\n\texport LD_PRELOAD=/usr/local/geek1011/lib/libhidedir_kobo.so\nfi\n# dotfile-hack-pre-end'

# The code snippet we want to inject *after* nickel's startup
POST_PATT='# dotfile-hack-post-start\nunset LD_PRELOAD\n# dotfile-hack-post-end'

# Remember if we actually patched something
PATCH_DONE="false"

# FIXME: Real rcS paths!
# Do we need to patch?
if ! grep -q dotfile-hack-pre-start rcS ; then
	# Assume we'll have done something untoward to rcS!
	PATCH_DONE="true"

	# Do eeeet!
	sed "/\/usr\/local\/Kobo\/nickel/i ${PRE_PATT}" -i rcS
fi

# Do we need to patch?
if ! grep -q dotfile-hack-post-start rcS ; then
	# Assume we'll have done something untoward to rcS!
	PATCH_DONE="true"

	# Do eeeet!
	sed "/\/usr\/local\/Kobo\/nickel/a ${POST_PATT}" -i rcS
fi

# If we applied a patch, reboot right now, as we modified the amount of lines in rcS, and shit may otherwise horribly blow up.
if [ "${PATCH_DONE}" = "true" ] ; then
	# If there's a global preload config, remove it
	if [ -f /etc/ld.so.preload ] ; then
		# FIXME!
		echo rm /etc/ld.so.preload
	fi

	sync
	# FIXME!
	#reboot
fi

exit 0

