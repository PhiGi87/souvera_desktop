/*
 * SPDX-FileCopyrightText: 2020 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <string>

class ConfigIni
{
public:
    ConfigIni();

    bool load();

    std::wstring getAppName() const;

private:
    std::wstring _appName;
};
