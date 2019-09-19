#!/bin/sh -x

# In the (possibly unreachable, depending on timings) case there's an update pending, don't do anything,
# we wouldn't want to reboot *during* an update...
if [ -e /mnt/onboard/.kobo/Kobo.tgz ] || [ -e /mnt/onboard/.kobo/KoboRoot.tgz ] ; then
	fbink -qmp -y 5 "[.H] Update in progress, abort!"
	exit 0
fi

# Constants
KOBO_RCS="/etc/init.d/rcS"
DBKP_RCS="/usr/local/geek1011/etc/dotfile-rcS.bak"


# If we have a leftover backup, it means the patching failed, so don't try again.
if [ -f "${DBKP_RCS}" ] ; then
	fbink -qmph -y 5 "[.H] Leftover backup, abort!"
	exit 0
fi

# Remember if we actually patched something
PATCH_DONE="false"

# Make a backup...
mkdir -p /usr/local/geek1011/etc
cp -a "${KOBO_RCS}" "${DBKP_RCS}"

# Do we need to patch?
if ! grep -q '^# dotfile-hack-pre-start' "${KOBO_RCS}" ; then
	# Assume we'll have done something untoward to rcS!
	PATCH_DONE="true"

	# The code snippet we want to inject *before* nickel's startup
	PRE_PATT='# dotfile-hack-pre-start\nif [ -f /usr/local/geek1011/lib/libhidedir_kobo.so ] \&\& [ ! -f /etc/ld.so.preload ] ; then\n\texport LD_PRELOAD=/usr/local/geek1011/lib/libhidedir_kobo.so\nfi\n# dotfile-hack-pre-end'

	# Do eeeet!
	sed "/\/usr\/local\/Kobo\/nickel/i #%PRE_PATT%" -i "${KOBO_RCS}"
	sed -e "s:#%PRE_PATT%:${PRE_PATT}:" -i "${KOBO_RCS}"
fi

# Do we need to patch?
if ! grep -q '^# dotfile-hack-post-start' "${KOBO_RCS}" ; then
	# Assume we'll have done something untoward to rcS!
	PATCH_DONE="true"

	# The code snippet we want to inject *after* nickel's startup
	POST_PATT='# dotfile-hack-post-start\nunset LD_PRELOAD\n# dotfile-hack-post-end'

	# Do eeeet!
	sed "/\/usr\/local\/Kobo\/nickel/a #%POST_PATT%" -i "${KOBO_RCS}"
	sed -e "s:#%POST_PATT%:${POST_PATT}:" -i "${KOBO_RCS}"
fi

# If we applied a patch, reboot right now, as we modified the amount of lines in rcS, and shit may otherwise horribly blow up.
if [ "${PATCH_DONE}" = "true" ] ; then
	sync

	# Double-check that it was successful...
	if grep -q '^# dotfile-hack-pre-start' "${KOBO_RCS}" && grep -q '^# dotfile-hack-pre-end' "${KOBO_RCS}" && grep -q '^# dotfile-hack-post-start' "${KOBO_RCS}" && grep -q '^# dotfile-hack-post-end' "${KOBO_RCS}" ; then
		# Whee!
		fbink -qmph -y 5 "[.H] Patching successful, rebooting . . ."

		# If there's a global preload config, remove it
		if [ -f /etc/ld.so.preload ] ; then
			rm /etc/ld.so.preload
		fi

		# Remove our backup
		rm "${DBKP_RCS}"
		sync

		# And reboot
		reboot
	else
		# Uh oh...
		fbink -qmph -y 5 "[.H] Patching failed, aborting . . ."

		# Restore backup
		cp -a "${DBKP_RCS}" "${KOBO_RCS}"
		# Keep the backup in place, so we don't try the patching again!
		sync

		# And reboot
		reboot
	fi
else
	# Already patched, remove backup ;).
	rm "${DBKP_RCS}"
fi

exit 0

