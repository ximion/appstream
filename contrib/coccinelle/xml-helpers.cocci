/* SPDX-License-Identifier: LGPL-2.1-or-later */

/**
 * xmlGetProp -> as_xml_get_prop_value
 */
@@
expression p, q, r;
typedef xmlChar;
typedef gchar;
position pos : script:python() {
        not (pos[0].file.endswith("src/as-xml.c"))
};
@@
- r@pos = xmlGetProp (p, (xmlChar*) q);
+ r = as_xml_get_prop_value (p, q);
@@
expression p, q, r;
position pos : script:python() {
        not (pos[0].file.endswith("src/as-xml.c"))
};
@@
- r@pos = (gchar*) xmlGetProp (p, (xmlChar*) q);
+ r = as_xml_get_prop_value (p, q);

/**
 * xmlNewChild -> as_xml_add_node
 */
@@
expression p, q, r;
position pos : script:python() {
        not (pos[0].file.endswith("src/as-xml.c"))
};
@@
- r@pos = xmlNewChild (p, NULL, (xmlChar*) q, NULL);
+ r = as_xml_add_node (p, q);
@@
expression p, q, r;
position pos : script:python() {
        not (pos[0].file.endswith("src/as-xml.c"))
};
@@
- r@pos = xmlNewChild (p, NULL, (xmlChar*) q, (xmlChar*) "");
+ r = as_xml_add_node (p, q);

/**
 * xmlNewTextChild -> as_xml_add_text_node
 */
@@
expression r, root, name, value;
position pos : script:python() {
        not (pos[0].file.endswith("src/as-xml.c"))
};
@@
- r@pos = xmlNewTextChild (root, NULL, (xmlChar*) name, (xmlChar*) value);
+ r = as_xml_add_text_node (root, name, value);

/**
 * xmlNewProp -> as_xml_add_text_prop
 */
@@
expression node, name, value;
position pos : script:python() {
        not (pos[0].file.endswith("src/as-xml.c"))
};
@@
- xmlNewProp@pos (node, (xmlChar*) name, (xmlChar*) value);
+ as_xml_add_text_prop (node, name, value);
