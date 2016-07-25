#!/usr/bin/env python3
#
# Copyright (C) 2015 Matthias Klumpp <mak@debian.org>
# Licensed under LGPL-2.0+

#
# Script to download updated static data from the web and process it for AppStream to use.
#

import requests

def get_tld_list(fname, url):
    print("Getting TLD list from IANA...")

    data_result = ["# IANA TLDs we allow for AppStream IDs",
                   "# derived from the full list at {}".format(url)]

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

    with open(fname, 'w') as f:
        f.write("\n".join(data_result))
        f.write('\n')


def main():
    get_tld_list("iana-filtered-tld-list.txt", "https://data.iana.org/TLD/tlds-alpha-by-domain.txt")

    print("All done.")

if __name__ == '__main__':
    main()
