#!/usr/bin/env python3
#
# Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
from pathlib import Path


# additional CSS from system locations, we use it if available
EXTRA_CSS = [['/usr/share/javascript/highlight.js/styles/routeros.css',
              'highlight.css']]


def daps_build(src_dir, project_name, daps_exe):
    print('Creating HTML with DAPS...')
    sys.stdout.flush()

    build_dir = os.path.join(src_dir, '_docbuild')
    cmd = [daps_exe,
           'html',
           '--clean']
    if project_name:
        cmd.extend(['--name', project_name])

    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir, exist_ok=True)

    ret = subprocess.call(cmd, cwd=src_dir)
    if ret != 0:
        print('Documentation HTML build failed!')
        sys.exit(6)

    html_out_dir = os.path.join(build_dir, project_name, 'html', project_name)

    # copy the (usually missing) plain SVG project icon
    shutil.copy(os.path.join(src_dir, 'images', 'src', 'svg', 'appstream-logo.svg'),
                os.path.join(html_out_dir, 'images'))

    # copy extra CSS if it is available
    for css_fname in EXTRA_CSS:
        if os.path.exists(css_fname[0]):
            shutil.copy(css_fname[0], os.path.join(html_out_dir, 'static',
                                                   'css', css_fname[1]))

    return build_dir


def copy_result(build_dir, project_name, dest_dir):
    print('Copying HTML documentation...')
    if os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)

    shutil.copytree(os.path.join(build_dir, project_name, 'html', project_name),
                    dest_dir, symlinks=True)


def cleanup_build_dir(build_dir):
    print('Cleaning up.')
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)


def daps_validate(src_dir, daps_exe):
    print('Validating documentation with DAPS...')

    build_dir = os.path.join(src_dir, '_docbuild')
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir, exist_ok=True)

    ret = subprocess.call([daps_exe, 'validate'], cwd=src_dir)
    if ret != 0:
        print('Validation failed!')
    cleanup_build_dir(build_dir)
    return ret == 0


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--validate', action='store_true')

    parser.add_argument('--src', action='store')
    parser.add_argument('--builddir', action='store')
    parser.add_argument('--daps', action='store', default='daps')
    parser.add_argument('project', action='store', default='AppStream', nargs='?')

    options = parser.parse_args(args)

    if not options.validate and not options.build:
        print('No action was specified.')
        return 1

    if not options.src:
        print('Source root directory is not defined!')
        return 1

    os.chdir(options.src)

    if options.build:
        # build the HTML files
        build_dir = daps_build(options.src, options.project, options.daps)

        # copy to output HTML folder, overriding all previous contents
        copy_result(build_dir, options.project,
                    os.path.join(options.src, 'html'))

        # remove temporary directory
        cleanup_build_dir(build_dir)

        # make a dummy file so Meson can rebuild documentation on demand
        if options.builddir:
            Path(os.path.join(options.builddir, 'docs_built.stamp')).touch()

        print('Documentation built.')

    elif options.validate:
        # validate the XML
        ret = daps_validate(options.src, options.daps)
        if not ret:
            sys.exit(6)

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv[1:]))
