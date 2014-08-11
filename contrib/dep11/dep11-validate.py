#!/usr/bin/python3
#
# Copyright (C) 2014 Matthias Klumpp <mak@debian.org>
#
# Licensed under the GNU General Public License Version 3
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
import yaml
from optparse import OptionParser
from voluptuous import Schema, Required, All, Any, Length, Range, Match, Url

schema_header = Schema({
    Required('File'): All(str, 'DEP-11', msg="Must be \"DEP-11\""),
    Required('Origin'): All(str, Length(min=1)),
    Required('Version'): All(str, Match(r'(\d+\.?)+$'), msg="Must be a valid version number"),
})

schema_provides_dbus = Schema({
    Required('type'): All(str, Length(min=1)),
    Required('service'): All(str, Length(min=1)),
})

schema_provides = Schema({
    Any('mimetypes',
        'binaries',
        'libraries',
        'python3',
        'python2',
        'firmware'): All(list, [str], Length(min=1)),
    'dbus': All(list, Length(min=1), [schema_provides_dbus]),
})

schema_keywords = Schema({
    Required('C'): All(list, [str], Length(min=1), msg="Must have an unlocalized 'C' key"),
    dict: All(list, [str], Length(min=1)),
}, extra = True)

schema_translated = Schema({
    Required('C'): All(str, Length(min=1), msg="Must have an unlocalized 'C' key"),
    dict: All(str, Length(min=1)),
}, extra = True)

schema_image = Schema({
    Required('width'): All(int, Range(min=10)),
    Required('height'): All(int, Range(min=10)),
    Required('url'): All(str, Url()),
})

schema_screenshots = Schema({
    Required('default', default=False): All(bool),
    Required('image-source'): All(dict, Length(min=1), schema_image),
    'image-thumbnail': All(dict, Length(min=1), schema_image),
    'caption': All(dict, Length(min=1), schema_translated),
})

schema_icon = Schema({
    Required(Any('stock',
                 'cached'), msg="A 'stock' or 'cached' icon must at least be provided."): All(str, Length(min=1)),
    'cached': All(str, Match(r'.*[.].*$'), msg='Icon entry is missing filename or extension'),
    'remote': All(str, Url()),
})

schema_url = Schema({
    Any('homepage',
        'bugtracker',
        'faq',
        'help',
        'donation'): All(str, Url()),
})

schema_component = Schema({
    Required('Type'): All(str, Any('generic', 'desktop-app', 'web-app', 'addon', 'codec', 'inputmethod')),
    Required('ID'): All(str, Length(min=1)),
    Required('Name'): All(dict, Length(min=1), schema_translated),
    Required('Packages'): All(list, Length(min=1)),
    'Summary': All(dict, Length(min=1), schema_translated),
    'Description': All(dict, Length(min=1), schema_translated),
    'Categories': All(list, [str], Length(min=1)),
    'Url': All(dict, Length(min=1), schema_url),
    'Icon': All(dict, Length(min=1), schema_icon),
    'Keywords': All(dict, Length(min=1), schema_keywords),
    'Provides': All(dict, Length(min=1), schema_provides),
    'ProjectGroup': All(str, Length(min=1)),
    'DeveloperName': All(dict, Length(min=1), schema_translated),
    'Screenshots': All(list, Length(min=1), [schema_screenshots]),
    'Extends': All(str, Length(min=1)),
})

class DEP11Validator:
    issue_list = list()

    def __init__(self):
        pass

    def add_issue(self, msg):
        self.issue_list.append(msg)

    def _test_locale_cruft(self, doc, key):
        ldict = doc.get(key, None)
        if not ldict:
            return
        for lang in ldict.keys():
            if lang == 'x-test':
                self.add_issue("[%s][%s]: %s" % (doc['ID'], key, "Found cruft locale: x-test"))
            if lang.endswith('.UTF-8'):
                self.add_issue("[%s][%s]: %s" % (doc['ID'], key, "AppStream locale names should not specify encoding (ends with .UTF-8)"))

    def validate(self, fname):
        ret = True
        try:
            docs = yaml.load_all(open(fname, 'r'))
            header = next(docs)
        except Exception as e:
            self.add_issue("Could not parse file: %s" % (str(e)))
            return False

        try:
            schema_header(header)
        except Exception as e:
            self.add_issue("Invalid DEP-11 header: %s" % (str(e)))
            ret = False

        for doc in docs:
            if not doc:
                self.add_issue("FATAL: Empty document found.")
                ret = False
                continue
            if not doc.get('ID', None):
                self.add_issue("FATAL: Component without ID found.")
                ret = False
                continue
            try:
                schema_component(doc)
            except Exception as e:
                self.add_issue("[%s]: %s" % (doc['ID'], str(e)))
                ret = False

            self._test_locale_cruft(doc, 'Name')
            self._test_locale_cruft(doc, 'Summary')
            self._test_locale_cruft(doc, 'Description')
            self._test_locale_cruft(doc, 'DeveloperName')
            # TODO: test screenshot caption

        return ret

    def print_issues(self):
        for issue in self.issue_list:
            print(issue)

    def clear_issues():
        self.issue_list = list()

def main():
    parser = OptionParser()
    parser.add_option("--no-color",
                  action="store_true", dest="no_color", default=False,
                  help="don'r print colored output")

    (options, args) = parser.parse_args()

    if len(args) < 1:
        print("You need to specify a file to validate!")
        sys.exit(4)
    fname = args[0]

    validator = DEP11Validator()
    ret = validator.validate(fname)
    validator.print_issues()
    if ret:
        msg = "Validation successful."
    else:
        msg = "Validation failed!"
    if options.no_color:
        print(msg)
    elif ret:
        print('\033[92m' + msg + '\033[0m')
    else:
        print('\033[91m' + msg + '\033[0m')

    if not ret:
        sys.exit(1)

if __name__ == "__main__":
    main()
