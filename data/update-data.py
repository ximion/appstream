#!/usr/bin/env python3
#
# Copyright (C) 2015-2020 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-2.1+

#
# Script to download updated static data from the web and process it for AppStream to use.
#

import os
import json
import requests
import subprocess
from datetime import date
from tempfile import TemporaryDirectory


IANA_TLD_LIST_URL = 'https://data.iana.org/TLD/tlds-alpha-by-domain.txt'
SPDX_REPO_URL = 'https://github.com/spdx/license-list-data.git'


def get_tld_list(fname, url):
    print('Getting TLD list from IANA...')

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
        f.write('# IANA TLDs we recognize for AppStream IDs.\n')
        f.write('# Derived from the full list at {}\n'.format(url))
        f.write('# Last updated on {}\n'.format(date.today().isoformat()))

        f.write('\n'.join(data_result))
        f.write('\n')


def get_spdx_id_list(licenselist_fname, exceptionlist_fname, git_url, with_deprecated=True):
    print('Updating list of SPDX license IDs...')
    tdir = TemporaryDirectory(prefix='spdx_master-')

    subprocess.check_call(['git', 'clone', git_url, tdir.name])
    last_tag_ver = subprocess.check_output(['git', 'describe', '--abbrev=0',  '--tags'], cwd=tdir.name)
    last_tag_ver = str(last_tag_ver.strip(), 'utf-8')
    if last_tag_ver.startswith('v'):
        last_tag_ver = last_tag_ver[1:]

    # load license and exception data
    licenses_json_fname = os.path.join(tdir.name, 'json', 'licenses.json')
    exceptions_json_fname = os.path.join(tdir.name, 'json', 'exceptions.json')
    with open(licenses_json_fname, 'r') as f:
        licenses_data = json.loads(f.read())
    with open(exceptions_json_fname, 'r') as f:
        exceptions_data = json.loads(f.read())

    # get version of the data we are currently retrieving
    license_ver_ref = licenses_data.get('licenseListVersion')
    if not license_ver_ref:
        license_ver_ref = last_tag_ver
    exceptions_ver_ref = exceptions_data.get('licenseListVersion')
    if not license_ver_ref:
        exceptions_ver_ref = last_tag_ver

    lid_list = []
    for license in licenses_data['licenses']:
        lid_list.append(license['licenseId'])

    eid_list = []
    for exception in exceptions_data['exceptions']:
        eid_list.append(exception['licenseExceptionId'])

    lid_list.sort()
    with open(licenselist_fname, 'w') as f:
        f.write('# The list of licenses recognized by SPDX, v{}\n'.format(license_ver_ref))
        f.write('\n'.join(lid_list))
        f.write('\n')

    eid_list.sort()
    with open(exceptionlist_fname, 'w') as f:
        f.write('# The list of license exceptions recognized by SPDX, v{}\n'.format(exceptions_ver_ref))
        f.write('\n'.join(eid_list))
        f.write('\n')


def main():
    get_tld_list('iana-filtered-tld-list.txt', IANA_TLD_LIST_URL)
    get_spdx_id_list('spdx-license-ids.txt', 'spdx-license-exception-ids.txt', SPDX_REPO_URL)

    print('All done.')


if __name__ == '__main__':
    main()
