/// S2E Selective Symbolic Execution Platform
///
/// Copyright (c) 2026 Li-Wen Hsu
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.

#ifndef S2E_KEY_VALUE_STORE_H
#define S2E_KEY_VALUE_STORE_H

/// C interface to the KeyValueStore plugin (integer values only).
/// Usable from kernel code.

#include <s2e/s2e.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum S2E_KVSTORE_PLUGIN_COMMANDS {
    KVS_PUT_STRING,
    KVS_GET_STRING,
    KVS_PUT_INT,
    KVS_GET_INT,
    KVS_LOCK,
    KVS_UNLOCK,
    KVS_INTEGER_KEY = 0x80000000
} S2E_KVSTORE_PLUGIN_COMMANDS;

typedef struct S2E_KVSTORE_PLUGIN_COMMAND {
    S2E_KVSTORE_PLUGIN_COMMANDS Command;
    union {
        uint64_t KeyAddress;
        uint64_t IntegerKey;
    };
    uint64_t Local;
    union {
        uint64_t ValueAddress;
        uint64_t IntegerValue;
    };
    uint64_t ValueSize;
    uint64_t NewKey;
    uint64_t Success;
} S2E_KVSTORE_PLUGIN_COMMAND;

/// Get an integer value. Returns 0 when the key does not exist.
static inline int s2e_kvs_get_int(const char *key, uint64_t *value, int local) {
    S2E_KVSTORE_PLUGIN_COMMAND cmd;
    cmd.Command = KVS_GET_INT;
    cmd.KeyAddress = (uintptr_t) key;
    cmd.Local = local;
    cmd.NewKey = 0;
    cmd.Success = 0;
    cmd.ValueAddress = 0;
    cmd.ValueSize = 0;
    __s2e_touch_string(key);
    s2e_invoke_plugin("KeyValueStore", &cmd, sizeof(cmd));
    *value = cmd.IntegerValue;
    return (int) cmd.Success;
}

/// Set an integer value. Returns 0 on failure; new_key (optional) tells whether the key was created.
static inline int s2e_kvs_put_int(const char *key, uint64_t value, int local, int *new_key) {
    S2E_KVSTORE_PLUGIN_COMMAND cmd;
    cmd.Command = KVS_PUT_INT;
    cmd.KeyAddress = (uintptr_t) key;
    cmd.Local = local;
    cmd.NewKey = 0;
    cmd.Success = 0;
    cmd.IntegerValue = value;
    cmd.ValueSize = 0;
    __s2e_touch_string(key);
    s2e_invoke_plugin("KeyValueStore", &cmd, sizeof(cmd));
    if (new_key) {
        *new_key = (int) cmd.NewKey;
    }
    return (int) cmd.Success;
}

#ifdef __cplusplus
}
#endif

#endif
