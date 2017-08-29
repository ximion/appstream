#!/usr/bin/env python3
#
# Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the license, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#
# This is a helper script for Meson and intended to be called by the
# AppStream buildsystem. You do not want to run this manually.
#

import os
import argparse
import subprocess
import shutil
from pathlib import Path


parser = argparse.ArgumentParser()
parser.add_argument('--ws')
parser.add_argument('--src')
parser.add_argument('--out')
parser.add_argument('--publican', default='publican')
parser.add_argument('project')
parser.add_argument('version')


def assemble_ws(src_dir, ws_dir):
    print('Assembling workspace...')
    if os.path.exists(ws_dir):
        shutil.rmtree(ws_dir)

    shutil.copytree(os.path.join(src_dir, 'sources'), os.path.join(ws_dir, 'en-US'), symlinks=True)
    shutil.copy2(os.path.join(src_dir, 'publican.cfg'), os.path.join(ws_dir, 'publican.cfg'))


def run_publican(ws_dir, publican_exe):
    print('Running Publican...')
    return subprocess.check_call([publican_exe,
                                  'build',
                                  '--src_dir=' + ws_dir,
                                  '--pub_dir=' + os.path.join(ws_dir, 'public'),
                                  '--langs=en-US',
                                  '--publish',
                                  '--formats=html'])


def copy_result(project, version, ws_dir, out_dir):
    print('Copying result...')
    if os.path.exists(out_dir):
        shutil.rmtree(out_dir)

    shutil.copytree(os.path.join(ws_dir, 'public', 'en-US', project, version, 'html', project),
                    out_dir, symlinks=True)


def main(args):
    options = parser.parse_args(args)

    # some control over where Publican puts its tmp dir
    os.chdir(options.ws)

    if not options.ws:
        print('You need to define a temporary workspace directory!')
        return 1

    if not options.src:
        print('You need to define a source directory!')
        return 1

    if not options.out:
        print('You need to define an output directory!')
        return 1

    ws_dir = os.path.join(options.ws, 'publican_ws')

    # create temporary workspace for Publican, outside of the source tree
    assemble_ws(options.src, ws_dir)

    # make HTML
    run_publican(ws_dir, options.publican)

    # copy to output folder, overrding its contents
    copy_result(options.project, options.version, ws_dir, os.path.join(options.out, 'html'))

    # make a dummy file so Meson can rebuild documentation on demand
    Path(os.path.join(options.ws, 'docs_built.stamp')).touch()

    print('Documentation built.')
    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv[1:]))
