# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


__all__ = (
    "cmake_dir_set",
    "build_info",
    "SOURCE_DIR",
    "CMAKE_DIR",
)


import sys
if sys.version_info.major < 3:
    print("\nPython3.x or newer needed, found %s.\nAborting!\n" %
          sys.version.partition(" ")[0])
    sys.exit(1)


import os
from os.path import join, dirname, normpath, abspath

import subprocess

from typing import (
    Any,
    IO,
)
from collections.abc import (
    Callable,
    Iterator,
    Sequence,
)

import shlex


SOURCE_DIR = join(dirname(__file__), "..", "..")
SOURCE_DIR = normpath(SOURCE_DIR)
SOURCE_DIR = abspath(SOURCE_DIR)

# copied from project_info.py
CMAKE_DIR = "."


def cmake_dir_set(cmake_dir: str) -> None:
    """
    Callers may not run this tool from the CWD, in this case,
    allow the value to be set.
    """
    # Use a method in case any other values need to be updated in the future.
    global CMAKE_DIR
    CMAKE_DIR = cmake_dir


def is_c_header(filename: str) -> bool:
    ext = os.path.splitext(filename)[1]
    return (ext in {".h", ".hpp", ".hxx", ".hh"})


def is_c(filename: str) -> bool:
    ext = os.path.splitext(filename)[1]
    return (ext in {".c", ".cpp", ".cxx", ".m", ".mm", ".rc", ".cc", ".inl", ".osl"})


def is_c_any(filename: str) -> bool:
    return is_c(filename) or is_c_header(filename)


def cmake_cache_var_iter() -> Iterator[tuple[str, str, str]]:
    import re
    re_cache = re.compile(r'([A-Za-z0-9_\-]+)?:?([A-Za-z0-9_\-]+)?=(.*)$')
    with open(join(CMAKE_DIR, "CMakeCache.txt"), 'r', encoding='utf-8') as cache_file:
        for l in cache_file:
            match = re_cache.match(l.strip())
            if match is not None:
                var, type_, val = match.groups()
                yield (var, type_ or "", val)


def cmake_cache_var(var: str) -> str | None:
    for var_iter, _type_iter, value_iter in cmake_cache_var_iter():
        if var == var_iter:
            return value_iter
    return None


def cmake_cache_var_or_exit(var: str) -> str:
    value = cmake_cache_var(var)
    if value is None:
        print("Unable to find %r exiting!" % value)
        sys.exit(1)
    return value


def do_ignore(filepath: str, ignore_prefix_list: Sequence[str] | None) -> bool:
    if ignore_prefix_list is None:
        return False

    relpath = os.path.relpath(filepath, SOURCE_DIR)
    return any([relpath.startswith(prefix) for prefix in ignore_prefix_list])


def makefile_log() -> list[str]:

    # support both make and ninja
    make_exe = cmake_cache_var_or_exit("CMAKE_MAKE_PROGRAM")

    make_exe_basename = os.path.basename(make_exe)

    if make_exe_basename.startswith(("make", "gmake")):
        print("running 'make' with --dry-run ...")
        with subprocess.Popen(
            (
                make_exe,
                "-C", CMAKE_DIR,
                "--always-make",
                "--dry-run",
                "--keep-going",
                "VERBOSE=1",
            ),
            stdout=subprocess.PIPE,
        ) as proc:
            stdout_data, stderr_data = proc.communicate()

    elif make_exe_basename.startswith("ninja"):
        print("running 'ninja' with -t commands ...")
        with subprocess.Popen(
            (
                make_exe,
                "-C", CMAKE_DIR,
                "-t", "commands",
            ),
            stdout=subprocess.PIPE,
        ) as proc:
            stdout_data, stderr_data = proc.communicate()
    else:
        print("CMAKE_MAKE_PROGRAM: \"{:s}\" is not known (make/gmake/ninja)")
        sys.exit(1)
    del stderr_data

    print("done!", len(stdout_data), "bytes")
    return stdout_data.decode("utf-8", errors="ignore").split("\n")


