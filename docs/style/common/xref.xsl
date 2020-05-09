<?xml version="1.0" encoding="UTF-8"?>
<!--
  Purpose:
    Collect common linking templates, independant of any target format

  Author(s):  Stefan Knorr <sknorr@suse.de>
              Thomas Schraitle <toms@opensuse.org>

  Copyright:  2015 Thomas Schraitle

-->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xlink="http://www.w3.org/1999/xlink"
  xmlns:fo="http://www.w3.org/1999/XSL/Format"
  exclude-result-prefixes="xlink fo">


<xsl:template match="*" mode="intra.title.markup">
  <xsl:param name="linkend"/>
  <xsl:param name="first" select="0"/>
  <xsl:message>WARNING: Element <xsl:value-of select="local-name(.)"/> cannot be used for intra xref linking.
  - affected ID: <xsl:value-of select="(./@id|./@xml:id)[last()]"/>
  - linkend: <xsl:value-of select="$linkend"/></xsl:message>
</xsl:template>


<xsl:template name="generate.intra.separator">
 <xsl:param name="lang"/>
  <xsl:variable name="comma">
   <xsl:call-template name="gentext.template">
    <xsl:with-param name="context" select="'xref'"/>
    <xsl:with-param name="name"  select="'intra-separator'"/>
    <xsl:with-param name="lang" select="$lang"/>
   </xsl:call-template>
  </xsl:variable>

  <xsl:choose>
   <xsl:when test="$comma != ''"><xsl:value-of select="$comma"/></xsl:when>
   <xsl:otherwise><xsl:text>, </xsl:text></xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="create.linkto.other.book">
  <xsl:param name="target"/>
  <!-- It seems we can't get any useful value here anyway, so lets trust the
       caller to get it right... -->
  <xsl:param name="lang" select="'en'"/>
  <!-- Make sure that we transform zh-TW to zh_tw etc. -->
  <xsl:param name="lang-normalized" select="translate($lang, '-ABCDEFGHIJKLMNOPQRSTUVWXYZ', '_abcdefghijklmnopqrstuvwxyz')"/>
  <xsl:variable name="refelem" select="local-name($target)"/>
  <xsl:variable name="target.article" select="$target/ancestor-or-self::article"/>
  <xsl:variable name="target.book" select="$target/ancestor-or-self::book"/>
  <xsl:variable name="text">
    <xsl:apply-templates select="$target" mode="intra.title.markup">
      <xsl:with-param name="linkend" select="@linkend"/>
      <xsl:with-param name="lang" select="$lang-normalized"/>
    </xsl:apply-templates>
  </xsl:variable>
  <xsl:copy-of select="$text"/>
