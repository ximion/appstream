#!/usr/bin/env python3
#
# Copyright (C) 2015 Matthias Klumpp <mak@debian.org>
# Licensed under LGPL-2.0+

#
# Script to download updated static data from the web and process it for AppStream to use.
#

import os
import requests
import subprocess
from datetime import date
from tempfile import TemporaryDirectory

def get_tld_list(fname, url):
    print("Getting TLD list from IANA...")

    data_result = list()
    r = requests.get(url)
    raw_data = str(r.content, 'utf-8')
    for line in raw_data.split('\n'):
        line = line.strip()
        if not line:
            continue
        if line.startswith('#'):
            continue

        # filter out internationalized names, we don't support them for AppStream IDs
        if line.startswith('XN--'):
            continue

        # we disallow the very long names (usually brands)
        # FIXME: Do we really want to impose this restriction just to keep the TLD
        # pool small?
        if len(line) > 4:
            continue

        data_result.append(line.lower())

    data_result.sort()
    with open(fname, 'w') as f:
        f.write("# IANA TLDs we recognize for AppStream IDs.\n")
        f.write("# Derived from the full list at {}\n".format(url))
        f.write("# Last updated on {}\n".format(date.today().isoformat()))

        f.write("\n".join(data_result))
        f.write('\n')


def get_spdx_id_list(fname, git_url, with_deprecated=True):
    print("Updating list of SPDX license IDs...")
    tdir = TemporaryDirectory(prefix="spdx_master-")

    subprocess.check_call(['git', 'clone', git_url, tdir.name])
    last_tag_name = subprocess.check_output(['git', 'describe', '--abbrev=0',  '--tags'], cwd=tdir.name)
    last_tag_name = str(last_tag_name.strip(), 'utf-8')

    licenses_text_dir = os.path.join(tdir.name, 'text')

    id_list = list()
    # get SPDX license-IDs from filenames
    for spdx_fname in os.listdir(licenses_text_dir):
        if not spdx_fname.endswith('.txt'):
            continue
        raw_id = os.path.splitext(spdx_fname)[0]
        # check if this is actually an SPDX ID file
        if ' ' in raw_id:
            continue
        if raw_id.startswith('deprecated_'):
            if with_deprecated:
                raw_id = raw_id[11:]
            else:
                continue
        id_list.append(raw_id)

    id_list.sort()
    with open(fname, 'w') as f:
        f.write("# The list of licenses recognized by SPDX, {}\n".format(last_tag_name))
        f.write("# Source: http://spdx.org/licenses/\n")

        f.write("\n".join(id_list))
        f.write('\n')

def main():
    get_tld_list     ("iana-filtered-tld-list.txt", "https://data.iana.org/TLD/tlds-alpha-by-domain.txt")
    get_spdx_id_list ("spdx-license-ids.txt", "https://github.com/spdx/license-list-data.git")

    print("All done.")

if __name__ == '__main__':
    main()
