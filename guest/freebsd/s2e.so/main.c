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


#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <s2e/s2e.h>

#include "function_models.h"
#include "s2e_so.h"

#define MAX_S2E_SYM_ARGS_SIZE 44

uint8_t g_enable_function_models = 0;

static void __emit_error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/// Make the command line arguments listed in S2E_SYM_ARGS symbolic (same syntax as the Linux s2e.so).
static void initialize_cmdline(int argc, char **argv) {
    char *sym_args = getenv("S2E_SYM_ARGS");
    if (!sym_args) {
        s2e_warning("S2E_SYM_ARGS is not set. All arguments will be concrete");
        return;
    }

    char sym_arg_name[MAX_S2E_SYM_ARGS_SIZE];
    int i = 0;

    size_t str_args_len = strlen(sym_args);

    // 1 - symbolic, 0 - concrete
    char *args_type = (char *) calloc(argc, sizeof(char));
    if (!args_type) {
        __emit_error("Memory allocation failed");
    }

    int valid = 1;
    char *str_tmp = sym_args;
    while ((size_t) (str_tmp - sym_args) < str_args_len) {
        char *end_ptr;
        long arg_num = strtol(str_tmp, &end_ptr, 10);
        if (end_ptr == str_tmp) {
            valid = 0;
            break;
        }

        if (arg_num >= 0 && arg_num < argc && errno != ERANGE) {
            args_type[arg_num] = 1;
        } else {
            valid = 0;
        }

        str_tmp = end_ptr;
    }

    if (!valid) {
        s2e_warning("S2E_SYM_ARGS contains incorrect configuration\n");
    }

    for (i = 0; i < argc; i++) {
        if (args_type[i]) {
            snprintf(sym_arg_name, MAX_S2E_SYM_ARGS_SIZE, "arg%d", i);
            s2e_make_symbolic(argv[i], strlen(argv[i]), sym_arg_name);
        }
    }

    free(args_type);
}

///
/// FreeBSD's rtld calls the functions in .init_array with (argc, argv, envp),
/// so there is no need to interpose __libc_start_main (which does not exist
/// here anyway). All the shared objects of the process are mapped by the
/// time this runs, before main() and before the constructors of the executable.
///
static void __attribute__((constructor)) s2e_so_init(int argc, char **argv, char **envp) {
    (void) envp;

    initialize_models();
    s2e_load_modules();

    initialize_cmdline(argc, argv);

    g_enable_function_models = s2e_plugin_loaded("FunctionModels");
}
