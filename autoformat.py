#!/usr/bin/env python3
#
# Copyright (C) 2015-2022 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-2.1+
#
# Format all AppStream source code in-place.
#

import os
import sys
import shutil
import fnmatch
import subprocess
import tempfile
from glob import glob

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

EXCLUDE_MATCH = []

EXTRA_STYLE_RULES_FOR = [
    (
        [
            'AlignConsecutiveDeclarations: AcrossEmptyLinesAndComments',
            'AlignConsecutiveAssignments: AcrossComments',
            'PenaltyReturnTypeOnItsOwnLine: 1000',
        ],
        [
            '*/compose/*.h',
            '*/contrib/*.h',
            '*/data/*.h',
            '*/docs/*.h',
            '*/src/*.h',
            '*/tests/*.h',
            '*/tools/*.h',
        ],
    ),
]


def format_cpp_sources(sources, style_fname=None, extra_styles: list[str] = None):
    """Format C/C++ sources with clang-format."""

    if not sources:
        return

    command = ['clang-format', '-i']

    if extra_styles:
        style_rules = []
        if style_fname:
            with open(style_fname, 'r') as f:
                style_rules = [l.strip() for l in f.readlines()]

        with tempfile.NamedTemporaryFile(mode='w') as fp:
            style_rules.extend(extra_styles)
            fp.write('\n'.join(style_rules))
            fp.flush()

            command.append('--style=file:{}'.format(fp.name))
            command.extend(sources)
            subprocess.run(command, check=True)
            return

    if style_fname:
        command.append('--style=file:{}'.format(style_fname))

    command.extend(sources)
    subprocess.run(command, check=True)


def format_python_sources(sources):
    """Format Python sources with Black."""

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


def run(current_dir, args):
    # check for tools
    if not shutil.which('clang-format'):
        print(
            'The `clang-format` formatter is not installed. Please install it to continue!',
            file=sys.stderr,
        )
        return 1
    if not shutil.which('black'):
        print(
            'The `black` formatter is not installed. Please install it to continue!',
            file=sys.stderr,
        )
        return 1

    # if no include directories are explicitly specified, we read all locations
    if not INCLUDE_LOCATIONS:
        INCLUDE_LOCATIONS.append('.')

    # collect sources
    cpp_sources = []
    cpp_style_matches = [[]] * len(EXTRA_STYLE_RULES_FOR)
    py_sources = []
    for il_path_base in INCLUDE_LOCATIONS:
        il_path = os.path.join(current_dir, il_path_base)
        if os.path.isfile(il_path):
            candidates = [il_path]
        else:
            candidates = glob(il_path + '/**/*', recursive=True)

        for filename in candidates:
            skip = False
            for exclude in EXCLUDE_MATCH:
                if fnmatch.fnmatch(filename, exclude):
                    skip = True
                    break
            if skip:
                continue

            if filename.endswith(('.c', '.cpp', '.h', '.hpp')):
                cpp_sources.append(filename)

                for i, er in enumerate(EXTRA_STYLE_RULES_FOR):
                    for pattern in er[1]:
                        if fnmatch.fnmatch(filename, pattern):
                            cpp_style_matches[i].append(filename)
                            break

            elif filename.endswith('.py'):
                py_sources.append(filename)

    # format
    format_python_sources(py_sources)
    format_cpp_sources(cpp_sources)

    for i, er in enumerate(EXTRA_STYLE_RULES_FOR):
        format_cpp_sources(
            cpp_style_matches[i],
            os.path.join(current_dir, '.clang-format'),
            er[0],
        )

    return 0


if __name__ == '__main__':
    thisfile = __file__
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))
    os.chdir(thisdir)

    sys.exit(run(thisdir, sys.argv[1:]))
