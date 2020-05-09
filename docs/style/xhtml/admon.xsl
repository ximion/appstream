<?xml version="1.0" encoding="UTF-8"?>
<!-- 
  Purpose:
     Changing structure of graphical admonitions

   Author(s):    Thomas Schraitle <toms@opensuse.org>
   Copyright: 2012, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:exsl="http://exslt.org/common"
  xmlns="http://www.w3.org/1999/xhtml"
  exclude-result-prefixes="exsl">
  
  <xsl:template name="graphical.admonition">
    <xsl:param name="admonition" select="."/>
    <xsl:variable name="admon.type">
      <xsl:choose>
        <xsl:when test="local-name(.)='note'">Note</xsl:when>
        <xsl:when test="local-name(.)='warning'">Warning</xsl:when>
        <xsl:when test="local-name(.)='caution'">Caution</xsl:when>
        <xsl:when test="local-name(.)='tip'">Tip</xsl:when>
        <xsl:when test="local-name(.)='important'">Important</xsl:when>
        <xsl:otherwise>Note</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="alt">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key" select="$admon.type"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="class">
      <xsl:text>admonition </xsl:text>
      <xsl:value-of select="local-name(.)"/>
      <xsl:text> </xsl:text>
      <xsl:choose>
        <xsl:when test="@role='compact'">compact</xsl:when>
        <xsl:otherwise>normal</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    
    <div>
      <xsl:call-template name="id.attribute">
        <xsl:with-param name="node" select="$admonition"/>
        <xsl:with-param name="force" select="1"/>
      </xsl:call-template>
      <xsl:call-template name="generate.class.attribute">
        <xsl:with-param name="class" select="$class"/>
      </xsl:call-template>
      <img class="symbol" alt="{$alt}" title="{$admon.type}">
        <xsl:attribute name="src">
          <xsl:call-template name="admon.graphic"/>
        </xsl:attribute>
      </img>
      <!-- compact admons can have no title unless explicitly set, otherwise,
      we always generate at least the text Note/Tip/... -->
      <xsl:if test="((title or info/title) or ($admon.textlabel != 0 and not(@role='compact')))">
        <h6>
          <xsl:apply-templates select="." mode="object.title.markup"/>
        </h6>
      </xsl:if>
      <xsl:apply-templates/>
    </div>
  </xsl:template>
  
  <xsl:template name="admon.graphic">
  <xsl:param name="node" select="."/>
  <xsl:value-of select="concat($admon.graphics.path, $admon.graphics.prefix)"/>
  <xsl:choose>
    <xsl:when test="local-name($node)='note'">note</xsl:when>
    <xsl:when test="local-name($node)='warning'">warning</xsl:when>
    <xsl:when test="local-name($node)='caution'">caution</xsl:when>
    <xsl:when test="local-name($node)='tip'">tip</xsl:when>
    <xsl:when test="local-name($node)='important'">important</xsl:when>
    <xsl:otherwise>note</xsl:otherwise>
  </xsl:choose>
  <xsl:value-of select="$admon.graphics.extension"/>
</xsl:template>

</xsl:stylesheet>
