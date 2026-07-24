# SPDX-FileCopyrightText: 2020 Souvera (Host-On Service Provider GmbH)
# SPDX-FileCopyrightText: 2014 ownCloud, Inc.
# SPDX-License-Identifier: GPL-2.0-or-later
if(APPLE)
    # traverse into osx subdirectory to install and patch the create-pack script
    add_subdirectory(osx)
elseif(WIN32)
    # MSI package scripts, helper DLL and migration tools
    add_subdirectory(win)
endif()
