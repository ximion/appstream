<?xml version="1.0" encoding="UTF-8"?>
<!--
   Purpose:
     Transform DocBook document into single XHTML file

   Parameters:
     Too many to list here, see:
     http://docbook.sourceforge.net/release/xsl/current/doc/html/index.html

   Input:
     DocBook 4/5 document

   Output:
     Single XHTML file

   See Also:
     * http://doccookbook.sf.net/html/en/dbc.common.dbcustomize.html
     * http://sagehill.net/docbookxsl/CustomMethods.html#WriteCustomization

   Authors:    Thomas Schraitle <toms@opensuse.org>,
               Stefan Knorr <sknorr@suse.de>
   Copyright:  2012-2018 Thomas Schraitle, Stefan Knorr

-->

<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:exsl="http://exslt.org/common"
    xmlns:date="http://exslt.org/dates-and-times"
    xmlns="http://www.w3.org/1999/xhtml"
    exclude-result-prefixes="exsl date">

  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>

  <xsl:include href="../VERSION.xsl"/>

  <xsl:include href="../common/dates-revisions.xsl"/>
  <xsl:include href="../common/labels.xsl"/>
  <xsl:include href="../common/titles.xsl"/>
  <xsl:include href="../common/navigation.xsl"/>
  <xsl:include href="../common/string-replace.xsl"/>
  <xsl:include href="../common/xref.xsl"/>

  <xsl:include href="param.xsl"/>
  <xsl:include href="create-permalink.xsl"/>

  <xsl:include href="autotoc.xsl"/>
  <xsl:include href="autobubbletoc.xsl"/>
  <xsl:include href="lists.xsl"/>
  <xsl:include href="verbatim.xsl"/>
  <xsl:include href="component.xsl"/>
  <xsl:include href="glossary.xsl"/>
  <xsl:include href="formal.xsl"/>
  <xsl:include href="sections.xsl"/>
  <xsl:include href="division.xsl"/>
  <xsl:include href="inline.xsl"/>
  <xsl:include href="xref.xsl"/>
  <xsl:include href="html.xsl"/>
  <xsl:include href="admon.xsl"/>
  <xsl:include href="block.xsl"/>
  <xsl:include href="qandaset.xsl"/>
  <xsl:include href="titlepage.xsl"/>
  <xsl:include href="titlepage.templates.xsl"/>

<!-- Actual templates start here -->

  <xsl:template name="clearme">
    <xsl:param name="wrapper">div</xsl:param>
    <xsl:element name="{$wrapper}" namespace="http://www.w3.org/1999/xhtml">
      <xsl:attribute name="class">clearme</xsl:attribute>
    </xsl:element>
  </xsl:template>

 <!-- Adapt head.contents to...
 + generate more useful page titles ("Chapter x. Chapter Name" -> "Chapter Name | Book Name")
 + remove the inline styles for draft mode, so they can be substituted by styles
   in the real stylesheet
 -->