def build_info(
        use_c: bool = True,
        use_cxx: bool = True,
        ignore_prefix_list: list[str] | None = None,
) -> list[tuple[str, list[str], list[str]]]:
    makelog = makefile_log()

    source = []

    compilers = []
    if use_c:
        compilers.append(cmake_cache_var_or_exit("CMAKE_C_COMPILER"))
    if use_cxx:
        compilers.append(cmake_cache_var_or_exit("CMAKE_CXX_COMPILER"))

    print("compilers:", " ".join(compilers))

    fake_compiler = "%COMPILER%"

    print("parsing make log ...")

    for line in makelog:
        args_orig: str | list[str] = line.split()
        args = [fake_compiler if c in compilers else c for c in args_orig]
        if args == args_orig:
            # No compilers in the command, skip.
            continue
        del args_orig

        # Join arguments in case they are not.
        args_str = " ".join(args)
        args_str = args_str.replace(" -isystem", " -I")
        args_str = args_str.replace(" -D ", " -D")
        args_str = args_str.replace(" -I ", " -I")

        args = shlex.split(args_str)
        del args_str
        # end

        # remove compiler
        args[:args.index(fake_compiler) + 1] = []

        c_files = [f for f in args if is_c(f)]
        inc_dirs = [f[2:].strip() for f in args if f.startswith('-I')]
        defs = [f[2:].strip() for f in args if f.startswith('-D')]
        for c in sorted(c_files):

            if do_ignore(c, ignore_prefix_list):
                continue

            source.append((c, inc_dirs, defs))

        # make relative includes absolute
        # not totally essential but useful
        for i, f in enumerate(inc_dirs):
            if not os.path.isabs(f):
                inc_dirs[i] = os.path.abspath(os.path.join(CMAKE_DIR, f))

        # safety check that our includes are ok
        for f in inc_dirs:
            if not os.path.exists(f):
                raise Exception("%s missing" % f)

    print("done!")

    return source


def build_defines_as_source() -> str:
    """
    Returns a string formatted as an include:
        '#defines A=B\n#define....'
    """
    # Works for both GCC and CLANG.
    cmd = (cmake_cache_var_or_exit("CMAKE_C_COMPILER"), "-dM", "-E", "-")
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stdin=subprocess.DEVNULL,
    )

    # We know this is always true based on the input arguments to `Popen`.
    assert process.stdout is not None
    stdout: IO[bytes] = process.stdout

    return stdout.read().strip().decode('ascii')


def build_defines_as_args() -> list[str]:
    return [
        ("-D" + "=".join(l.split(maxsplit=2)[1:]))
        for l in build_defines_as_source().split("\n")
        if l.startswith('#define')
    ]


def process_make_non_blocking(proc: subprocess.Popen[Any]) -> subprocess.Popen[Any]:
    import fcntl
    for fh in (proc.stderr, proc.stdout):
        if fh is None:
            continue
        fd = fh.fileno()
        fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
    return proc


# Could be moved elsewhere!, this just happens to be used by scripts that also
# use this module.
def queue_processes(
        process_funcs: Sequence[tuple[Callable[..., subprocess.Popen[Any]], tuple[Any, ...]]],
        *,
        job_total: int = -1,
        sleep: float = 0.1,
        process_finalize: Callable[[subprocess.Popen[Any], bytes, bytes], int | None] | None = None,
) -> None:
    """
    Takes a list of function argument pairs, each function must return a process.
    """

    if job_total == -1:
        import multiprocessing
        job_total = multiprocessing.cpu_count()
        del multiprocessing

    if job_total == 1:
        for func, args in process_funcs:
            sys.stdout.flush()
            sys.stderr.flush()

            process = func(*args)
            if process_finalize is not None:
                data = process.communicate()
                process_finalize(process, *data)
    else:
        import time

        if process_finalize is not None:
            def poll_and_finalize(
                    p: subprocess.Popen[Any],
                    stdout: list[bytes],
                    stderr: list[bytes],
            ) -> int | None:
                assert p.stdout is not None
                if data := p.stdout.read():
                    stdout.append(data)
                assert p.stderr is not None
                if data := p.stderr.read():
                    stderr.append(data)

                if (returncode := p.poll()) is not None:
                    data_stdout, data_stderr = p.communicate()
                    if data_stdout:
                        stdout.append(data_stdout)
                    if data_stderr:
                        stderr.append(data_stderr)
                    process_finalize(p, b"".join(stdout), b"".join(stderr))
                return returncode
        else:
            def poll_and_finalize(
                    p: subprocess.Popen[Any],
                    stdout: list[bytes],
                    stderr: list[bytes],
            ) -> int | None:
                return p.poll()

        processes: list[tuple[subprocess.Popen[Any], list[bytes], list[bytes]]] = []
        for func, args in process_funcs:
            # wait until a thread is free
            while 1:
                processes[:] = [p_item for p_item in processes if poll_and_finalize(*p_item) is None]

                if len(processes) <= job_total:
                    break
                time.sleep(sleep)

            sys.stdout.flush()
            sys.stderr.flush()

            processes.append((process_make_non_blocking(func(*args)), [], []))

        # Don't return until all jobs have finished.
        while 1:
            processes[:] = [p_item for p_item in processes if poll_and_finalize(*p_item) is None]

            if not processes:
                break
            time.sleep(sleep)


def main() -> None:
    if not os.path.exists(join(CMAKE_DIR, "CMakeCache.txt")):
        print("This script must run from the cmake build dir")
        return

    for s in build_info():
        print(s)


if __name__ == "__main__":
    main()
