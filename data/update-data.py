#!/usr/bin/env python3
#
# Copyright (C) 2015-2022 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-2.1+

#
# Script to download updated static data from the web and process it for AppStream to use.
#

import os
import io
import json
import requests
import subprocess
import yaml
from datetime import date
from lxml import etree
from tempfile import TemporaryDirectory


IANA_TLD_LIST_URL = 'https://data.iana.org/TLD/tlds-alpha-by-domain.txt'
SPDX_REPO_URL = 'https://github.com/spdx/license-list-data.git'
MENU_SPEC_URL = 'https://gitlab.freedesktop.org/xdg/xdg-specs/raw/master/menu/menu-spec.xml'
XPATH_MAIN_CATEGORIES = (
    '/article/appendix[@id="category-registry"]/sect1[@id="main-category-registry"]//tbody/row/*[1]'
)
XPATH_ADDITIONAL_CATEGORIES = '/article/appendix[@id="category-registry"]/sect1[@id="additional-category-registry"]//tbody/row/*[1]'


def update_tld_list(url, fname):
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


def _read_spdx_licenses(data_dir, last_tag_ver, only_free=False):
    # load license and exception data
    licenses_json_fname = os.path.join(data_dir, 'json', 'licenses.json')
    exceptions_json_fname = os.path.join(data_dir, 'json', 'exceptions.json')
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

    lic_list = []
    for license in licenses_data['licenses']:
        if only_free:
            if not license.get('isFsfLibre') and not license.get('isOsiApproved'):
                continue
        lic_list.append({'id': license['licenseId'], 'name': license['name']})

    exc_list = []
    for exception in exceptions_data['exceptions']:
        exc_list.append({'id': exception['licenseExceptionId'], 'name': exception['name']})

    return {
        'licenses': sorted(lic_list, key=lambda x: x['id']),
        'exceptions': sorted(exc_list, key=lambda x: x['id']),
        'license_list_ver': license_ver_ref,
        'exception_list_ver': exceptions_ver_ref,
    }


def _read_dfsg_free_license_list():
    '''Obtain a (manually curated) list of DFSG-free licenses'''
    licenses = set()
    with open('dfsg-free-licenses.txt', 'r') as f:
        for line in f.readlines():
            if line.startswith('#'):
                continue
            licenses.add(line.strip())
    return licenses


def update_spdx_id_list(
    git_url,
    header_template_fname,
    licenselist_header_fname,
    licenselist_free_fname,
    with_deprecated=True,
):
    print('Updating list of SPDX license IDs...')
    tdir = TemporaryDirectory(prefix='spdx_master-')

    subprocess.check_call(['git', 'clone', git_url, tdir.name])
    last_tag_ver = subprocess.check_output(
        ['git', 'describe', '--abbrev=0', '--tags'], cwd=tdir.name
    )
    last_tag_ver = str(last_tag_ver.strip(), 'utf-8')
    if last_tag_ver.startswith('v'):
        last_tag_ver = last_tag_ver[1:]

    license_data = _read_spdx_licenses(tdir.name, last_tag_ver)
    license_list = license_data['licenses']
    exception_list = license_data['exceptions']
    license_list_ver = license_data['license_list_ver']

    with open(header_template_fname, 'r') as f:
        header_tmpl = f.read()

    with open(licenselist_header_fname, 'w') as f:
        lic_array_def = ''
        exc_array_def = ''
        for license in license_list:
            lic_id = license['id'].replace('"', '\\"')
            lic_name = license['name'].replace('"', '\\"')
            lic_array_def += f'\t{{"{lic_id}", N_("{lic_name}")}},\n'
        for exception in exception_list:
            exc_id = exception['id'].replace('"', '\\"')
            exc_name = exception['name'].replace('"', '\\"')
            exc_array_def += f'\t{{"{exc_id}", N_("{exc_name}")}},\n'

        lic_array_def += f'\t{{NULL, NULL}}'
        exc_array_def += f'\t{{NULL, NULL}}'

        header = header_tmpl.replace('@LICENSE_INFO_DEFS@', lic_array_def.strip())
        header = header.replace('@EXCEPTION_INFO_DEFS@', exc_array_def.strip())
        header = header.replace('@LICENSE_LIST_VERSION@', license_list_ver)
        header = header.replace('@EXCEPTION_LIST_VERSION@', license_data['exception_list_ver'])

        f.write(header)

    license_free_data = _read_spdx_licenses(tdir.name, last_tag_ver, only_free=True)
    with open(licenselist_free_fname, 'w') as f:
        f.write(
            '# The list of free (OSI, FSF approved or DFSG-free) licenses recognized by SPDX, v{}\n'.format(
                license_list_ver
            )
        )
        lid_set = set([d['id'] for d in license_list])
        free_lid_set = set([d['id'] for d in license_free_data['licenses']])
        for dfsg_lid in _read_dfsg_free_license_list():
            if dfsg_lid not in lid_set:
                raise Exception(
                    'Unknown license-ID "{}" found in DFSG-free license list!'.format(dfsg_lid)
                )
            free_lid_set.add(dfsg_lid)
        f.write('\n'.join(sorted(free_lid_set)))
        f.write('\n')


def update_platforms_data():
    print('Updating platform triplet part data...')

    with open('platforms.yml', 'r') as f:
        data = yaml.safe_load(f)

    def write_platform_data(fname, values):
        with open(fname, 'w') as f:
            f.write('# This file is derived from platforms.yml - DO NOT EDIT IT MANUALLY!\n')
            f.write('\n'.join(values))
            f.write('\n')

    write_platform_data('platform_arch.txt', data['architectures'])
    write_platform_data('platform_os.txt', data['os_kernels'])
    write_platform_data('platform_env.txt', data['os_environments'])


def update_categories_list(spec_url, cat_fname):
    print('Updating XDG categories data...')

    req = requests.get(spec_url)
    tree = etree.parse(io.BytesIO(req.content))

    all_cat_names = []
    entries = tree.xpath(XPATH_MAIN_CATEGORIES)
    assert len(entries) > 0
    all_cat_names.extend([e.text for e in entries])
    entries = tree.xpath(XPATH_ADDITIONAL_CATEGORIES)
    assert len(entries) > 0
    all_cat_names.extend([e.text for e in entries])
    all_cat_names.sort()

    with open(cat_fname, 'w') as f:
        f.write('# Freedesktop Menu Categories\n')
        f.write('# See https://specifications.freedesktop.org/menu-spec/latest/apa.html\n')
        f.write('\n'.join(all_cat_names))
        f.write('\n')


def main():
    data_dir = os.path.dirname(os.path.abspath(__file__))
    print('Data directory is: {}'.format(data_dir))
    os.chdir(data_dir)

    update_tld_list(IANA_TLD_LIST_URL, 'iana-filtered-tld-list.txt')
    update_spdx_id_list(
        SPDX_REPO_URL,
        os.path.join(data_dir, 'spdx-license-header.tmpl'),
        '../src/as-spdx-data.h',
        'spdx-free-license-ids.txt',
    )
    update_categories_list(MENU_SPEC_URL, 'xdg-category-names.txt')
    update_platforms_data()

    print('All done.')


if __name__ == '__main__':
    main()