<xsl:template name="head.content">
  <xsl:param name="node" select="."/>
  <xsl:param name="product-name">
    <xsl:call-template name="product.name"/>
  </xsl:param>
  <xsl:param name="product-number">
    <xsl:call-template name="product.number"/>
  </xsl:param>
  <xsl:param name="product">
    <xsl:call-template name="version.info"/>
  </xsl:param>
  <xsl:param name="structure.title.candidate">
    <xsl:choose>
      <xsl:when test="self::book or self::article or self::set">
        <xsl:apply-templates select="title | *[contains(local-name(), 'info')]/title[last()]" mode="title.markup.textonly"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="((ancestor::book | ancestor::article)[last()]/title |
                                      (ancestor::book | ancestor::article)[last()]/*[contains(local-name(), 'info')]/title)[last()]"
           mode="title.markup.textonly"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:param>
  <xsl:param name="structure.title">
    <xsl:choose>
      <xsl:when test="$structure.title.candidate != ''">
        <xsl:value-of select="$structure.title.candidate"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:choose>
          <xsl:when test="self::book or self::article or self::set">
            <xsl:call-template name="gentext">
              <xsl:with-param name="key" select="local-name(.)"/>
            </xsl:call-template>
          </xsl:when>
          <xsl:when test="ancestor::article">
            <xsl:call-template name="gentext">
              <xsl:with-param name="key" select="'article'"/>
            </xsl:call-template>
          </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="gentext">
              <xsl:with-param name="key" select="'book'"/>
            </xsl:call-template>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:param>
  <xsl:param name="substructure.title.short">
    <xsl:if test="not(self::book or self::article or self::set)">
      <xsl:choose>
        <xsl:when test="*[contains(local-name(), 'info')]/title | title | refmeta/refentrytitle">
          <xsl:apply-templates select="(*[contains(local-name(), 'info')]/title | title | refmeta/refentrytitle)[last()]" mode="title.markup.textonly"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="gentext">
            <xsl:with-param name="key" select="local-name(.)"/>
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:param>
  <xsl:param name="substructure.title.long">
    <xsl:if test="not(self::book or self::article or self::set)">
       <xsl:apply-templates select="." mode="object.title.markup"/>
    </xsl:if>
  </xsl:param>

  <xsl:param name="title">
    <xsl:if test="$substructure.title.short != ''">
      <xsl:value-of select="concat($substructure.title.short, $head.content.title.separator)"/>
    </xsl:if>

    <xsl:value-of select="$structure.title"/>

    <xsl:if test="$product != ''">
      <xsl:value-of select="concat($head.content.title.separator, $product)"/>
    </xsl:if>
  </xsl:param>

  <xsl:variable name="meta-og.description">
    <xsl:variable name="info"
      select=" (articleinfo|bookinfo|prefaceinfo|chapterinfo|appendixinfo
               |sectioninfo|sect1info|sect2info|sect3info|sect4info|sect5info
               |referenceinfo
               |refentryinfo
               |partinfo
               |info
               |docinfo)[1]"/>
    <xsl:variable name="first-para" select="(descendant::para|descendant::simpara)[1]"/>
    <xsl:choose>
      <xsl:when test="$info and $info/abstract">
        <xsl:for-each select="$info/abstract[1]/*">
          <xsl:value-of select="normalize-space(.)"/>
          <xsl:if test="position() &lt; last()">
            <xsl:text> </xsl:text>
          </xsl:if>
        </xsl:for-each>
      </xsl:when>
      <xsl:otherwise>
        <!-- Except for the lack of markup here, this code is very similar to that in autotoc.xsl. Unify later if possible. -->
        <xsl:variable name="teaser">
          <xsl:apply-templates/>
        </xsl:variable>
        <xsl:variable name="teaser-safe">
          <xsl:call-template name="string-replace">
            <xsl:with-param name="input" select="$teaser"/>
            <xsl:with-param name="search-string" select="'&quot;'"/>
            <!-- The xslns-build script we use to transform the stylesheets to
            their namespaced version is unsafe for the string &amp;quot; as the
            value here, so we just replace double quotes with a space and hope
            for the best. -->
            <xsl:with-param name="replace-string" select="' '"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:choose>
          <xsl:when test="string-length(normalize-space($teaser-safe)) &gt; $teaser.length">
            <xsl:value-of select="substring(normalize-space($teaser-safe),1,$teaser.length)"/>
            <xsl:value-of select="'…'"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$teaser-safe"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <title><xsl:value-of select="$title"/></title>

  <meta name="viewport"
    content="width=device-width, initial-scale=1.0, user-scalable=yes"/>

  <xsl:if test="$html.base != ''">
    <base href="{$html.base}"/>
  </xsl:if>

  <!-- Insert links to CSS files or insert literal style elements -->
  <xsl:call-template name="generate.css"/>

  <xsl:if test="$html.stylesheet != ''">
    <xsl:call-template name="output.html.stylesheets">
      <xsl:with-param name="stylesheets" select="normalize-space($html.stylesheet)"/>
    </xsl:call-template>
  </xsl:if>

  <xsl:if test="$html.script != ''">
    <xsl:call-template name="output.html.scripts">
      <xsl:with-param name="scripts" select="normalize-space($html.script)"/>
    </xsl:call-template>
  </xsl:if>

  <xsl:if test="$link.mailto.url != ''">
    <link rev="made" href="{$link.mailto.url}"/>
  </xsl:if>

  <xsl:call-template name="meta-generator"/>

  <xsl:if test="$product-name != ''">
    <meta name="product-name" content="{$product-name}"/>
  </xsl:if>
  <xsl:if test="$product-number != ''">
    <meta name="product-number" content="{$product-number}"/>
  </xsl:if>

  <meta name="book-title" content="{$structure.title}"/>
  <xsl:if test="$substructure.title.long != ''">
    <meta name="chapter-title" content="{$substructure.title.long}"/>
  </xsl:if>

  <meta name="description" content="{$meta-og.description}"/>

  <xsl:apply-templates select="." mode="head.keywords.content"/>

  <xsl:if test="$canonical-url-base != ''">
    <xsl:variable name="ischunk">
      <xsl:call-template name="chunk"/>
    </xsl:variable>
    <xsl:variable name="filename">
      <xsl:choose>
        <xsl:when test="$ischunk = 1">
          <xsl:apply-templates mode="chunk-filename" select="."/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat($root.filename,$html.ext)"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="canonical.url">
      <xsl:value-of select="concat($canonical-url-base,'/',$filename)"/>
    </xsl:variable>
    <xsl:variable name="og.title">
      <!-- localize punctuation -->
      <xsl:if test="$product != ''">
        <xsl:value-of select="$product"/>
        <xsl:text>: </xsl:text>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="$substructure.title.long != ''">
          <xsl:value-of select="$substructure.title.long"/>
          <xsl:text> (</xsl:text>
          <xsl:value-of select="$structure.title"/>
          <xsl:text>)</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$structure.title"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="og.image">
      <xsl:choose>
        <!-- Ignoring stuff like inlinemediaobjects here, because those are
        likely very small anyway. Let's hope SVGs work too.-->
        <xsl:when
          test="(descendant::figure/descendant::imagedata/@fileref
                |descendant::informalfigure/descendant::imagedata/@fileref)[1]">
          <xsl:value-of
            select="concat($canonical-url-base, '/', $img.src.path,
                    (descendant::figure/descendant::imagedata/@fileref
                    |descendant::informalfigure/descendant::imagedata/@fileref)[1])"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat($canonical-url-base, '/', $daps.header.logo)"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <link rel="canonical" href="{$canonical.url}"/>
    <xsl:text>&#10;</xsl:text>
    <meta property="og:title" content="{$og.title}"/>
    <xsl:text>&#10;</xsl:text>
    <meta property="og:image" content="{$og.image}"/>
    <xsl:text>&#10;</xsl:text>
    <meta property="og:description" content="{$meta-og.description}"/>
    <xsl:text>&#10;</xsl:text>
    <meta property="og:url" content="{$canonical.url}"/>
  </xsl:if>

</xsl:template>

  <xsl:template name="meta-generator">
    <xsl:element name="meta">
      <xsl:attribute name="name">generator</xsl:attribute>
      <xsl:attribute name="content">
        <xsl:value-of select="concat($converter.name, ' ', $converter.version)"/>
      </xsl:attribute>
    </xsl:element>
  </xsl:template>

  <xsl:template match="refentry" mode="titleabbrev.markup">
    <xsl:value-of select="refmeta/refentrytitle[text()]"/>
  </xsl:template>

  <xsl:template match="appendix|article|book|bibliography|chapter|section|part|preface|glossary|sect1|set|refentry"
                mode="breadcrumbs">
    <xsl:param name="class">crumb</xsl:param>
    <xsl:param name="context">header</xsl:param>

    <xsl:variable name="title.candidate">
      <xsl:apply-templates select="." mode="titleabbrev.markup"/>
    </xsl:variable>
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="$title.candidate != ''">
          <xsl:value-of select="$title.candidate"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="gentext">
            <xsl:with-param name="key" select="local-name(.)"/>
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:element name="a" namespace="http://www.w3.org/1999/xhtml">
      <xsl:call-template name="generate.class.attribute">
        <xsl:with-param name="class" select="$class"/>
      </xsl:call-template>
      <xsl:attribute name="href">
        <xsl:call-template name="href.target">
          <xsl:with-param name="object" select="."/>
          <xsl:with-param name="context" select="."/>
        </xsl:call-template>
      </xsl:attribute>
      <xsl:attribute name="accesskey">
          <xsl:text>c</xsl:text>
      </xsl:attribute>
      <span class="single-contents-icon"> </span>
      <xsl:if test="$context = 'fixed-header'">
        <xsl:call-template name="gentext">
          <xsl:with-param name="key">showcontentsoverview</xsl:with-param>
        </xsl:call-template>
        <xsl:call-template name="gentext">
          <xsl:with-param name="key">admonseparator</xsl:with-param>
        </xsl:call-template>
      </xsl:if>
      <xsl:value-of select="string($title)"/>
    </xsl:element>
  </xsl:template>

  <xsl:template name="breadcrumbs.navigation">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="context">header</xsl:param>
    <xsl:param name="debug"/>

    <xsl:if test="$generate.breadcrumbs != 0">
      <div class="crumbs inactive">
        <xsl:if test="$context = 'header'">
          <xsl:call-template name="generate.breadcrumbs.back"/>
        </xsl:if>
        <xsl:apply-templates select="." mode="breadcrumbs">
          <xsl:with-param name="class">single-crumb</xsl:with-param>
          <xsl:with-param name="context" select="$context"/>
        </xsl:apply-templates>
        <xsl:if test="$context = 'header'">
          <div class="bubble-corner active-contents"> </div>
        </xsl:if>
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template name="generate.breadcrumbs.back">
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="$overview-page-title != 0">
          <xsl:value-of select="$overview-page-title"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$overview-page"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:if test="$overview-page != ''">
      <a class="overview-link" href="{$overview-page}" title="{$title}">
        <span class="overview-icon"><xsl:value-of select="$title"/></span>
      </a>
      <span><xsl:copy-of select="$daps.breadcrumbs.sep"/></span>
    </xsl:if>
  </xsl:template>

  <!-- ===================================================== -->
  <xsl:template name="pickers">
    <xsl:if test="$generate.pickers != 0">
      <div id="_pickers">
        <div id="_language-picker" class="inactive">
          <a id="_language-picker-button" href="#">
            <span class="picker">
              <xsl:call-template name="gentext">
                <xsl:with-param name="key">LocalisedLanguageName</xsl:with-param>
              </xsl:call-template>
            </span>
          </a>
          <div class="bubble-corner active-contents"> </div>
          <div class="bubble active-contents">
            <h6>
              <xsl:call-template name="gentext">
                <xsl:with-param name="key">selectlanguage</xsl:with-param>
              </xsl:call-template>
            </h6>
            <a class="selected" href="#">
              <xsl:call-template name="gentext">
                <xsl:with-param name="key">LocalisedLanguageName</xsl:with-param>
              </xsl:call-template>
            </a>
          </div>
        </div>
        <div id="_format-picker" class="inactive">
          <a id="_format-picker-button" href="#">
            <span class="picker">Web Page</span>
          </a>
          <div class="bubble-corner active-contents"> </div>
          <div class="bubble active-contents">
            <h6>
              <xsl:call-template name="gentext">
                <xsl:with-param name="key">selectformat</xsl:with-param>
              </xsl:call-template>
            </h6>
            <xsl:call-template name="picker.selection"/>
            <a href="#">
              <xsl:call-template name="gentext">
                <xsl:with-param name="key">formatpdf</xsl:with-param>
              </xsl:call-template>
            </a>
            <a href="#">
              <xsl:call-template name="gentext">
                <xsl:with-param name="key">formatepub</xsl:with-param>
              </xsl:call-template>
            </a>
          </div>
        </div>
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template name="picker.selection">
    <a href="#">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key">formathtml</xsl:with-param>
      </xsl:call-template>
    </a>
    <a class="selected" href="#">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key">formatsinglehtml</xsl:with-param>
      </xsl:call-template>
    </a>
  </xsl:template>

  <xsl:template name="create.header.logo">
    <xsl:if test="$generate.logo != 0">
      <div id="_logo">
        <xsl:choose>
          <xsl:when test="$homepage != ''">
            <a href="{$homepage}">
              <img src="{$daps.header.logo}" alt="{$daps.header.logo.alt}"/>
              <span><xsl:value-of select="string($daps.header.logo.text)"/></span>
            </a>
          </xsl:when>
          <xsl:otherwise>
            <img src="{$daps.header.logo}" alt="{$daps.header.logo.alt}"/>
            <span><xsl:value-of select="string($daps.header.logo.text)"/></span>
          </xsl:otherwise>
        </xsl:choose>
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template name="create.header.buttons">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>

    <div class="buttons">
      <a class="top-button button" href="#">
        <xsl:call-template name="gentext">
          <xsl:with-param name="key">totopofpage</xsl:with-param>
        </xsl:call-template>
      </a>
      <xsl:call-template name="create.header.buttons.nav">
        <xsl:with-param name="prev" select="$prev"/>
        <xsl:with-param name="next" select="$next"/>
      </xsl:call-template>
      <xsl:call-template name="clearme"/>
    </div>
  </xsl:template>

  <xsl:template name="create.header.buttons.nav">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <!-- This is a stub, intentionally.
         The version in chunk-common does something, though. -->
  </xsl:template>

  <xsl:template name="fixed-header-wrap">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="nav.context"/>

    <div id="_fixed-header-wrap" class="inactive">
      <div id="_fixed-header">
        <xsl:call-template name="breadcrumbs.navigation">
          <xsl:with-param name="prev" select="$prev"/>
          <xsl:with-param name="next" select="$next"/>
          <xsl:with-param name="context">fixed-header</xsl:with-param>
        </xsl:call-template>
        <xsl:call-template name="create.header.buttons">
          <xsl:with-param name="prev" select="$prev"/>
          <xsl:with-param name="next" select="$next"/>
        </xsl:call-template>
        <xsl:call-template name="clearme"/>
      </div>
      <xsl:if test="$generate.bubbletoc != 0">
        <div class="active-contents bubble">
          <div class="bubble-container">
            <div id="_bubble-toc">
              <xsl:call-template name="bubble-toc"/>
            </div>
            <xsl:call-template name="clearme"/>
          </div>
        </div>
      </xsl:if>
    </div>
  </xsl:template>

  <xsl:template name="share.and.print">
    <xsl:param name="prev" select="/foo"/>
    <xsl:param name="next" select="/foo"/>
    <xsl:param name="nav.context"/>
  </xsl:template>

  <xsl:template name="outerelement.class.attribute">
    <!-- To accommodate for ActiveDoc's needs, add this to both body and
         #_content.-->
    <xsl:param name="node" select="'body'"/>

    <xsl:attribute name="class">
      <xsl:if test="($draft.mode = 'yes' or
                    ($draft.mode = 'maybe' and
                    ancestor-or-self::*[@status][1]/@status = 'draft'))
                    and $draft.watermark.image != ''"
        >draft </xsl:if><xsl:if test="$node = 'body'"><xsl:if test="$is.chunk = 0"
        >single </xsl:if>offline js-off</xsl:if></xsl:attribute>
  </xsl:template>

  <xsl:template name="bypass">
    <!-- Bypass blocks help disabled, e.g. blind users navigate more quickly.
    Hard-to-parse W3C spec: https://www.w3.org/TR/WCAG20/#navigation-mechanisms-skip -->
    <xsl:param name="format" select="'single'"/>
    <xsl:if test="not($optimize.plain.text = 1)">
      <div class="bypass-block">
        <a href="#_content">
          <xsl:call-template name="gentext.template">
            <xsl:with-param name="context" select="'bypass-block'"/>
            <xsl:with-param name="name" select="'bypass-to-content'"/>
          </xsl:call-template>
        </a>
        <xsl:if test="$format = 'chunk'">
          <!-- Going to #_bottom-navigation is an admittedly quirky choice but
          the other two places in which we have this kind of page nav
          (regular header and fixed header) do not assign an ID to it. -->
          <a href="#_bottom-navigation">
            <xsl:call-template name="gentext.template">
              <xsl:with-param name="context" select="'bypass-block'"/>
              <xsl:with-param name="name" select="'bypass-to-nav'"/>
            </xsl:call-template>
          </a>
        </xsl:if>
      </div>
    </xsl:if>
  </xsl:template>


<xsl:template match="*" mode="process.root">
  <xsl:param name="prev"/>
  <xsl:param name="next"/>
  <xsl:param name="nav.context"/>
  <xsl:param name="content">
    <xsl:apply-imports/>
  </xsl:param>
  <xsl:variable name="doc" select="self::*"/>
  <xsl:variable name="lang">
    <xsl:apply-templates select="(ancestor-or-self::*/@lang)[last()]" mode="html.lang.attribute"/>
  </xsl:variable>
  <xsl:call-template name="user.preroot"/>
  <xsl:call-template name="root.messages"/>

  <html lang="{$lang}" xml:lang="{$lang}">
    <xsl:call-template name="root.attributes"/>
    <head>
      <xsl:call-template name="system.head.content">
        <xsl:with-param name="node" select="$doc"/>
      </xsl:call-template>
      <xsl:call-template name="head.content">
        <xsl:with-param name="node" select="$doc"/>
      </xsl:call-template>
      <xsl:call-template name="user.head.content">
        <xsl:with-param name="node" select="$doc"/>
      </xsl:call-template>
    </head>
    <body>
      <xsl:call-template name="body.attributes"/>
      <xsl:call-template name="outerelement.class.attribute"/>
      <xsl:call-template name="bypass"/>
      <div id="_outer-wrap">
        <div id="_white-bg">
          <div id="_header">
            <xsl:call-template name="create.header.logo"/>
            <xsl:call-template name="pickers"/>
            <xsl:call-template name="breadcrumbs.navigation">
              <xsl:with-param name="prev" select="$prev"/>
              <xsl:with-param name="next" select="$next"/>
            </xsl:call-template>
            <xsl:call-template name="clearme"/>
          </div>
        </div>

        <xsl:if test="$generate.fixed.header != 0">
          <xsl:call-template name="fixed-header-wrap">
            <xsl:with-param name="next" select="$next"/>
            <xsl:with-param name="prev" select="$prev"/>
          </xsl:call-template>
        </xsl:if>

        <xsl:call-template name="user.header.content"/>
        <div id="_toc-bubble-wrap"></div>
        <div id="_content">
          <xsl:call-template name="outerelement.class.attribute">
            <xsl:with-param name="node" select="'id-content'"/>
          </xsl:call-template>
          <div class="documentation">

          <xsl:apply-templates select="."/>

          </div>
          <div class="page-bottom">
            <xsl:call-template name="share.and.print">
              <xsl:with-param name="prev" select="$prev"/>
              <xsl:with-param name="next" select="$next"/>
              <xsl:with-param name="nav.context" select="$nav.context"/>
            </xsl:call-template>
          </div>
        </div>
      </div>
    </body>
  </html>
