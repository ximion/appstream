<?xml version="1.0" encoding="UTF-8"?>
<!--
  Purpose:
     Splitting section-wise titles into number and title

   Author(s):    Thomas Schraitle <toms@opensuse.org>
   Copyright: 2012, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:exsl="http://exslt.org/common"
  xmlns="http://www.w3.org/1999/xhtml"
  xmlns:dm="urn:x-suse:ns:docmanager"
  exclude-result-prefixes="exsl">

  <xsl:template name="create.header.title">
    <xsl:param name="node" select="."/>
    <xsl:param name="level" select="0"/>
    <xsl:param name="legal" select="0"/>
    <xsl:variable name="label">
      <xsl:apply-templates select="$node" mode="label.markup">
        <xsl:with-param name="allow-anchors" select="1"/>
      </xsl:apply-templates>
    </xsl:variable>
    <!-- NOTE: The gentext context is NOT considered -->
    <xsl:if test="$legal = 0">
      <span class="number">
        <xsl:copy-of select="$label"/>
        <xsl:text> </xsl:text>
      </span>
    </xsl:if>
    <span class="name">
      <xsl:apply-templates select="$node" mode="title.markup">
        <xsl:with-param name="allow-anchors" select="1"/>
      </xsl:apply-templates>
    </span>
    <xsl:text> </xsl:text>
  </xsl:template>


<xsl:template match="sect1[@role='legal']|section[@role='legal']|
                     part[@role='legal']|chapter[@role='legal']|
                     appendix[@role='legal']">
  <xsl:choose>
    <xsl:when test="ancestor::*[@role='legal'] or $is.chunk = 1">
      <xsl:apply-imports/>
    </xsl:when>
    <xsl:otherwise>
      <div class="legal-section">
        <xsl:choose>
          <xsl:when test="local-name(.) = 'chapter' or
                          local-name(.) = 'appendix'">
            <xsl:call-template name="chapter-preface-appendix"/>
            <!-- To avoid importing a template that doesn't set ID's the right
                 way. -->
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-imports/>
          </xsl:otherwise>
        </xsl:choose>
      </div>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template match="bridgehead">
  <xsl:param name="allow-anchors" select="1"/>
  <xsl:variable name="container">
    <xsl:if test="not(@renderas)">
      <xsl:value-of select="(ancestor::appendix|ancestor::article|ancestor::bibliography|
                             ancestor::chapter|ancestor::glossary|ancestor::glossdiv|
                             ancestor::index|ancestor::partintro|ancestor::preface|
                             ancestor::refsect1|ancestor::refsect2|ancestor::refsect3|
                             ancestor::sect1|ancestor::sect2|ancestor::sect3|ancestor::sect4|
                             ancestor::sect5|ancestor::section|ancestor::setindex|
                             ancestor::simplesect)[last()]"/>
    </xsl:if>
  </xsl:variable>

  <xsl:variable name="clevel">
    <xsl:if test="$container != ''">
      <xsl:choose>
        <xsl:when test="local-name($container) = 'appendix' or
                      local-name($container) = 'chapter' or
                      local-name($container) = 'article' or
                      local-name($container) = 'bibliography' or
                      local-name($container) = 'glossary' or
                      local-name($container) = 'index' or
                      local-name($container) = 'partintro' or
                      local-name($container) = 'preface' or
                      local-name($container) = 'setindex'">1</xsl:when>
        <xsl:when test="local-name($container) = 'glossdiv'">
          <xsl:value-of select="count(ancestor::glossdiv)+1"/>
        </xsl:when>
        <xsl:when test="local-name($container) = 'sect1' or
                      local-name($container) = 'sect2' or
                      local-name($container) = 'sect3' or
                      local-name($container) = 'sect4' or
                      local-name($container) = 'sect5' or
                      local-name($container) = 'refsect1' or
                      local-name($container) = 'refsect2' or
                      local-name($container) = 'refsect3' or
                      local-name($container) = 'section' or
                      local-name($container) = 'simplesect'">
          <xsl:variable name="slevel">
            <xsl:call-template name="section.level">
              <xsl:with-param name="node" select="$container"/>
            </xsl:call-template>
          </xsl:variable>
          <xsl:value-of select="$slevel + 1"/>
        </xsl:when>
        <xsl:otherwise>1</xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:variable>
  <xsl:variable name="level-final">
    <xsl:choose>
      <xsl:when test="@renderas = 'sect1'">1</xsl:when>
      <xsl:when test="@renderas = 'sect2'">2</xsl:when>
      <xsl:when test="@renderas = 'sect3'">3</xsl:when>
      <xsl:when test="@renderas = 'sect4'">4</xsl:when>
      <xsl:when test="@renderas">5</xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$clevel"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:variable name="hlevel">
    <xsl:choose>
      <xsl:when test="$level-final &gt;= 5">6</xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$level-final + 1"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:variable name="legal">
    <xsl:choose>
      <xsl:when test="ancestor::*[@role='legal']">1</xsl:when>
      <xsl:otherwise>0</xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:variable name="class">title<xsl:if test="$legal = 1"> legal</xsl:if></xsl:variable>

  <div>
    <xsl:attribute name="class">sect<xsl:value-of select="$level-final"/> bridgehead</xsl:attribute>
    <xsl:element name="h{$hlevel}" namespace="http://www.w3.org/1999/xhtml">
      <xsl:attribute name="class"><xsl:value-of select="$class"/></xsl:attribute>
      <xsl:if test="$allow-anchors != 0">
        <xsl:call-template name="id.attribute">
          <xsl:with-param name="node" select="."/>
          <xsl:with-param name="force" select="1"/>
        </xsl:call-template>
      </xsl:if>

      <span class="name">
        <xsl:apply-templates/>
      </span>

      <xsl:call-template name="create.permalink">
         <xsl:with-param name="object" select="."/>
      </xsl:call-template>
      <xsl:call-template name="editlink"/>
    </xsl:element>
  </div>
