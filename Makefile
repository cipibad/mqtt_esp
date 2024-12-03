#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

$(shell echo "#define FW_VERSION \"$$(git describe --always --dirty | sed 's/-g[a-z0-9]\{7\}//')\"" > main/version.h.tmp; if diff -q main/version.h.tmp main/version.h >/dev/null 2>&1; then rm main/version.h.tmp; else mv main/version.h.tmp main/version.h; fi)

PROJECT_NAME := mqtt_ssl

include $(IDF_PATH)/make/project.mk

