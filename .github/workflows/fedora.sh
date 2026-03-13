#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autoconf-archive
	dbus-glib-devel
	desktop-file-utils
	gcc
	git
	gtk3-devel
	libSM-devel
	libXtst-devel
	make
	mate-common
	mesa-libGLES-devel
	pango-devel
	redhat-rpm-config
	systemd-devel
	xmlto
	xorg-x11-xtrans-devel
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
