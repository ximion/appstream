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


def _read_spdx_licenses(data_dir, last_tag_ver):
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
        is_free = True if license.get('isFsfLibre') or license.get('isOsiApproved') else False
        lic_list.append({'id': license['licenseId'], 'name': license['name'], 'is_free': is_free})

    exc_list = []
    for exception in exceptions_data['exceptions']:
        exc_list.append({'id': exception['licenseExceptionId'], 'name': exception['name']})

    return {
        'licenses': sorted(lic_list, key=lambda x: x['id']),
        'exceptions': sorted(exc_list, key=lambda x: x['id']),
        'license_list_ver': license_ver_ref,
        'exception_list_ver': exceptions_ver_ref,
    }


def _read_dfsg_free_license_set():
    '''Obtain a (manually curated) list of DFSG-free licenses'''
    licenses = set()
    with open('dfsg-free-licenses.txt', 'r') as f:
        for line in f.readlines():
            if line.startswith('#'):
                continue
            licenses.add(line.strip())
    return licenses


def _write_c_array_header(tmpl_fname, out_fname, l10n_keys=None, l10n_hints=None, **kwargs):
    """Create a C header with lists based on a template."""

    if not l10n_keys:
        l10n_keys = set()
    l10n_keys = set(l10n_keys)
    if not l10n_hints:
        l10n_hints = {}

    with open(tmpl_fname, 'r') as f:
        header = f.read()

    for key, data in kwargs.items():
        if isinstance(data, list):
            if not data:
                continue

            array_def = ''
            for entry in data:
                entry_c = ''
                l10n_ignore = set()
                if key in l10n_hints:
                    tr_hint = l10n_hints[key]
                    only_matches = tr_hint.get('only_matches', [])

                    # ignore match terms for translation
                    if only_matches:
                        l10n_ignore = l10n_keys.copy()
                        for ms in only_matches:
                            for k, v in entry.items():
                                if k not in l10n_ignore:
                                    continue
                                if not isinstance(v, str):
                                    continue
                                if ms in v:
                                    l10n_ignore.remove(k)

                    if not l10n_keys.issubset(l10n_ignore):
                        v = list(entry.values())[0]
                        entry_c += '\t/* TRANSLATORS: {} */\n'.format(tr_hint['msg'].format(v))

                entry_c += '\t{'
                for k, v in entry.items():
                    if isinstance(v, str):
                        v = '"' + v.replace('"', '\\"') + '"'
                    elif isinstance(v, bool):
                        v = "TRUE" if v else "FALSE"
                    else:
                        raise ValueError('Invalid type for array value: %s' % type(v))

                    if k in l10n_keys and k not in l10n_ignore:
                        v_c = f'N_({v})'
                    else:
                        v_c = f'{v}'
                    entry_c += f' {v_c},'
                array_def += entry_c[:-1] + ' },\n'

            zero_term = ''
            for _, v in data[0].items():
                if isinstance(v, bool):
                    zero_term += 'FALSE, '
                else:
                    zero_term += 'NULL, '
            array_def += '\t{ ' + zero_term[:-2] + ' },\n'

            header = header.replace(f'@{key}@', array_def.strip())
        else:
            header = header.replace(f'@{key}@', str(data))

    with open(out_fname, 'w') as f:
        f.write(header)


def update_spdx_id_list(
    git_url,
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

    # read all SPDX license data
    license_data = _read_spdx_licenses(tdir.name, last_tag_ver)
    license_list = license_data['licenses']
    exception_list = license_data['exceptions']
    license_list_ver = license_data['license_list_ver']

    # augment free-ness info with DFSG extension list
    dfsg_extension = _read_dfsg_free_license_set()
    for license in license_list:
        lic_id = license['id']
        if lic_id in dfsg_extension:
            license['is_free'] = True
            dfsg_extension.remove(lic_id)

    # extension list should be empty by now
    if dfsg_extension:
        raise Exception(
            'Unknown license-ID "{}" found in DFSG-free license list!'.format(dfsg_extension[0])
        )

    _write_c_array_header(
        'spdx-license-header.tmpl',
        licenselist_header_fname,
        ['name'],
        l10n_hints={
            'LICENSE_INFO_DEFS': {
                'msg': 'Please do not translate the license name itself.',
                'only_matches': [' only', ' or later'],
            },
            'EXCEPTION_INFO_DEFS': {
                'msg': 'Please do not translate the license exception name itself.',
                'only_matches': [' only', ' or later'],
            },
        },
        LICENSE_INFO_DEFS=license_list,
        EXCEPTION_INFO_DEFS=exception_list,
        LICENSE_LIST_VERSION=license_list_ver,
        EXCEPTION_LIST_VERSION=license_data['exception_list_ver'],
    )


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


def update_gui_env_ids(data_header_fname):
    print('Updating GUI environment IDs...')

    desktops_list = []
    desktops_set = set()
    with open('desktop-environments.txt', 'r') as f:
        for line in f.readlines():
            if line.startswith('#'):
                continue
            de_id, de_name = line.strip().split(' ', 1)
            de_id = de_id.strip()
            if not de_id:
                continue
            desktops_set.add(de_id.lower())
            desktops_list.append({'id': de_id, 'name': de_name.strip()})

    # fixup
    desktops_set.remove('kde')
    desktops_set.add('plasma')

    # extend the existing styles list
    gui_env_ids = set()
    gui_env_list = []
    gui_envs_raw = []
    with open('desktop-style-ids.txt', 'r') as f:
        for line in f.readlines():
            if line.startswith('#'):
                continue
            line = line.strip()
            gui_envs_raw.append(line)
            es_id, es_name = line.split(' ', 1)
            es_id = es_id.strip().lower()
            if not de_id:
                continue
            gui_env_ids.add(es_id)
            gui_env_list.append({'id': es_id, 'name': es_name.strip()})

    for de_id in desktops_set:
        if de_id not in gui_env_ids:
            gui_envs_raw.append(f'{de_id}    {de_id.capitalize()}')

    with open('desktop-style-ids.txt', 'w') as f:
        f.write('# List of recognized GUI environments\n')
        f.write('# ID            Human-readable name\n#\n')
        f.write('\n'.join(sorted(gui_envs_raw)))
        f.write('\n')

    # write C header with all data
    _write_c_array_header(
        'desktop-env-header.tmpl',
        data_header_fname,
        ['name'],
        l10n_hints={
            'DE_DEFS': {'msg': 'Name of the "{}" desktop environment.'},
            'GUI_ENV_STYLE_DEFS': {'msg': 'Name of the "{}" visual environment style.'},
        },
        DE_DEFS=desktops_list,
        GUI_ENV_STYLE_DEFS=gui_env_list,
    )


def main():
    data_dir = os.path.dirname(os.path.abspath(__file__))
    print('Data directory is: {}'.format(data_dir))
    os.chdir(data_dir)

    update_tld_list(IANA_TLD_LIST_URL, 'iana-filtered-tld-list.txt')
    update_spdx_id_list(
        SPDX_REPO_URL,
        '../src/as-spdx-data.h',
        'spdx-free-license-ids.txt',
    )
    update_categories_list(MENU_SPEC_URL, 'xdg-category-names.txt')
    update_platforms_data()
    update_gui_env_ids('../src/as-desktop-env-data.h')

    print('All done.')


if __name__ == '__main__':
    main()
