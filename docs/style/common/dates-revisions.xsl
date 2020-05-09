<?xml version="1.0" encoding="UTF-8"?>
<!--
  Purpose:
    Return ready-made markup for the date and revision field on the imprint
    page in both FO and HTML.

  Author(s):    Stefan Knorr <sknorr@suse.de>,
                Thomas Schraitle <toms@opensuse.org>

  Copyright:    2013, Stefan Knorr, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:date="http://exslt.org/dates-and-times"
  exclude-result-prefixes="date">


  <xsl:template name="date.and.revision.inner">
    <xsl:variable name="date">
      <xsl:apply-templates select="(bookinfo/date | info/date |
        articleinfo/date | ancestor::book/bookinfo/date |
        ancestor::set/setinfo/date | ancestor::book/info/date |
        ancestor::set/info/date)[1]"/>
    </xsl:variable>

    <xsl:variable name="version"
      select="(bookinfo/releaseinfo | articleinfo/releaseinfo |
               info/releaseinfo | ancestor::book/bookinfo/releaseinfo |
               ancestor::set/setinfo/releaseinfo |
               ancestor::book/info/releaseinfo |
               ancestor::set/info/releaseinfo)[1]"/>

    <!-- Make SVN revision numbers look a little less cryptic. Git does
         (by design) not have a feature that randomly changes your
         commits to update a revision number within a file. This
         template will thus only ever be of use for SVN-hosted files. -->
    <xsl:variable name="revision-candidate"
      select="substring-before(($version)[1],' $')"/>
    <xsl:variable name="revision">
      <xsl:choose>
        <xsl:when test="starts-with($revision-candidate, '$Rev: ')">
          <xsl:value-of select="substring-after($revision-candidate,
                                                '$Rev: ')"/>
        </xsl:when>
        <xsl:when test="starts-with($revision-candidate, '$Revision: ')">
          <xsl:value-of select="substring-after($revision-candidate,
                                                '$Revision: ')"/>
        </xsl:when>
        <xsl:when test="starts-with($revision-candidate, '$LastChangedRevision: ')">
          <xsl:value-of select="substring-after($revision-candidate,
                                                '$LastChangedRevision: ')"/>
        </xsl:when>
        <xsl:otherwise/>
      </xsl:choose>
    </xsl:variable>

    <xsl:call-template name="imprint.label">
      <xsl:with-param name="label" select="'PubDate'"/>
    </xsl:call-template>
    <xsl:choose>
      <xsl:when test="$date != ''">
        <xsl:value-of select="$date"/>
      </xsl:when>
      <xsl:when test="function-available('date:date-time') or
                      function-available('date:dateTime')">
        <xsl:variable name="format">
          <xsl:call-template name="gentext.template">
            <xsl:with-param name="context" select="'datetime'"/>
            <xsl:with-param name="name" select="'format'"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:variable name="date-exslt">
          <xsl:choose>
            <xsl:when test="function-available('date:date-time')">
              <xsl:value-of select="date:date-time()"/>
            </xsl:when>
            <xsl:when test="function-available('date:dateTime')">
              <!-- Xalan quirk -->
              <xsl:value-of select="date:dateTime()"/>
            </xsl:when>
          </xsl:choose>
        </xsl:variable>

        <xsl:call-template name="datetime.format">
          <xsl:with-param name="date" select="$date-exslt"/>
          <xsl:with-param name="format" select="$format"/>
          <xsl:with-param name="padding" select="1"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>??<xsl:message>Need EXSLT date support.</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="$version != ''">
      <!-- Misappropriated but hopefully still correct everywhere. -->
      <xsl:call-template name="gentext.template">
        <xsl:with-param name="context" select="'iso690'"/>
        <xsl:with-param name="name" select="'spec.pubinfo.sep'"/>
      </xsl:call-template>
    </xsl:if>
    <xsl:if test="$version != ''">
      <xsl:choose>
        <xsl:when test="$revision != ''">
          <xsl:call-template name="imprint.label">
            <xsl:with-param name="label" select="'Revision'"/>
          </xsl:call-template>
          <xsl:value-of select="$revision"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="imprint.label">
            <xsl:with-param name="label" select="'version'"/>
          </xsl:call-template>
          <xsl:value-of select="$version"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>


</xsl:stylesheet>
