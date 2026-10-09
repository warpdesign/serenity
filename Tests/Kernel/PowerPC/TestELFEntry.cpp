/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

extern "C" int _invoke_entry(int argc, char** argv, char** envp, int (*entry)(int, char**, char**));

static char s_arg0[] = "serenity-ppc";
static char s_env0[] = "MODE=probe";
static char* s_argv[] = { s_arg0, nullptr };
static char* s_envp[] = { s_env0, nullptr };

static int entry(int argc, char** argv, char** envp)
{
    if (argc != 1 || argv != s_argv || envp != s_envp)
        return 0;
    if (argv[0] != s_arg0 || envp[0] != s_env0 || argv[1] || envp[1])
        return 0;
    return 0x750;
}

extern "C" int powerpc_elf_entry_test()
{
    return _invoke_entry(1, s_argv, s_envp, entry) == 0x750;
}
