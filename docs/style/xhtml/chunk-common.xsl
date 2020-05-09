<?xml version="1.0" encoding="UTF-8"?>
<!--
   Purpose:
     Create structure of chunked contents

   See Also:
     * http://doccookbook.sf.net/html/en/dbc.common.dbcustomize.html
     * http://sagehill.net/docbookxsl/CustomMethods.html#WriteCustomization

   Author:    Thomas Schraitle <toms@opensuse.org>,
              Stefan Knorr <sknorr@suse.de>
   Copyright: 2012, 2013, Thomas Schraitle, Stefan Knorr

-->

<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:exsl="http://exslt.org/common"
    xmlns="http://www.w3.org/1999/xhtml"
    xmlns:t="http://nwalsh.com/docbook/xsl/template/1.0"
    xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0"
    exclude-result-prefixes="exsl l t">

  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk-common.xsl"/>

  <!-- ===================================================== -->
  <xsl:template
    match="appendix|article|book|bibliography|chapter|part|preface|glossary|sect1|set|refentry|index"
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
      <xsl:choose>
        <xsl:when test="$class='book-link'">
          <xsl:attribute name="href">
            <xsl:value-of select="concat($root.filename, $html.ext)"/>
          </xsl:attribute>
          <xsl:attribute name="title">
            <xsl:value-of select="string($title)"/>
          </xsl:attribute>
          <span class="book-icon">
            <xsl:choose>
              <xsl:when test="$overview-page = ''">
                <xsl:attribute name="class">book-icon</xsl:attribute>
              </xsl:when>
              <xsl:otherwise>
                <xsl:attribute name="class">book-icon lower-level</xsl:attribute>
              </xsl:otherwise>
            </xsl:choose>
            <xsl:value-of select="string($title)"/>
          </span>
        </xsl:when>
        <xsl:otherwise>
          <xsl:attribute name="href">
            <xsl:call-template name="href.target">
              <xsl:with-param name="object" select="."/>
              <xsl:with-param name="context" select="."/>
            </xsl:call-template>
          </xsl:attribute>
          <xsl:if test="$class = 'single-crumb'">
            <span class="single-contents-icon"> </span>
          </xsl:if>
          <xsl:if test="$context = 'fixed-header'">
            <xsl:call-template name="gentext">
              <xsl:with-param name="key">showcontentsoverview</xsl:with-param>
            </xsl:call-template>
            <xsl:call-template name="gentext">
              <xsl:with-param name="key">admonseparator</xsl:with-param>
            </xsl:call-template>
          </xsl:if>
          <xsl:value-of select="string($title)"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:element>
  </xsl:template>

  <xsl:template name="breadcrumbs.navigation">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="context">header</xsl:param>
    <xsl:param name="debug"/>

    <xsl:if test="$generate.breadcrumbs != 0">
      <div class="crumbs">
        <xsl:call-template name="generate.breadcrumbs.back"/>
        <xsl:choose>
          <xsl:when test="$rootelementname != 'article'">
            <xsl:for-each select="ancestor-or-self::*">
              <xsl:choose>
                <xsl:when test="$rootid != '' and descendant::*[@id = string($rootid)]"/>
                <xsl:when test="not(ancestor::*) or @id = string($rootid)">
                  <xsl:apply-templates select="." mode="breadcrumbs">
                    <xsl:with-param name="class">book-link</xsl:with-param>
                  </xsl:apply-templates>
                </xsl:when>
                <xsl:otherwise>
                  <span><xsl:copy-of select="$daps.breadcrumbs.sep"/></span>
                  <xsl:apply-templates select="." mode="breadcrumbs"/>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:for-each>
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-templates select="." mode="breadcrumbs">
              <xsl:with-param name="class">single-crumb</xsl:with-param>
              <xsl:with-param name="context" select="$context"/>
            </xsl:apply-templates>
            <xsl:if test="$context = 'header'">
              <div class="bubble-corner active-contents"> </div>
            </xsl:if>
          </xsl:otherwise>
        </xsl:choose>
      </div>
    </xsl:if>
  </xsl:template>

  <!-- ===================================================== -->
  <xsl:template name="picker.selection">
    <a class="selected" href="#">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key">formathtml</xsl:with-param>
      </xsl:call-template>
    </a>
    <a href="#">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key">formatsinglehtml</xsl:with-param>
      </xsl:call-template>
    </a>
  </xsl:template>

  <!-- ===================================================== -->

  <xsl:template name="create.header.buttons.nav">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:if test="not(self::set|self::article)">
      <div class="button">
        <xsl:call-template name="header.navigation">
          <xsl:with-param name="next" select="$next"/>
          <xsl:with-param name="prev" select="$prev"/>
          <!--<xsl:with-param name="nav.context" select="$nav.context"/>-->
        </xsl:call-template>
      </div>
    </xsl:if>
