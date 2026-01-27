#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Debian
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autoconf-archive
	autopoint
	gcc
	git
	intltool
	libdbus-glib-1-dev
	libgl1-mesa-dev
	libgles2-mesa-dev
	libglib2.0-dev
	libgtk-3-dev
	libice-dev
	libsm-dev
	libstartup-notification0-dev
	libsystemd-dev
	libx11-dev
	libxau-dev
	libxext-dev
	libxrender-dev
	libxt-dev
	libxtst-dev
	make
	mate-common
	xmlto
	xsltproc
)

infobegin "Update system"
apt-get update -qq
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
