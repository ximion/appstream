#!/usr/bin/env python3
#
# Copyright (C) 2024-2025 Matthias Klumpp <matthias@tenstral.net>
#
# Licensed under the GNU Lesser General Public License Version 2.1
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 2.1 of the license, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#
# This is a helper script for Meson and intended to be called by the
# AppStream buildsystem. You do not want to run this manually.
#

import os
import argparse
import subprocess
import shutil


def run_gidocgen_generate(gidocgen_exe, output_dir, namespace, extra_args):
    """Work around a quirk in gi-docgen that results in misnamed devhelp files."""
    cmd = [
        gidocgen_exe,
        'generate',
        '--quiet',
        '--no-namespace-dir',
    ]

    post_rename = False
    if namespace and output_dir:
        post_rename = True
        cmd.extend(['--output-dir', namespace])
    elif output_dir:
        cmd.extend(['--output-dir', output_dir])
    cmd.extend(extra_args)

    ret = subprocess.call(cmd)

    if post_rename:
        # unfortunately for some reason, gi-docgen takes the output directory
        # name as namespace name, resulting in misnamed devhelp files.
        # We work around this here.
        if os.path.isdir(output_dir):
            shutil.rmtree(output_dir)
        shutil.move(namespace, output_dir)

    return ret == 0


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--gidocgen', action='store', default='gi-docgen')
    parser.add_argument('--output-dir', action='store')
    parser.add_argument('--namespace', action='store')

    options, extra_args = parser.parse_known_args(args)

    ret = run_gidocgen_generate(options.gidocgen, options.output_dir, options.namespace, extra_args)
    if not ret:
        sys.exit(1)

    return 0


if __name__ == '__main__':
    import sys

    sys.exit(main(sys.argv[1:]))
