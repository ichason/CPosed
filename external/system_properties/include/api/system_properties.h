/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

/**
 * @file system_properties.h
 * @brief System properties.
 */

#include <sys/cdefs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hacks.h"

__BEGIN_DECLS

/** An opaque structure representing a system property. */
typedef struct prop_info prop_info;

/**
 * The limit on the length of a property value.
 * (See PROP_NAME_MAX for property names.)
 */
#define PROP_VALUE_MAX  92

/**
 * Sets system property `name` to `value`, creating the system property if it doesn't already exist.
 *
 * Returns 0 on success, or -1 on failure.
 */
int __system_property_set(const char* _Nonnull __name, const char* _Nonnull __value);

/**
 * Returns a `prop_info` corresponding system property `name`, or nullptr if it doesn't exist.
 * Use __system_property_read_callback() to query the current value.
 *
 * Property lookup is expensive, so it can be useful to cache the result of this
 * function rather than using __system_property_get().
 */
const prop_info* _Nullable __system_property_find(const char* _Nonnull __name);

/**
 * Calls `callback` with a consistent trio of name, value, and serial number
 * for property `pi`.
 *
 * Available since API level 26.
 */
void __system_property_read_callback(const prop_info* _Nonnull __pi,
    void (* _Nonnull __callback)(void* _Nullable __cookie, const char* _Nonnull __name, const char* _Nonnull __value, uint32_t __serial),
    void* _Nullable __cookie) __INTRODUCED_IN(26);

/**
 * Passes a `prop_info` for each system property to the provided
 * callback. Use __system_property_read_callback() to read the value of
 * any of the properties.
 *
 * This method is for inspecting and debugging the property system, and not generally useful.
 *
 * Returns 0 on success, or -1 on failure.
 */
int __system_property_foreach(void (* _Nonnull __callback)(const prop_info* _Nonnull __pi, void* _Nullable __cookie), void* _Nullable __cookie);

/**
 * Waits for the specific system property identified by `pi` to be updated
 * past `old_serial`. Waits no longer than `relative_timeout`, or forever
 * if `relative_timeout` is null.
 *
 * If `pi` is null, waits for the global serial number instead.
 *
 * If you don't know the current serial, use 0.
 *
 * Returns true and updates `*new_serial_ptr` on success, or false if the call
 * timed out.
 *
 * Available since API level 26.
 */
struct timespec;
bool __system_property_wait(const prop_info* _Nullable __pi, uint32_t __old_serial, uint32_t* _Nonnull __new_serial_ptr, const struct timespec* _Nullable __relative_timeout)
    __INTRODUCED_IN(26);

/**
 * Deprecated: there's no limit on the length of a property name since
 * API level 26, though the limit on property values (PROP_VALUE_MAX) remains.
 */
#define PROP_NAME_MAX   32

/** Deprecated. Use __system_property_read_callback() instead. */
int __system_property_read(const prop_info* _Nonnull __pi, char* _Nullable __name, char* _Nonnull __value);
/** Deprecated. Use __system_property_read_callback() instead. */
int __system_property_get(const char* _Nonnull __name, char* _Nonnull __value);
/** Deprecated. Use __system_property_foreach() instead. */
const prop_info* _Nullable __system_property_find_nth(unsigned __n);

__END_DECLS
