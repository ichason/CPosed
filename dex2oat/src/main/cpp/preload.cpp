/*
 * This file is part of LSPosed.
 *
 * LSPosed is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LSPosed is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LSPosed.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2020 EdXposed Contributors
 * Copyright (C) 2021 LSPosed Contributors
 */

//
// Created by Irena on 2025/3/22.
//

#include <cstdlib>
#include <cstring>
#include <string>
#include <dlfcn.h>

#define LOG_TAG "PRELOAD"
#include "logging.h"

extern "C" {
    [[gnu::visibility("default"), gnu::used]]
    void _ZN3art15CompilerOptionsC1Ev(void *self) { // NOLINT(bugprone-reserved-identifier)
        static void *Constructor = dlsym(RTLD_NEXT, "_ZN3art15CompilerOptionsC1Ev");

        if (!Constructor) {
            const char *symbol = "_ZN3art15CompilerOptionsC1Ev";
            LOGE("get sym %s, error: %s", symbol, dlerror());
            return;
        }

        using ConstructorType = void (*)(void *);
        auto constructor = reinterpret_cast<ConstructorType>(Constructor);
        constructor(self);

        constexpr size_t MAX_ITERATIONS = 10;
        constexpr size_t MAX_OFFSET = sizeof(uintptr_t) * MAX_ITERATIONS;
        for (size_t i = 0; i <= MAX_OFFSET; i += sizeof(uintptr_t)) {
            if (i == MAX_OFFSET) {
                LOGE("offsetof inline_max_code_units_ not found!");
                return;
            }

            auto* member_ptr = reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(self) + i);
            uintptr_t member_value = *member_ptr;

            if (member_value == static_cast<uintptr_t>(-1)) {
                *member_ptr = 0;
                break;
            }
        }

        (void)unsetenv("LD_PRELOAD");

    }
}