</xsl:template>

  <!-- ===================================================== -->
  <xsl:template name="fixed-header-wrap">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="nav.context"/>

    <xsl:if test="$generate.fixed.header != 0">
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
        <xsl:if test="$generate.bubbletoc != 0 and $rootelementname = 'article'">
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
    </xsl:if>
  </xsl:template>

  <xsl:template name="create-find-area">
    <xsl:param name="prev" select="/foo"/>
    <xsl:param name="next" select="/foo"/>
    <xsl:variable name="localisationfind">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key">find</xsl:with-param>
      </xsl:call-template>
    </xsl:variable>

    <div id="_find-area" class="active">
      <div class="inactive-contents">
        <a href="#" id="_find-area-button" class="tool" title="{$localisationfind}">
          <span class="pad-tools-50-out">
            <span class="pad-tools-50-in">
              <span class="tool-spacer">
                <span class="find-icon"><xsl:value-of select="$localisationfind"/></span>
              </span>
              <span class="tool-label"><xsl:value-of select="$localisationfind"/></span>
            </span>
          </span>
        </a>
      </div>
      <div class="active-contents">
        <form action="post">
          <div class="find-form">
            <input type="text" id="_find-input" value="{$localisationfind}"/>
            <button id="_find-button" alt="{$localisationfind}" title="{$localisationfind}">
              <xsl:value-of select="$localisationfind"/>
            </button>
            <label id="_find-input-label">
              <xsl:value-of select="$localisationfind"/>
            </label>
            <xsl:call-template name="clearme"/>
          </div>
        </form>
      </div>
    </div>
  </xsl:template>

  <!-- ===================================================== -->
  <xsl:template name="toolbar-wrap">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="nav.context"/>
    <xsl:variable name="rootnode"  select="generate-id(/*) = generate-id(.)"/>
        <xsl:variable name="localisationcontents">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key">contentsoverview</xsl:with-param>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="title.candidate">
      <xsl:apply-templates mode="title.markup"
        select="((ancestor-or-self::book | ancestor-or-self::article)|key('id', $rootid))[last()]"/>
    </xsl:variable>
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="$title.candidate != ''">
          <xsl:value-of select="$title.candidate"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="gentext">
            <xsl:with-param name="key"
              select="local-name((ancestor-or-self::book | ancestor-or-self::article)[last()])"/>
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:if test="$generate.toolbar != 0 and $rootelementname != 'article'">
      <div id="_toolbar-wrap">
        <div id="_toolbar">
          <xsl:choose>
            <!-- We don't need a bubble-toc in the page for set. -->
            <!-- FIXME: Haven't looked any further, but just testing for
                 rootnode seems wrong. One might go so far as to say that
                 $rootnode does not have to be a set. -->
            <xsl:when test="$generate.bubbletoc = 0 or $rootnode">
              <div id="_toc-area" class="inactive"> </div>
            </xsl:when>
            <xsl:otherwise>
              <div id="_toc-area" class="inactive">
                <a id="_toc-area-button" class="tool" title="{$localisationcontents}"
                 accesskey="c">
                  <xsl:attribute name="href">
                    <xsl:call-template name="href.target">
                      <xsl:with-param name="object" select="/*"/>
                    </xsl:call-template>
                  </xsl:attribute>
                  <span class="tool-spacer">
                    <span class="toc-icon">
                      <xsl:value-of select="$localisationcontents"/>
                    </span>
                    <xsl:call-template name="clearme">
                      <xsl:with-param name="wrapper">span</xsl:with-param>
                    </xsl:call-template>
                  </span>
                  <span class="tool-label">
                    <xsl:value-of select="$localisationcontents"/>
                  </span>
                </a>
                <div class="active-contents bubble-corner"> </div>
                <div class="active-contents bubble">
                  <div class="bubble-container">
                    <h6>
                      <xsl:value-of select="$title"/>
                    </h6>
                    <div id="_bubble-toc">
                      <xsl:call-template name="bubble-toc"/>
                    </div>
                    <xsl:call-template name="clearme"/>
                  </div>
                </div>
              </div>
            </xsl:otherwise>
          </xsl:choose>

          <xsl:choose>
            <xsl:when test="self::set|self::article">
              <div id="_nav-area" class="inactive"></div>
            </xsl:when>
            <xsl:otherwise>
              <div id="_nav-area" class="inactive">
                <div class="tool">
                  <span class="nav-inner">
                    <span class="tool-label">
                      <xsl:call-template name="gentext">
                        <xsl:with-param name="key">navigation</xsl:with-param>
                      </xsl:call-template>
                    </span>
                    <!-- Add navigation -->
                    <xsl:call-template name="header.navigation">
                      <xsl:with-param name="next" select="$next"/>
                      <xsl:with-param name="prev" select="$prev"/>
                      <xsl:with-param name="nav.context" select="$nav.context"/>
                    </xsl:call-template>
                  </span>
                </div>
              </div>
            </xsl:otherwise>
          </xsl:choose>

          <!-- Do not create a Find area for now, instead of focus on remaining
               missing essentials. Make Find functional later. -->
          <!-- <xsl:call-template name="create-find-area">
            <xsl:with-param name="next" select="$next"/>
            <xsl:with-param name="prev" select="$prev"/>
            <xsl:with-param name="nav.context" select="$nav.context"/>
          </xsl:call-template> -->

        </div>
      </div>
    </xsl:if>
  </xsl:template>

  <!-- ===================================================== -->
  <xsl:template name="header.navigation">
    <xsl:param name="prev" select="/foo"/>
    <xsl:param name="next" select="/foo"/>
    <xsl:param name="nav.context"/>

    <xsl:variable name="needs.navig">
      <xsl:call-template name="is.node.in.navig">
        <xsl:with-param name="next" select="$next"/>
        <xsl:with-param name="prev" select="$prev"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="isnext">
      <xsl:call-template name="is.next.node.in.navig">
        <xsl:with-param name="next" select="$next"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="isprev">
      <xsl:call-template name="is.prev.node.in.navig">
        <xsl:with-param name="prev" select="$prev"/>
      </xsl:call-template>
    </xsl:variable>

  <!--
     We use two node sets and calculate the set difference
     with the following, general XPath expression:

      setdiff = $node-set1[count(.|$node-set2) != count($node-set2)]

     $node-set1 contains the ancestors of all nodes, starting with the
     current node (but the current node is NOT included in the set)

     $node-set2 contains the ancestors of all nodes starting from the
     node which points to the $rootid parameter

     For example:
     node-set1: {/, set, book, chapter}
     node-set2: {/, set, }
     setdiff:   {        book, chapter}
  -->
  <xsl:variable name="ancestorrootnode" select="key('id', $rootid)/ancestor::*"/>
  <xsl:variable name="setdiff" select="ancestor::*[count(. | $ancestorrootnode)
                                != count($ancestorrootnode)]"/>
  <xsl:if test="$needs.navig">
       <xsl:choose>
         <xsl:when test="count($prev) > 0 and $isprev = 'true'">
           <a accesskey="p" class="tool-spacer">
             <xsl:attribute name="title">
               <xsl:apply-templates select="$prev"
                 mode="object.title.markup"/>
             </xsl:attribute>
             <xsl:attribute name="href">
               <xsl:call-template name="href.target">
                 <xsl:with-param name="object" select="$prev"/>
               </xsl:call-template>
             </xsl:attribute>
             <span class="prev-icon">←</span>
           </a>
         </xsl:when>
         <xsl:otherwise>
           <span class="tool-spacer">
             <span class="prev-icon">←</span>
           </span>
         </xsl:otherwise>
       </xsl:choose>
       <xsl:choose>
         <xsl:when test="count($next) > 0 and $isnext = 'true'">
           <a accesskey="n" class="tool-spacer">
             <xsl:attribute name="title">
               <xsl:apply-templates select="$next"
                 mode="object.title.markup"/>
             </xsl:attribute>
             <xsl:attribute name="href">
               <xsl:call-template name="href.target">
                 <xsl:with-param name="object" select="$next"/>
               </xsl:call-template>
             </xsl:attribute>
             <span class="next-icon">→</span>
           </a>
         </xsl:when>
         <xsl:otherwise>
           <span class="tool-spacer">
             <span class="next-icon">→</span>
           </span>
         </xsl:otherwise>
       </xsl:choose>
    </xsl:if>
  </xsl:template>


  <!-- ===================================================== -->
  <xsl:template name="bottom.navigation">
    <xsl:param name="prev" select="/foo"/>
    <xsl:param name="next" select="/foo"/>
    <xsl:param name="nav.context"/>
    <xsl:variable name="needs.navig">
      <xsl:call-template name="is.node.in.navig">
        <xsl:with-param name="next" select="$next"/>
        <xsl:with-param name="prev" select="$prev"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="isnext">
      <xsl:call-template name="is.next.node.in.navig">
        <xsl:with-param name="next" select="$next"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="isprev">
      <xsl:call-template name="is.prev.node.in.navig">
        <xsl:with-param name="prev" select="$prev"/>
      </xsl:call-template>
    </xsl:variable>

    <!--
     We use two node sets and calculate the set difference
     with the following, general XPath expression:

      setdiff = $node-set1[count(.|$node-set2) != count($node-set2)]

     $node-set1 contains the ancestors of all nodes, starting with the
     current node (but the current node is NOT included in the set)

     $node-set2 contains the ancestors of all nodes starting from the
     node which points to the $rootid parameter

     For example:
     node-set1: {/, set, book, chapter}
     node-set2: {/, set, }
     setdiff:   {        book, chapter}
  -->
    <xsl:variable name="ancestorrootnode"
      select="key('id', $rootid)/ancestor::*"/>
    <xsl:variable name="setdiff"
      select="ancestor::*[count(. | $ancestorrootnode)
                                != count($ancestorrootnode)]"/>
    <xsl:if test="$generate.bottom.navigation != 0 and $needs.navig = 'true' and not(self::set)">
      <div id="_bottom-navigation">
        <xsl:if test="count($next) > 0 and $isnext = 'true'">
          <a class="nav-link">
            <xsl:attribute name="href">
              <xsl:call-template name="href.target">
                <xsl:with-param name="object" select="$next"/>
              </xsl:call-template>
            </xsl:attribute>
            <span class="next-icon">→</span>
            <span class="nav-label">
              <xsl:apply-templates select="$next" mode="page-bottom.label"/>
              <xsl:apply-templates select="$next" mode="titleabbrev.markup"/>
            </span>
          </a>
        </xsl:if>
        <xsl:if test="count($prev) > 0 and $isprev = 'true'">
          <a class="nav-link">
            <xsl:attribute name="href">
              <xsl:call-template name="href.target">
                <xsl:with-param name="object" select="$prev"/>
              </xsl:call-template>
            </xsl:attribute>
            <span class="prev-icon">←</span>
            <span class="nav-label">
              <xsl:apply-templates select="$prev" mode="page-bottom.label"/>
              <xsl:apply-templates select="$prev" mode="titleabbrev.markup"/>
            </span>
          </a>
      </xsl:if>
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template match="chapter|appendix|part" mode="page-bottom.label">
    <xsl:variable name="template">
      <xsl:call-template name="gentext.template">
        <xsl:with-param name="context" select="'xref-number'"/>
        <xsl:with-param name="name" select="local-name()"/>
      </xsl:call-template>
    </xsl:variable>

    <span class="number">
      <xsl:call-template name="substitute-markup">
        <xsl:with-param name="template" select="$template"/>
      </xsl:call-template>
      <xsl:text> </xsl:text>
    </span>
  </xsl:template>

  <xsl:template match="*" mode="page-bottom.label"/>

  <!-- ===================================================== -->
  <xsl:template name="chunk-element-content">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="nav.context"/>
    <xsl:param name="content">
      <xsl:apply-imports/>
    </xsl:param>

    <xsl:call-template name="chunk-element-content-html">
      <xsl:with-param name="content" select="$content"/>
      <xsl:with-param name="next" select="$next"/>
      <xsl:with-param name="prev" select="$prev"/>
      <xsl:with-param name="nav.context" select="$nav.context"/>
    </xsl:call-template>
  </xsl:template>

  <!--
     Needed to allow customization by drupal/xhtml/chunk.xsl
     (circumvent import issues with <xsl:apply-imports/>)
  -->
  <xsl:template name="chunk-element-content-html">
    <xsl:param name="prev"/>
    <xsl:param name="next"/>
    <xsl:param name="nav.context"/>
    <xsl:param name="content"/>

    <xsl:variable name="lang">
      <xsl:apply-templates select="(ancestor-or-self::*/@lang)[last()]" mode="html.lang.attribute"/>
    </xsl:variable>

    <xsl:call-template name="user.preroot"/>

    <html lang="{$lang}" xml:lang="{$lang}">
      <xsl:call-template name="root.attributes"/>
      <xsl:call-template name="html.head">
        <xsl:with-param name="prev" select="$prev"/>
        <xsl:with-param name="next" select="$next"/>
      </xsl:call-template>

      <body>
        <xsl:call-template name="body.attributes"/>
        <xsl:call-template name="outerelement.class.attribute"/>
        <xsl:call-template name="bypass">
          <xsl:with-param name="format" select="'chunk'"/>
        </xsl:call-template>
        <div id="_outer-wrap">
          <xsl:if test="$generate.header != 0">
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
          </xsl:if>

          <xsl:call-template name="toolbar-wrap">
            <xsl:with-param name="next" select="$next"/>
            <xsl:with-param name="prev" select="$prev"/>
          </xsl:call-template>

          <xsl:call-template name="fixed-header-wrap">
            <xsl:with-param name="next" select="$next"/>
            <xsl:with-param name="prev" select="$prev"/>
          </xsl:call-template>

          <xsl:call-template name="user.header.navigation">
            <xsl:with-param name="prev" select="$prev"/>
            <xsl:with-param name="next" select="$next"/>
            <xsl:with-param name="nav.context" select="$nav.context"/>
          </xsl:call-template>

          <xsl:call-template name="user.header.content"/>
          <xsl:if test="$rootelementname = 'article'">
            <div id="_toc-bubble-wrap"></div>
          </xsl:if>
          <div id="_content">
            <xsl:call-template name="outerelement.class.attribute">
              <xsl:with-param name="node" select="'id-content'"/>
            </xsl:call-template>
            <div class="documentation">

            <xsl:copy-of select="$content"/>

            </div>
            <div class="page-bottom">
              <xsl:call-template name="bottom.navigation">
                <xsl:with-param name="prev" select="$prev"/>
                <xsl:with-param name="next" select="$next"/>
                <xsl:with-param name="nav.context" select="$nav.context"/>
              </xsl:call-template>
              <xsl:call-template name="share.and.print">
                <xsl:with-param name="prev" select="$prev"/>
                <xsl:with-param name="next" select="$next"/>
                <xsl:with-param name="nav.context" select="$nav.context"/>
              </xsl:call-template>
            </div>
          </div>
          <div id="_inward"></div>
        </div>

        <div id="_footer-wrap">
          <xsl:call-template name="user.footer.content"/>
          <xsl:call-template name="user.footer.navigation">
            <xsl:with-param name="prev" select="$prev"/>
            <xsl:with-param name="next" select="$next"/>
            <xsl:with-param name="nav.context" select="$nav.context"/>
          </xsl:call-template>
        </div>

    </body>
  </html>
</xsl:template>

<!-- ======================================================================= -->

  <xsl:template name="bubble-toc">
    <xsl:if test="$generate.bubbletoc != 0">
      <xsl:call-template name="bubble-toc.inner">
        <xsl:with-param name="node" select="((ancestor-or-self::book | ancestor-or-self::article)|key('id', $rootid))[last()]"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
