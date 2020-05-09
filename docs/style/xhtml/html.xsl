<?xml version="1.0"?>
<!-- 
  Purpose:
     Override original id.attribute template, so we can have a special parameter
     to force an ID.
     
   See Also:
     * http://docbook.sourceforge.net/release/xsl/current/doc/html/index.html

   Author(s): Stefan Knorr <sknorr@suse.de>
   Copyright: 2012, Stefan Knorr

-->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/1999/xhtml">


    <xsl:template name="id.attribute">
        <xsl:param name="node" select="."/>
        <xsl:param name="conditional" select="1"/>
        <xsl:param name="force" select="0"/>
        <xsl:choose>
            <xsl:when test="$generate.id.attributes = 0 and $force != 1">
                <!-- No id attributes when this param is zero -->
            </xsl:when>
            <xsl:when test="($force = 1 or $conditional = 0 or $node/@id or $node/@xml:id)
                and local-name($node) != 'figure'">
                <xsl:attribute name="id">
                    <xsl:call-template name="object.id">
                        <xsl:with-param name="object" select="$node"/>
                    </xsl:call-template>
                </xsl:attribute>
            </xsl:when>
        </xsl:choose>
    </xsl:template>


    <!--  Adapted to support hidden value of OS  - currently this value
          is only used in the SUSE Manager documentation. -->
    <xsl:template name="common.html.attributes">
        <xsl:param name="inherit" select="0"/>
        <xsl:param name="class">
            <xsl:value-of select="local-name(.)"/>
            <xsl:text> </xsl:text>
            <xsl:if test="($draft.mode = 'yes' or $draft.mode = 'maybe')
                           and normalize-space(@os) = 'hidden'">
                <xsl:value-of select="@os"/>
            </xsl:if>
        </xsl:param>

        <xsl:apply-templates select="." mode="common.html.attributes">
            <xsl:with-param name="class" select="$class"/>
            <xsl:with-param name="inherit" select="$inherit"/>
        </xsl:apply-templates>
    </xsl:template>

</xsl:stylesheet>
