<?xml version="1.0" encoding="UTF-8"?>
<!--
  Purpose:
     Splitting part-wise titles into number and title

   Author(s): Thomas Schraitle <toms@opensuse.org>
   Copyright: 2012, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/1999/xhtml">


  <xsl:template match="part" mode="object.label.template">
    <xsl:call-template name="gentext.template">
      <xsl:with-param name="context" select="'xref-number'"/>
      <xsl:with-param name="name" select="local-name()"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="create.division.title">
    <xsl:param name="node" select="."/>
    <xsl:variable name="label.template">
      <xsl:apply-templates select="$node" mode="object.label.template"/>
    </xsl:variable>

    <xsl:if test="$label.template != ''">
      <span class="number">
        <xsl:call-template name="substitute-markup">
          <xsl:with-param name="template" select="$label.template"/>
        </xsl:call-template>
        <xsl:text> </xsl:text>
      </span>
    </xsl:if>
    <span class="name">
      <xsl:apply-templates select="$node" mode="title.markup">
        <xsl:with-param name="allow-anchors" select="1"/>
      </xsl:apply-templates>
      <xsl:text> </xsl:text>
    </span>
  </xsl:template>

  <xsl:template name="division.title">
    <xsl:param name="node" select="."/>
    <h1 class="title">
      <!-- No calling of id.attribute template as ID is definied in 
        parent <div>
      -->
      <xsl:call-template name="anchor">
        <xsl:with-param name="node" select="$node"/>
        <xsl:with-param name="conditional" select="0"/>
      </xsl:call-template>
      <xsl:call-template name="create.division.title">
        <xsl:with-param name="node" select="$node"/>
      </xsl:call-template>
      <xsl:call-template name="create.permalink">
        <xsl:with-param name="object" select="$node"/>
      </xsl:call-template>
      <xsl:call-template name="editlink"/>
    </h1>
  </xsl:template>

  <xsl:template match="book">
    <xsl:call-template name="id.warning"/>

    <div>
      <xsl:apply-templates select="." mode="common.html.attributes"/>
      <xsl:call-template name="id.attribute">
        <xsl:with-param name="conditional" select="0"/>
      </xsl:call-template>

      <xsl:call-template name="book.titlepage"/>

      <xsl:apply-templates select="dedication" mode="dedication"/>
      <xsl:apply-templates select="acknowledgements" mode="acknowledgements"/>

      <xsl:variable name="toc.params">
        <xsl:call-template name="find.path.params">
          <xsl:with-param name="table" select="normalize-space($generate.toc)"/>
        </xsl:call-template>
      </xsl:variable>

      <xsl:call-template name="make.lots">
        <xsl:with-param name="toc.params" select="$toc.params"/>
        <xsl:with-param name="toc">
          <xsl:call-template name="division.toc">
            <xsl:with-param name="toc.title.p" select="contains($toc.params, 'title')"/>
          </xsl:call-template>
        </xsl:with-param>
      </xsl:call-template>

      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/legalnotice"/>
      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/legalnotice"/>

      <xsl:apply-templates/>
    </div>
  </xsl:template>

</xsl:stylesheet>
