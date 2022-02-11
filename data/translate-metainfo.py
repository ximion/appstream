#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-2.1+

#
# This is a hack around msgfmt ignoring inline markup tags, and Meson not
# having itstool integration in old versions.
# FIXME: This is a terrible workaround that should go away as soon as possible.
#

import os
import sys
import shutil
import subprocess


def main(args):
    itstool_exe = args[0]
    input_fname = args[1]
    output_fname = args[2]
    domain = args[3]
    its_fname = args[4]
    locale_dir = args[5]

    temp_mo_dir = output_fname + '.mo'
    shutil.rmtree(temp_mo_dir, ignore_errors=True)
    os.makedirs(temp_mo_dir, exist_ok=True)

    mo_files = []
    for locale in args[6:]:
        locale = locale.strip()
        mo_src = os.path.join(locale_dir, locale, 'LC_MESSAGES', domain + '.mo')
        mo_dst = os.path.join(temp_mo_dir, locale + '.mo')
        shutil.copy(mo_src, mo_dst, follow_symlinks=False)
        mo_files.append(mo_dst)

    cmd = [itstool_exe,
            '-i', its_fname,
            '-j', input_fname,
            '-o', output_fname]
    cmd.extend(mo_files)
    subprocess.run(cmd, check=True)

    # cleanup
    shutil.rmtree(temp_mo_dir, ignore_errors=True)


if __name__ == '__main__':
    main(sys.argv[1:])
