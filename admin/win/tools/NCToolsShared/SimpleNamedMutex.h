/*
 * SPDX-FileCopyrightText: 2020 Souvera (Host-On Service Provider GmbH)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <windows.h>
#include <string>

class SimpleNamedMutex
{
public:
    SimpleNamedMutex(const std::wstring &name);

    bool lock();
    void unlock();

private:
    std::wstring _name;
    HANDLE _hMutex = nullptr;
};
