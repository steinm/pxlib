#!/bin/sh
#
# autogen.sh glue for CMU Cyrus IMAP
# $Id$
#
# Requires: automake, autoconf, dpkg-dev
set -e

# Refresh GNU autotools toolchain.
for i in config.guess config.sub missing install-sh mkinstalldirs ; do
	test -r /usr/share/automake/${i} && {
		rm -f ${i}
		cp /usr/share/automake/${i} .
	}
	chmod 755 ${i}
done

intltoolize --force --copy
aclocal
autoheader
automake --verbose --copy --force --add-missing
autoconf

# For the Debian build
test -d debian && {
	# Kill executable list first
	rm -f debian/executable.files

	# Make sure our executable and removable lists won't be screwed up
	debclean && echo Cleaned buildtree just in case...

	# refresh list of executable scripts, to avoid possible breakage if
	# upstream tarball does not include the file or if it is mispackaged
	# for whatever reason.
	echo Generating list of executable files...
	rm -f debian/executable.files
	find -type f -perm +111 ! -name '.*' -fprint debian/executable.files

	# link these in Debian builds
#	rm -f config.sub config.guess
#	ln -s /usr/share/misc/config.sub .
#	ln -s /usr/share/misc/config.guess .
}

exit 0
