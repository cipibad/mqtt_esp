#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := mqtt_ssl
EXTRA_COMPONENT_DIRS := $(CURDIR)/../../../esp-idf-lib/components

include $(IDF_PATH)/make/project.mk

