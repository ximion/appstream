<?xml version="1.0" encoding="UTF-8"?>
<!-- 
  Purpose:
     Add permalink to titlepage template

  Output:
     Creates <h1> headline.

   Author(s):   Stefan Knorr <sknorr@suse.de>
   Copyright:   2012, Stefan Knorr

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/1999/xhtml">


<xsl:template match="book/title|article/title|set/title" mode="titlepage.mode">
  <xsl:variable name="id">
    <xsl:choose>
      <!-- if title is in an *info wrapper, get the grandparent -->
      <xsl:when test="contains(local-name(..), 'info')">
        <xsl:call-template name="object.id">
          <xsl:with-param name="object" select="../.."/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="object.id">
          <xsl:with-param name="object" select=".."/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <h1>
    <xsl:apply-templates select="." mode="common.html.attributes"/>
    <xsl:choose>
      <xsl:when test="$generate.id.attributes = 0">
        <a id="{$id}"/>
      </xsl:when>
      <xsl:otherwise>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:choose>
      <xsl:when test="$show.revisionflag != 0 and @revisionflag">
    <span class="{@revisionflag}">
      <xsl:apply-templates mode="titlepage.mode"/>
    </span>
      </xsl:when>
      <xsl:otherwise>
    <xsl:apply-templates mode="titlepage.mode"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> </xsl:text>
    <xsl:call-template name="create.permalink">
      <xsl:with-param name="object" select=".."/>
    </xsl:call-template>
    <xsl:call-template name="editlink"/>
  </h1>
</xsl:template>

</xsl:stylesheet>