</xsl:template>

  <xsl:template name="user.head.content">
    <xsl:param name="node" select="."/>

    <xsl:text>&#10;</xsl:text>

    <xsl:if test="$daps.header.js.custom != ''">
      <xsl:call-template name="make.script.link">
        <xsl:with-param name="script.filename" select="$daps.header.js.custom"/>
      </xsl:call-template>
    </xsl:if>
    <xsl:if test="$enable.source.highlighting = 1">
      <xsl:call-template name="make.script.link">
        <xsl:with-param name="script.filename" select="$daps.header.js.highlight"/>
      </xsl:call-template>
      <script>
<xsl:text disable-output-escaping="yes">
<![CDATA[
document.addEventListener('DOMContentLoaded', (event) => {
  document.querySelectorAll('.verbatim-wrap.highlight').forEach((block) => {
    hljs.highlightBlock(block);
  });
});
hljs.configure({
  useBR: false
});
]]>
</xsl:text>
      </script>
    </xsl:if>
  </xsl:template>

  <xsl:template name="make.multi.script.link">
    <xsl:param name="input" select="''"/>
    <xsl:variable name="input-sanitized" select="concat(normalize-space($input),' ')"/>
    <xsl:if test="string-length($input) &gt; 1">
      <xsl:variable name="this" select="substring-before($input-sanitized,' ')"/>
      <xsl:variable name="next" select="substring-after($input-sanitized,' ')"/>
      <xsl:call-template name="make.script.link">
        <xsl:with-param name="script.filename" select="$this"/>
      </xsl:call-template>
      <xsl:call-template name="make.multi.script.link">
        <xsl:with-param name="input" select="$next"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>


  <xsl:template name="user.footer.content">
    <div id="_footer">
      <p>©
        <xsl:if test="function-available('date:year')">
          <xsl:value-of select="date:year()"/>
          <xsl:text> </xsl:text>
        </xsl:if>
        Freedesktop.org, the AppStream Project</p>
    </div>
  </xsl:template>

</xsl:stylesheet>
