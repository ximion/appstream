<?xml version="1.0" encoding="UTF-8"?>
<!--
  Purpose:
    Returns the number only of an element, if it has one. Normally, this
    is done by the original stylesheets, however, some has to be
    explicitly added.

  Author(s):    Thomas Schraitle <toms@opensuse.org>

  Copyright:    2013, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:exsl="http://exslt.org/common"
  exclude-result-prefixes="exsl">

  <xsl:template match="refsect1/title|refnamediv" mode="label.markup"/>

</xsl:stylesheet>
