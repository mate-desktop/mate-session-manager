#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Archlinux
requires=(
	ccache # Use ccache to speed up build
	clang  # Build with clang on Archlinux
)

# https://gitlab.archlinux.org/archlinux/packaging/packages/mate-session-manager
requires+=(
	autoconf-archive
	dbus-glib
	gcc
	gettext
	git
	glib2-devel
	gtk3
	libsm
	make
	mate-common
	mate-desktop
	python
	systemd
	which
	xtrans
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
