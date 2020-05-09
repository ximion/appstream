<?xml version="1.0" encoding="UTF-8"?>
<!-- 
  Purpose:
     Create permalink for divisions and formal object
     
  Parameters:
     * object (default ".")
       object node
     
     * generate.permalinks
       Flag which enables or disables the complete permalink generation
     
  Output:
     Creates an <a> tag with a href attribute, pointing to the
     respective ID.

   Author(s):    Thomas Schraitle <toms@opensuse.org>
   Copyright: 2012, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/1999/xhtml">

  <xsl:template name="create.permalink">
    <xsl:param name="object" select="."/>
    <xsl:variable name="target">
      <xsl:call-template name="href.target">
        <xsl:with-param name="object" select="$object"/>
      </xsl:call-template>
    </xsl:variable>

    <xsl:if test="$generate.permalinks != 0">
      <a title="Permalink" class="permalink">
        <xsl:attribute name="href">
          <xsl:choose>
            <xsl:when test="$target != ''">
              <xsl:value-of select="$target"/>
            </xsl:when>
            <xsl:otherwise>#</xsl:otherwise>
          </xsl:choose>
        </xsl:attribute>
        <xsl:text>#</xsl:text>
      </a>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
