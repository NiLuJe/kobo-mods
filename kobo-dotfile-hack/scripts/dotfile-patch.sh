#!/bin/sh -x

# In the (possibly unreachable, depending on timings) case there's an update pending, don't do anything,
# we wouldn't want to reboot *during* an update...
if [ -e /mnt/onboard/.kobo/Kobo.tgz ] || [ -e /mnt/onboard/.kobo/KoboRoot.tgz ] ; then
	fbink -qmp -y 5 "[.H] Update in progress, abort!"
	exit 0
fi

# If we have a leftover backup, it means the patching failed, so don't try again.
if [ -f /usr/local/geek1011/etc/dotfile-rcS.bak ] ; then
	fbink -qmph -y 5 "[.H] Leftover backup, abort!"
	exit 0
fi

# Remember if we actually patched something
PATCH_DONE="false"

# Make a backup...
mkdir -p /usr/local/geek1011/etc
cp -a /etc/init.d/rcS /usr/local/geek1011/etc/dotfile-rcS.bak

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
	sync

	# Double-check that it was successful...
	if grep -q dotfile-hack-pre-start /etc/init.d/rcS && grep -q dotfile-hack-pre-end /etc/init.d/rcS && grep -q dotfile-hack-post-start /etc/init.d/rcS && grep -q dotfile-hack-post-end /etc/init.d/rcS ; then
		# Whee!
		fbink -qmph -y 5 "[.H] Patching successful, rebooting . . ."

		# If there's a global preload config, remove it
		if [ -f /etc/ld.so.preload ] ; then
			rm /etc/ld.so.preload
		fi

		# Remove our backup
		rm /usr/local/geek1011/etc/dotfile-rcS.bak
		sync

		# And reboot (FIXME!)
		echo reboot
	else
		# Uh oh...
		fbink -qmph -y 5 "[.H] Patching failed, aborting . . ."

		# Restore backup
		cp -a /usr/local/geek1011/etc/dotfile-rcS.bak /etc/init.d/rcS
		# Keep the backup in place, so we don't try the patching again!
		sync

		# And reboot (FIXME!)
		echo reboot
	fi
else
	# Already patched, remove backup ;).
	rm /usr/local/geek1011/etc/dotfile-rcS.bak
	sync
fi

exit 0

