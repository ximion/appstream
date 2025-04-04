#!/usr/bin/python3
# pylint: disable=invalid-name,missing-docstring
#
# Copyright (C) 2023 Chun-wei Fan <fanc999@yahoo.com.tw>
#
# SPDX-License-Identifier: LGPL-2.1+

# Script to generate .def file from object files/static library

import argparse
import os
import re
import subprocess
import sys

from io import StringIO


def run_dumpbin(objs):
    dumpbin_results = []
    for o in objs:
        command = ['dumpbin', '/symbols', o]
        p = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=None,
            stdin=subprocess.PIPE,
            universal_newlines=True,
        )
        stdout, blah = p.communicate()
        if p.returncode != 0:
            sys.exit(p.returncode)
        dumpbin_results.append(stdout)

    return dumpbin_results

'''
Notice output from dumpbin /symbols is like the following:

Microsoft (R) COFF/PE Dumper Version 14.29.30152.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file src\libappstream-5-core.a

File Type: LIBRARY

COFF SYMBOL TABLE
000 00000000 SECT1  notype       Static       | .text
    Section length    0, #relocs    0, #linenums    0, checksum        0
002 00000000 SECT2  notype       Static       | .data
    Section length    0, #relocs    0, #linenums    0, checksum        0
004 00000000 SECT3  notype       Static       | .bss
    Section length   18, #relocs    0, #linenums    0, checksum        0
006 00000000 SECT4  notype       Static       | .text
    Section length   36, #relocs    6, #linenums    0, checksum 11750A74, selection    1 (pick no duplicates)
008 00000000 SECT4  notype ()    External     | as_video_get_type
009 00000000 SECT32 notype       Static       | .xdata
...

So, we are only more interested in items like:

008 00000000 SECT4  notype ()    External     | as_video_get_type

and we need to note that these can also involve symbols that we use from
other libraries, so we throw these out when we go through the output
'''

def extract_symbols(namespace, tokens):
    if tokens[4] == '()':
        check_idx = 5
        target_idx = 7
    else:
        check_idx = 4
        target_idx = 6
    
    if tokens[check_idx] == 'External':
        target_plat = None
        if 'Platform' in os.environ:
            target_plat = os.environ['Platform']
        use_sym_prefix = False
        if target_plat is not None and target_plat == 'x86':
            use_sym_prefix = True
        if use_sym_prefix:
            if re.match(r'^' + '_' + namespace + '_', tokens[target_idx]) is not None:
                return tokens[target_idx][1:]
        else:
            if re.match(r'^' + namespace + '_', tokens[target_idx]) is not None:
                return tokens[target_idx]

def process_dumpbin_outputs(namespace, dumpbin_results):
    extracted_symbols = []
    for res in dumpbin_results:
        dumpbin_lines = StringIO(res).readlines()
        for l in dumpbin_lines:
            tokens = l.split()
            if (len(tokens) == 7 or len(tokens) == 8) and tokens[3] == 'notype':
                result = extract_symbols(namespace, tokens)
                if result is not None:
                    extracted_symbols.append(result)
    extracted_symbols.sort()
    return extracted_symbols


def output_def_file(def_file, symbols):
    with open(def_file, "w") as out:
        out.write("EXPORTS\n")
        for s in symbols:
            out.write("%s\n" % s)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate .def from object files or static libs.')
    parser.add_argument('-n', '--namespace', dest='namespace',
                        help='namespace of symbols', required=True)
    parser.add_argument('--def', dest='def_file', required=True,
                        help='output def file')
    parser.add_argument('objects', nargs='+',
                        help='objects files/static libraries to process')

    args = parser.parse_args()
    namespace = args.namespace
    def_file = args.def_file
    objs = args.objects
    dumpbin_results = run_dumpbin(objs)
    output_def_file(def_file, process_dumpbin_outputs(namespace, dumpbin_results))