</xsl:template>

 <xsl:template match="section" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>
    <xsl:variable name="level" select="count(ancestor-or-self::section)"/>

   <!--<xsl:message>######## section <xsl:value-of select="$linkend"/></xsl:message>-->
   <xsl:apply-templates select="parent::*" mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
   </xsl:apply-templates>
   <xsl:call-template name="generate.intra.separator">
     <xsl:with-param name="lang" select="$lang"/>
   </xsl:call-template>
   <xsl:call-template name="substitute-markup">
      <xsl:with-param name="template">
        <xsl:call-template name="gentext.template">
          <xsl:with-param name="context" select="'xref'"/>
          <xsl:with-param name="name"  select="concat('intra-', local-name())"/>
          <xsl:with-param name="lang" select="$lang"/>
        </xsl:call-template>
      </xsl:with-param>
   </xsl:call-template>
 </xsl:template>

  <xsl:template match="sect1" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>
    <xsl:apply-templates select="parent::*" mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
    </xsl:apply-templates>
    <xsl:call-template name="generate.intra.separator">
     <xsl:with-param name="lang" select="$lang"/>
    </xsl:call-template>
    <xsl:call-template name="substitute-markup">
      <xsl:with-param name="template">
        <xsl:call-template name="gentext.template">
          <xsl:with-param name="context" select="'xref'"/>
          <xsl:with-param name="name"  select="concat('intra-', local-name())"/>
          <xsl:with-param name="lang" select="$lang"/>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>


  <xsl:template match="sect2|sect3|sect4|sect5" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>
    <xsl:apply-templates
      select="ancestor::appendix|ancestor::article|
      ancestor::chapter|ancestor::glossary|ancestor::preface"
      mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
    </xsl:apply-templates>
    <xsl:call-template name="generate.intra.separator">
     <xsl:with-param name="lang" select="$lang"/>
    </xsl:call-template>
    <xsl:call-template name="substitute-markup">
      <xsl:with-param name="template">
        <xsl:call-template name="gentext.template">
          <xsl:with-param name="context" select="'xref'"/>
          <xsl:with-param name="name"  select="concat('intra-', local-name())"/>
          <xsl:with-param name="lang" select="$lang"/>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>


  <xsl:template match="appendix|chapter" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>
    <!-- We don't want parts -->
    <xsl:apply-templates select="ancestor::book" mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
    </xsl:apply-templates>
    <xsl:call-template name="generate.intra.separator">
     <xsl:with-param name="lang" select="$lang"/>
    </xsl:call-template>
    <xsl:call-template name="substitute-markup">
      <xsl:with-param name="template">
        <xsl:call-template name="gentext.template">
          <xsl:with-param name="context" select="'xref'"/>
          <xsl:with-param name="name"  select="concat('intra-', local-name())"/>
          <xsl:with-param name="lang" select="$lang"/>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>


  <xsl:template match="preface" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>
    <xsl:apply-templates select="parent::*" mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
    </xsl:apply-templates>

    <xsl:call-template name="generate.intra.separator">
     <xsl:with-param name="lang" select="$lang"/>
    </xsl:call-template>
    <xsl:call-template name="substitute-markup">
      <xsl:with-param name="template">
        <xsl:call-template name="gentext.template">
          <xsl:with-param name="context" select="'xref'"/>
          <xsl:with-param name="name"  select="concat('intra-', local-name())"/>
          <xsl:with-param name="lang" select="$lang"/>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>


  <xsl:template match="part" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>
    <!-- We don't want parts, so skip them -->
    <xsl:apply-templates select="parent::*" mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
    </xsl:apply-templates>
  </xsl:template>


  <xsl:template match="article|book" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>
    <xsl:call-template name="substitute-markup">
      <xsl:with-param name="template">
        <xsl:call-template name="gentext.template">
          <xsl:with-param name="context" select="'xref'"/>
          <xsl:with-param name="name"  select="concat('intra-', local-name())"/>
          <xsl:with-param name="lang" select="$lang"/>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>


  <xsl:template
   match="variablelist|orderedlist|itemizedlist|procedure|table|figure|equation|
          caution|warning|important|note|tip"
   mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>

    <xsl:apply-templates select="parent::*" mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
    </xsl:apply-templates>
    <xsl:choose>
      <xsl:when test="title">
        <xsl:call-template name="generate.intra.separator">
         <xsl:with-param name="lang" select="$lang"/>
        </xsl:call-template>
        <xsl:apply-templates select="." mode="title.markup">
          <xsl:with-param name="lang" select="$lang"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message>WARNING: Element <xsl:value-of select="local-name(.)"/> without title used for intra xref linking.</xsl:message>
        <xsl:message>- affected ID: <xsl:value-of select="(./@id|./@xml:id)[last()]"/></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="step" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>

    <xsl:apply-templates select="." mode="xref-to"/>
  </xsl:template>

  <xsl:template match="varlistentry" mode="intra.title.markup">
    <xsl:param name="linkend"/>
    <xsl:param name="first" select="0"/>
    <xsl:param name="lang" select="'en'"/>

    <xsl:apply-templates select="ancestor::appendix|ancestor::article|
      ancestor::chapter|ancestor::glossary|ancestor::preface"
      mode="intra.title.markup">
      <xsl:with-param name="lang" select="$lang"/>
    </xsl:apply-templates>
    <xsl:value-of select="concat(' ', term[1])"/>
  </xsl:template>

</xsl:stylesheet>
