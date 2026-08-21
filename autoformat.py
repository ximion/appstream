#!/usr/bin/env python3
#
# Copyright (C) 2015-2026 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-2.1+
#
# Format all AppStream source code in-place.
#

import os
import re
import sys
import shutil
import fnmatch
import subprocess
import tempfile
from glob import glob

# Minimum version of clang-format that we need
MIN_CLANG_FORMAT_VERSION = 22

# Directories (or single files) to format. Paths are relative to the source root.
INCLUDE_LOCATIONS = [
    'autoformat.py',
    'compose',
    'contrib',
    'data',
    'docs',
    'src',
    'qt',
    'tests',
    'tools',
]

# Files that must not be touched, as fnmatch patterns against the full path.
EXCLUDE_MATCH = [
    '*/build/*',
    '*/subprojects/*',
]

C_LIKE_SUFFIXES = ('.c', '.h', '.cpp', '.hpp')

# Extra rules layered on top of .clang-format when formatting our C headers.
#
# Headers are almost entirely declarations, and we write them as a table: the
# return type, the name and the parameter list each get their own column, and
# every parameter goes on its own line.
HEADER_STYLE_RULES = [
    # line up the declared names into a column
    'AlignConsecutiveDeclarations: AcrossEmptyLinesAndComments',
    # line up the values of enum members and other consecutive assignments
    'AlignConsecutiveAssignments: AcrossComments',
    # declarations keep their return type on the same line as the name
    'PenaltyReturnTypeOnItsOwnLine: 1000',
    # keep one parameter per line instead of collapsing declarations that
    # happen to fit within the column limit
    'BinPackParameters: AlwaysOnePerLine',
]


def check_tool(name, version_re=None, want_major=None):
    """Verify a formatter is present and, if asked, has the expected major version."""

    if not shutil.which(name):
        print(
            'The `{}` formatter is not installed. Please install it to continue!'.format(name),
            file=sys.stderr,
        )
        return False

    if version_re is None:
        return True

    out = subprocess.run([name, '--version'], capture_output=True, text=True).stdout
    m = re.search(version_re, out)
    if not m:
        print('Unable to determine the version of `{}`.'.format(name), file=sys.stderr)
        return False

    major = int(m.group(1))
    if major < want_major:
        print(
            'Found `{}` {}, but we need at least {} to format this tree.'.format(
                name, major, want_major
            ),
            file=sys.stderr,
        )
        return False

    return True


def nearest_style_file(path):
    """Return the `.clang-format` clang-format would pick for `path`, if any."""

    directory = os.path.dirname(os.path.abspath(path))
    while True:
        candidate = os.path.join(directory, '.clang-format')
        if os.path.isfile(candidate):
            return candidate
        parent = os.path.dirname(directory)
        if parent == directory:
            return None
        directory = parent


def merge_style(style_fname, extra_rules):
    """Return `style_fname` with `extra_rules` layered on top, as YAML text."""

    key_re = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):')

    def blocks(lines):
        result = []
        for line in lines:
            line = line.rstrip()
            if not line or line.lstrip().startswith('#') or line.startswith('---'):
                continue
            match = key_re.match(line)
            if match:
                result.append((match.group(1), [line]))
            elif result:
                result[-1][1].append(line)
        return result

    with open(style_fname, 'r') as f:
        base = blocks(f.readlines())
    extra = blocks(extra_rules)
    overrides = dict(extra)

    merged = []
    for key, lines in base:
        merged.extend(overrides.pop(key, lines) if key in overrides else lines)
    for key, lines in extra:
        if key in overrides:
            merged.extend(lines)

    return '\n'.join(merged) + '\n'


def run_clang_format(sources, style=None):
    """Run clang-format over `sources`, optionally with an explicit style file."""

    if not sources:
        return

    command = ['clang-format', '-i']
    if style:
        command.append('--style=file:{}'.format(style))
    command.extend(sources)
    subprocess.run(command, check=True)


def format_c_sources(sources):
    """Format C/C++ sources with clang-format.

    Sources are normally handed to clang-format without a --style, so that it
    resolves the nearest `.clang-format` itself and the Qt bindings keep their
    own C++ style. Our C headers are the exception: they get the base style
    plus HEADER_STYLE_RULES.
    """

    if not sources:
        return

    root_style = nearest_style_file(os.path.join(os.getcwd(), '.clang-format'))

    headers = []
    others = []
    for filename in sources:
        # only our C headers, i.e. those that a subdirectory .clang-format
        # (the Qt bindings) has not claimed for a different language
        if filename.endswith('.h') and nearest_style_file(filename) == root_style:
            headers.append(filename)
        else:
            others.append(filename)

    run_clang_format(others)

    if headers:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.clang-format') as fp:
            fp.write(merge_style(root_style, HEADER_STYLE_RULES))
            fp.flush()
            run_clang_format(headers, style=fp.name)


def format_python_sources(sources):
    """Format Python sources with Black."""

    if not sources:
        return

    command = [
        'black',
        '-S',  # no string normalization
        '-l',
        '100',  # line length
        '-t',
        'py311',  # minimum Python target
    ]
    command.extend(sources)
    subprocess.run(command, check=True)


def collect_sources(current_dir, locations):
    """Return (c_sources, py_sources) below the given locations."""

    c_sources = []
    py_sources = []

    for location in locations:
        path = os.path.join(current_dir, location)
        if os.path.isfile(path):
            candidates = [path]
        elif os.path.isdir(path):
            candidates = glob(path + '/**/*', recursive=True)
        else:
            print('Skipping `{}`: no such file or directory.'.format(location), file=sys.stderr)
            continue

        for filename in candidates:
            if not os.path.isfile(filename):
                continue
            if any(fnmatch.fnmatch(filename, pattern) for pattern in EXCLUDE_MATCH):
                continue

            if filename.endswith(C_LIKE_SUFFIXES):
                c_sources.append(filename)
            elif filename.endswith('.py'):
                py_sources.append(filename)

    return sorted(set(c_sources)), sorted(set(py_sources))


def run(current_dir, args):
    if not check_tool('clang-format', r'version (\d+)', MIN_CLANG_FORMAT_VERSION):
        return 1
    if not check_tool('black'):
        return 1

    c_sources, py_sources = collect_sources(current_dir, INCLUDE_LOCATIONS)
    if not c_sources and not py_sources:
        print('Nothing to format.', file=sys.stderr)
        return 1

    format_python_sources(py_sources)
    format_c_sources(c_sources)

    return 0


if __name__ == '__main__':
    thisfile = __file__
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))
    os.chdir(thisdir)

    sys.exit(run(thisdir, sys.argv[1:]))
