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
import xml.etree.ElementTree as ET
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
        'firmware',
        'modaliases',
        'fonts'): All(list, [str], Length(min=1)),
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
    Required('source-image'): All(dict, Length(min=1), schema_image),
    'thumbnails': All(list, Length(min=1), [schema_image]),
    'caption': All(dict, Length(min=1), schema_translated),
})

schema_icon = Schema({
    'stock': All(str, Length(min=1)),
    'cached': All(str, Match(r'.*[.].*$'), msg='Icon entry is missing filename or extension'),
    'local': All(str, Match(r'^[\'"]?(?:/[^/]+)*[\'"]?$'), msg='Icon entry should be an absolute path'),
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
    Required('Type'): All(str, Any('generic', 'desktop-app', 'web-app', 'addon', 'codec', 'inputmethod', 'font')),
    Required('ID'): All(str, Length(min=1)),
    Required('Name'): All(dict, Length(min=1), schema_translated),
    Required('Packages'): All(list, [str], Length(min=1)),
    'Summary': All(dict, {str: str}, Length(min=1), schema_translated),
    'Description': All(dict, {str: str}, Length(min=1), schema_translated),
    'Categories': All(list, [str], Length(min=1)),
    'CompulsoryForDesktops': All(list, [str], Length(min=1)),
    'Url': All(dict, Length(min=1), schema_url),
    'Icon': All(dict, Length(min=1), schema_icon),
    'Keywords': All(dict, Length(min=1), schema_keywords),
    'Provides': All(dict, Length(min=1), schema_provides),
    'ProjectGroup': All(str, Length(min=1)),
    'ProjectLicense': All(str, Length(min=1)),
    'DeveloperName': All(dict, Length(min=1), schema_translated),
    'Screenshots': All(list, Length(min=1), [schema_screenshots]),
    'Extends': All(list, [str], Length(min=1)),
})

class DEP11Validator:
    issue_list = list()

    def __init__(self):
        pass

    def add_issue(self, msg):
        self.issue_list.append(msg)

    def _test_localized_dict(self, doc, ldict, id_string):
        ret = True
        for lang, value in ldict.items():
            if lang == 'x-test':
                self.add_issue("[%s][%s]: %s" % (doc['ID'], id_string, "Found cruft locale: x-test"))
            if lang == 'xx':
                self.add_issue("[%s][%s]: %s" % (doc['ID'], id_string, "Found cruft locale: xx"))
            if lang.endswith('.UTF-8'):
                self.add_issue("[%s][%s]: %s" % (doc['ID'], id_string, "AppStream locale names should not specify encoding (ends with .UTF-8)"))
            if (value.startswith("\"")) or (value.startswith("\'")):
                self.add_issue("[%s][%s]: %s" % (doc['ID'], id_string, "String is quoted: '%s' @ %s" % (value, lang)))
            if " " in lang:
                self.add_issue("[%s][%s]: %s" % (doc['ID'], id_string, "Locale name contains space: '%s'" % (lang)))
                # this - as opposed to the other issues - is an error
                ret = False
        return ret

    def _test_localized(self, doc, key):
        ldict = doc.get(key, None)
        if not ldict:
            return True

        return self._test_localized_dict(doc, ldict, key)

    def _test_custom_objects(self, lines):
        ret = True
        for i in range(0, len(lines)):
            if "!!python/" in lines[i]:
                self.add_issue("Python object encoded in line %i." % (i))
                ret = False
        return ret

    def _validate_description_tag(self, docid, child, allowed_tags):
        ret = True
        if not child.tag in allowed_tags:
            self.add_issue("[%s]: %s" % (docid, "Invalid description markup found: '%s' @ data['Description']" % (child.tag)))
            ret = False
        if child.attrib.get('{http://www.w3.org/XML/1998/namespace}lang'):
            self.add_issue("[%s]: Invalid, localized tag in long description: '%s' => %s @ data['Description']" % (docid, child.tag, child.text))
            ret = False
        elif len(child.attrib) > 0:
            self.add_issue("[%s]: Markup tag has attributes: '%s' => %s @ data['Description']" % (docid, child.tag, child.attrib))
            ret = False
        return ret

    def _validate_description(self, docid, desc):
        ret = True
        ET.register_namespace("xml", "http://www.w3.org/XML/1998/namespace")
        try:
            root = ET.fromstring("<root>%s</root>" % (desc))
        except Exception as e:
            self.add_issue("[%s]: %s" % (docid, "Broken description markup found: %s @ data['Description']" % (str(e))))
            return False
        for child in root:
            if not self._validate_description_tag(docid, child, ['p', 'ul', 'ol']):
                ret = False
            if (child.tag == 'ul') or (child.tag == 'ol'):
                for child2 in child:
                    if not self._validate_description_tag(docid, child2, ['li']):
                        ret = False
        return ret

    def validate(self, fname):
        ret = True
        ids_found = dict()

        f = open(fname, 'r')
        lines = f.readlines()
        f.seek(0)

        # see if there are any Python-specific objects encoded
        ret = self._test_custom_objects(lines)

        try:
            docs = yaml.load_all(f)
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
            docid = doc.get('ID')
            if not doc:
                self.add_issue("FATAL: Empty document found.")
                ret = False
                continue
            if not docid:
                self.add_issue("FATAL: Component without ID found.")
                ret = False
                continue
            if ids_found.get(docid):
                self.add_issue("FATAL: Found two components with the same ID: %s." % (docid))
                ret = False
                continue
            else:
                ids_found[docid] = True
            try:
                schema_component(doc)
            except Exception as e:
                self.add_issue("[%s]: %s" % (docid, str(e)))
                ret = False
                continue

            # more tests for the icon key
            icon = doc.get('Icon')
            if (doc['Type'] == "desktop-app") or (doc['Type'] == "web-app"):
                if not doc.get('Icon'):
                    self.add_issue("[%s]: %s" % (docid, "Components containing an application must have an 'Icon' key."))
                    ret = False
            if icon:
                if (not icon.get('stock')) and (not icon.get('cached')) and (not icon.get('local')):
                    self.add_issue("[%s]: %s" % (docid, "A 'stock', 'cached' or 'local' icon must at least be provided. @ data['Icon']"))
                    ret = False

            if not self._test_localized(doc, 'Name'):
                ret = False
            if not self._test_localized(doc, 'Summary'):
                ret = False
            if not self._test_localized(doc, 'Description'):
                ret = False
            if not self._test_localized(doc, 'DeveloperName'):
                ret = False

            for shot in doc.get('Screenshots', list()):
                caption = shot.get('caption')
                if caption:
                    if not self._test_localized_dict(doc, caption, "Screenshots.caption"):
                        ret = False

            desc = doc.get('Description', dict())
            for d in desc.values():
                if not self._validate_description(docid, d):
                    ret = False

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