</xsl:template>


<xsl:template name="section.heading">
  <xsl:param name="section" select="."/>
  <xsl:param name="level" select="1"/>
  <xsl:param name="allow-anchors" select="1"/>
  <xsl:param name="title"/>
  <xsl:variable name="legal">
    <xsl:choose>
      <xsl:when test="$section/ancestor::*[@role='legal']">1</xsl:when>
      <xsl:otherwise>0</xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:variable name="class">title<xsl:if test="$legal = 1"> legal</xsl:if></xsl:variable>

  <xsl:variable name="id">
    <xsl:choose>
      <!-- Make sure the subtitle doesn't get the same id as the title -->
      <xsl:when test="self::subtitle">
        <xsl:call-template name="object.id">
          <xsl:with-param name="object" select="."/>
        </xsl:call-template>
      </xsl:when>
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

  <!-- HTML H level is one higher than section level -->
  <xsl:variable name="hlevel">
    <xsl:choose>
      <!-- highest valid HTML H level is H6; so anything nested deeper
           than 5 levels down just becomes H6 -->
      <xsl:when test="$level &gt;= 5">6</xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$level + 1"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:element name="h{$hlevel}" namespace="http://www.w3.org/1999/xhtml">
    <xsl:attribute name="class"><xsl:value-of select="$class"/></xsl:attribute>

    <xsl:if test="$allow-anchors != 0">
      <xsl:call-template name="id.attribute">
        <xsl:with-param name="node" select="$section"/>
        <xsl:with-param name="force" select="1"/>
      </xsl:call-template>
    </xsl:if>
    <xsl:call-template name="create.header.title">
      <xsl:with-param name="node" select=".."/>
      <xsl:with-param name="level" select="$level"/>
      <xsl:with-param name="legal" select="$legal"/>
    </xsl:call-template>
    <xsl:call-template name="create.permalink">
       <xsl:with-param name="object" select="$section"/>
    </xsl:call-template>
    <xsl:call-template name="editlink"/>
    <xsl:call-template name="create.header.line">
       <xsl:with-param name="object" select="$section"/>
    </xsl:call-template>
  </xsl:element>
  <xsl:call-template name="debug.filename-id"/>
</xsl:template>

<!-- Fix up the output of section elements to look like the output of sectX
elements, to fix their display. -->
<xsl:template match="section" mode="common.html.attributes">
  <xsl:variable name="section" select="."/>

  <xsl:variable name="renderas">
    <xsl:choose>
      <xsl:when test="$section/@renderas = 'sect1'">1</xsl:when>
      <xsl:when test="$section/@renderas = 'sect2'">2</xsl:when>
      <xsl:when test="$section/@renderas = 'sect3'">3</xsl:when>
      <xsl:when test="$section/@renderas = 'sect4'">4</xsl:when>
      <xsl:when test="$section/@renderas = 'sect5'">5</xsl:when>
      <xsl:otherwise><xsl:value-of select="''"/></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="level">
    <xsl:choose>
      <xsl:when test="$renderas != ''">
        <xsl:value-of select="$renderas"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="section.level">
          <xsl:with-param name="node" select="$section"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="class" select="concat('sect',$level)"/>
  <xsl:variable name="inherit" select="0"/>
  <xsl:call-template name="generate.html.lang"/>
  <xsl:call-template name="dir">
    <xsl:with-param name="inherit" select="$inherit"/>
  </xsl:call-template>
  <xsl:apply-templates select="." mode="class.attribute">
    <xsl:with-param name="class" select="$class"/>
  </xsl:apply-templates>
</xsl:template>

<!-- Hook for additional customizations -->
<xsl:template name="create.header.line">
  <xsl:param name="object" select="."/>
</xsl:template>

<xsl:template name="debug.filename-id">
  <xsl:param name="node" select="."/>
  <xsl:variable name="xmlbase"
    select="ancestor-or-self::*[@xml:base][1]/@xml:base"/>

  <xsl:if test="$draft.mode = 'yes' and $xmlbase != ''">
    <div class="doc-status">
      <ul>
        <li>
          <span class="ds-label">File Name: </span>
          <xsl:value-of select="$xmlbase"/>
        </li>
        <li>
          <span class="ds-label">ID: </span>
          <xsl:choose>
            <xsl:when test="$node/parent::*[(@id|@xml:id)[1]] != ''">
              <xsl:call-template name="object.id">
                <xsl:with-param name="object" select="$node/parent::*"/>
              </xsl:call-template>
            </xsl:when>
            <xsl:otherwise><span class="ds-message">no ID found</span></xsl:otherwise>
          </xsl:choose>
        </li>
      </ul>
    </div>
  </xsl:if>
</xsl:template>

<xsl:template name="editlink">
  <xsl:variable name="editurl" select="ancestor-or-self::*/*[concat(local-name(.), 'info')]/dm:docmanager/dm:editurl[1]"/>
  <xsl:variable name="xmlbase" select="ancestor-or-self::*[@xml:base][1]/@xml:base"/>
  <xsl:if test="($draft.mode = 'yes' or $show.edit.link = 1) and $editurl != '' and $xmlbase != ''">
    <a class="report-bug" target="_blank" href="{$editurl}{$xmlbase}"
      title="Edit the source file for this section">Edit source</a>
  </xsl:if>
</xsl:template>

</xsl:stylesheet>
