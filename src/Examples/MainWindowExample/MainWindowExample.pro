# ***************************************************************************
# Copyright (c) 2009-2010, Jaco Naude
#
# See http://www.qtilities.org/licensing.html for licensing details.
#
# ***************************************************************************
QTILITIES += extension_system
include(../../Qtilities.pri)

QT       += core
QT       += gui

TARGET = MainWindowExample
CONFIG   -= app_bundle

TEMPLATE = app
DESTDIR = $$QTILITIES_BIN/Examples/MainWindowExample

# ------------------------------
# Temp Output Paths
# ------------------------------
OBJECTS_DIR     = $$QTILITIES_TEMP/MainWindowExample
MOC_DIR         = $$QTILITIES_TEMP/MainWindowExample
RCC_DIR         = $$QTILITIES_TEMP/MainWindowExample
UI_DIR          = $$QTILITIES_TEMP/MainWindowExample

# --------------------------
# Application Files
# --------------------------
SOURCES += main.cpp \
        ExampleMode.cpp \
        SideWidgetFileSystem.cpp

HEADERS += ExampleMode.h \
        SideWidgetFileSystem.h

FORMS   += ExampleMode.ui \
        SideWidgetFileSystem.ui

RC_FILE = rc_file.rc
