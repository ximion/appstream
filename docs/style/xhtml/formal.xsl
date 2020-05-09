<?xml version="1.0" encoding="UTF-8"?>
<!--
  Purpose:
     Splitting formal-wise titles into number and title

   Author(s):    Thomas Schraitle <toms@opensuse.org>
   Copyright: 2012, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:exsl="http://exslt.org/common"
  xmlns="http://www.w3.org/1999/xhtml"
  exclude-result-prefixes="exsl">


  <xsl:template match="example">
    <xsl:choose>
      <xsl:when test="glosslist|bibliolist|itemizedlist|orderedlist|
                      segmentedlist|simplelist|variablelist|programlistingco|
                      screenco|screenshot|cmdsynopsis|funcsynopsis|
                      classsynopsis|fieldsynopsis|constructorsynopsis|
                      destructorsynopsis|methodsynopsis|formalpara|para|
                      simpara|address|blockquote|graphicco|mediaobjectco|
                      indexterm|beginpage">
        <div class="complex-example">
          <xsl:apply-imports/>
        </div>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-imports/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <xsl:template
    match="procedure|example|table|figure|glosslist|variablelist|itemizedlist|orderedlist"
    mode="object.label.template">
    <xsl:call-template name="gentext.template">
      <xsl:with-param name="context" select="'styles'"/>
      <xsl:with-param name="name" select="concat( local-name(),'-label')"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="procedure|example|table|figure|glosslist|variablelist|itemizedlist|orderedlist"
    mode="object.title.template">
    <xsl:call-template name="gentext.template">
      <xsl:with-param name="context" select="'styles'"/>
      <xsl:with-param name="name" select="concat( local-name(),'-title')"/>
    </xsl:call-template>
  </xsl:template>


  <xsl:template name="create.formal.title">
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

  <!-- ===================================================== -->
  <xsl:template name="formal.object.heading">
    <xsl:param name="object" select="."/>
    <xsl:param name="title">
      <xsl:call-template name="create.formal.title">
        <xsl:with-param name="node" select="$object"/>
      </xsl:call-template>
    </xsl:param>

    <!-- FIXME: The 'if' here avoids outputting the (ugly/obvious)
         label "Abstract" before abstracts. This is probably the wrong solution.
         It works, but it's ugly. -->
    <xsl:if
      test="not(local-name($object) = 'abstract' or local-name($object) = 'highlights')
            or $object/title or $object/*[contains(local-name(.), 'info')]/title">
      <div class="{concat(local-name(),'-title-wrap')}">
        <h6 class="{concat(local-name(), '-title')}">
          <!-- Do NOT create an id here; parent contains one already -->
          <xsl:copy-of select="$title"/>
          <xsl:call-template name="create.permalink">
            <xsl:with-param name="object" select="$object"/>
          </xsl:call-template>
        </h6>
      </div>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
