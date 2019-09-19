#!/bin/sh -x

# In the (possibly unreachable, depending on timings) case there's an update pending, don't do anything,
# we wouldn't want to reboot *during* an update...
if [ -e /mnt/onboard/.kobo/Kobo.tgz ] || [ -e /mnt/onboard/.kobo/KoboRoot.tgz ] ; then
	exit 0
fi

# Remember if we actually patched something
PATCH_DONE="false"

# Do we need to patch?
if ! grep -q dotfile-hack-pre-start /etc/init.d/rcS ; then
	# Assume we'll have done something untoward to rcS!
	PATCH_DONE="true"

	# The code snippet we want to inject *before* nickel's startup
	PRE_PATT='# dotfile-hack-pre-start\nif [ -f /usr/local/geek1011/lib/libhidedir_kobo.so ] \&\& [ ! -f /etc/ld.so.preload ] ; then\n\texport LD_PRELOAD=/usr/local/geek1011/lib/libhidedir_kobo.so\nfi\n# dotfile-hack-pre-end'

	# Do eeeet!
	sed "/\/usr\/local\/Kobo\/nickel/i #%PRE_PATT%" -i /etc/init.d/rcS
	sed -e "s:#%PRE_PATT%:${PRE_PATT}:" -i /etc/init.d/rcS
fi

# Do we need to patch?
if ! grep -q dotfile-hack-post-start /etc/init.d/rcS ; then
	# Assume we'll have done something untoward to rcS!
	PATCH_DONE="true"

	# The code snippet we want to inject *after* nickel's startup
	POST_PATT='# dotfile-hack-post-start\nunset LD_PRELOAD\n# dotfile-hack-post-end'

	# Do eeeet!
	sed "/\/usr\/local\/Kobo\/nickel/a #%POST_PATT%" -i /etc/init.d/rcS
	sed -e "s:#%POST_PATT%:${POST_PATT}:" -i /etc/init.d/rcS
fi

# If we applied a patch, reboot right now, as we modified the amount of lines in rcS, and shit may otherwise horribly blow up.
if [ "${PATCH_DONE}" = "true" ] ; then
	# If there's a global preload config, remove it
	if [ -f /etc/ld.so.preload ] ; then
		rm /etc/ld.so.preload
	fi

	sync
	# FIXME! Check & restore before reboot.
	#reboot
fi

exit 0

