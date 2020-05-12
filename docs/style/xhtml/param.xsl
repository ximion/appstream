<?xml version="1.0"?>
<!--
  Purpose:
     Contains all parameters for (X)HTML
     (Sorted against the list in "Part 1. HTML Parameter Reference" in
      the DocBook XSL Stylesheets User Reference, see link below)

   See Also:
     * http://docbook.sourceforge.net/release/xsl/current/doc/html/index.html

   Author(s):     Thomas Schraitle <toms@opensuse.org>,
                  Stefan Knorr <sknorr@suse.de>
   Copyright: 2012, 2013, Thomas Schraitle, Stefan Knorr

-->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:saxon="http://icl.com/saxon"
  extension-element-prefixes="saxon"
  exclude-result-prefixes="saxon"
  xmlns="http://www.w3.org/1999/xhtml">

<!-- 0. Parameters for External Manipulation =================== -->
  <!-- Add a link to a product/company homepage to the logo -->
  <xsl:param name="homepage" select="'https://www.freedesktop.org/software/appstream/docs/'"/>
    <!-- Override with:
             -stringparam="homepage=https://www.example.com"
    -->

  <!-- Add a link back (up) to an external overview page. -->
  <xsl:param name="overview-page" select="''"/>
  <xsl:param name="overview-page-title" select="''"/>
    <!-- Override with:
             -stringparam="overview-page=https://www.example.com"
             -stringparam="overview-page-title='Back to Overview'"
    -->

  <!-- Base URL for <link rel=canonical> tags. No tags included if unset. -->
  <xsl:param name="canonical-url-base" select="''"/>
    <!-- Override with:
            -stringparam="canonical-url-base=https://www.example.com"
    -->

  <!-- Toggle inclusion of @font-face CSS. Set to 1 if you want to host
       the HTML on the internet or 0 if you are building for a locally
       installed package. -->
  <xsl:param name="build.for.web" select="1"/>
    <!-- Override with:
            -param="build.for.web=0"
    -->

  <!-- Whether to optimize for plain text output -->
  <xsl:param name="optimize.plain.text" select="0"/>
    <!-- Override with:
            -param="optimize.plain.text=1"
    -->

  <!-- Show "Edit Source" link always or only when draft mode is on.
       (dm:editurl needs to be defined in the document) -->
  <xsl:param name="show.edit.link" select="0"/>
    <!-- Override with:
            -param="show.edit.link=0"
    -->

<!-- 1. Admonitions  ============================================ -->
  <!-- Use graphics in admonitions?  0=no, 1=yes -->
  <xsl:param name="admon.graphics" select="1"/>
  <!-- Path to admonition graphics -->
  <xsl:param name="admon.graphics.path">static/images/</xsl:param>
  <!-- Specifies the CSS style attribute that should be added to admonitions -->
  <xsl:param name="admon.style" select="''"/>


 <xsl:param name="chunker.output.method">
   <xsl:choose>
     <xsl:when test="contains(system-property('xsl:vendor'), 'SAXON')">saxon:xhtml</xsl:when>
     <xsl:otherwise>xml</xsl:otherwise>
   </xsl:choose>
 </xsl:param>

<!-- 3. EBNF ==================================================== -->

<!-- 4. ToC/LoT/Index Generation ================================ -->
  <xsl:param name="toc.section.depth" select="1"/>
  <xsl:param name="generate.toc">
appendix  toc,title
article/appendix  nop
article   toc,title
book      toc,title,figure,table,example,equation
chapter   toc,title
part      toc,title
preface   toc,title
qandaset  nop
reference toc,title
sect1     toc
sect2     toc
sect3     toc
sect4     toc
sect5     toc
section   toc
set       toc,title
</xsl:param>

<!-- 5. Stylesheet Extensions =================================== -->

<!-- 6. Automatic labeling ====================================== -->
  <xsl:param name="section.autolabel" select="1"/>
  <xsl:param name="section.label.includes.component.label" select="1"/>

<!-- 7. HTML ==================================================== -->
  <xsl:param name="css.decoration" select="0"/>
  <xsl:param name="docbook.css.link" select="0"/>

  <!-- To add e.g. brand specific additional stylesheets -->
  <xsl:param name="extra.css" select="''"/>
  <xsl:param name="docbook.css.source"/>
    <!-- Intentionally left empty – we already have a stylesheet, with this, we
         only override DocBook's default. -->

    <!-- The &#10;s introduce the necessary linebreak into this variable made up
         of variables... ugh. -->
  <xsl:param name="html.stylesheet">
<xsl:if test="$build.for.web != 1">static/css/fonts-onlylocal.css</xsl:if><xsl:text>&#10;</xsl:text>
<xsl:value-of select="$daps.header.css.standard"/><xsl:text>&#10;</xsl:text>
<xsl:if test="$enable.source.highlighting = 1"><xsl:value-of select="$daps.header.css.highlight"/><xsl:text>&#10;</xsl:text></xsl:if>
<xsl:value-of select="$extra.css"/>
</xsl:param>
  <xsl:param name="make.clean.html" select="1"/>
  <xsl:param name="make.valid.html" select="1"/>
  <xsl:param name="enable.source.highlighting" select="1"/>

  <xsl:param name="generate.id.attributes" select="1"/>

<!-- 8. XSLT Processing ========================================= -->
  <!-- Rule over footers? -->
  <xsl:param name="footer.rule" select="0"/>

<!-- 9. Meta/*Info and Titlepages =============================== -->
  <xsl:param name="generate.legalnotice.link" select="0"/>

<!-- 10. Reference Pages ======================================== -->

<!-- 11. Tables ================================================= -->

<!-- 12. QAndASet =============================================== -->
<xsl:param name="qanda.defaultlabel">none</xsl:param>
<xsl:param name="qandadiv.autolabel" select="0"></xsl:param>

<!-- 13. Linking ================================================ -->
<xsl:param name="ulink.target">_blank</xsl:param>
<xsl:param name="generate.consistent.ids" select="1"/>

<!-- 14. Cross References ======================================= -->

<!-- 15. Lists ================================================== -->

<!-- 16. Bibliography =========================================== -->

<!-- 17. Glossary =============================================== -->

<!-- 18. Miscellaneous ========================================== -->
  <xsl:param name="menuchoice.separator" select="$daps.breadcrumbs.sep"/>
  <xsl:param name="formal.title.placement">
figure after
example before
equation before
table before
procedure before
task before
  </xsl:param>
 
  <xsl:param name="runinhead.default.title.end.punct">:</xsl:param>

  <!-- From the DocBook XHMTL stylesheet's "formal.xsl" -->
  <xsl:param name="formal.object.break.after">0</xsl:param>

<!-- 19. Annotations ============================================ -->

<!-- 20. Graphics =============================================== -->
  <xsl:param name="img.src.path">images/</xsl:param><!-- DB XSL Version >=1.67.1 -->
  <xsl:param name="make.graphic.viewport" select="0"/> <!-- Do not create tables around graphics. -->
  <xsl:param name="link.to.self.for.mediaobject" select="1"/> <!-- Create links to the image itself around images. -->


<!-- 21. Chunking =============================================== -->
  <!-- The base directory of chunks -->
  <xsl:param name="base.dir">./html/</xsl:param>

  <xsl:param name="chunk.fast" select="1"/>
  <!-- Depth to which sections should be chunked -->
  <xsl:param name="chunk.section.depth" select="1"/>
  <!-- Use ID value of chunk elements as the filename? -->
  <xsl:param name="use.id.as.filename" select="1"/>

  <!-- Use graphics in navigational headers and footers? -->
  <xsl:param name="navig.graphics" select="1"/>
  <!-- Path to navigational graphics -->
  <xsl:param name="navig.graphics.path">navig/</xsl:param>
  <!-- Extension for navigational graphics -->
  <xsl:param name="navig.graphics.extension" select="'.png'"/>
  <!-- Identifies the name of the root HTML file when chunking -->
  <xsl:param name="root.filename">index</xsl:param>


<!-- 27. Localization =========================================== -->
  <!-- Use customized language files -->
  <xsl:param name="local.l10n.xml" select="document('../common/l10n/l10n.xml')"/>

<!-- 28. Style specific parameters =============================== -->
  <xsl:param name="is.chunk" select="0"/>

  <xsl:param name="admon.graphics.prefix">icon-</xsl:param>

  <!-- Create an image tag for the logo? -->
  <xsl:param name="generate.logo">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <xsl:param name="daps.header.logo">static/images/logo.png</xsl:param>
  <xsl:param name="daps.header.logo.text">AppStream</xsl:param>
  <xsl:param name="daps.header.logo.alt">Logo</xsl:param>
  <xsl:param name="daps.header.js.custom"></xsl:param>
  <xsl:param name="daps.header.js.highlight">static/js/highlight.min.js</xsl:param>
  <xsl:param name="daps.header.css.standard">static/css/style.css</xsl:param>
  <xsl:param name="daps.header.css.highlight">static/css/highlight.css</xsl:param>

  <!-- This list is intentionally quite strict (no aliases) to keep our documents
  consistent. -->
  <xsl:param name="highlight.supported.languages" select="'apache|bash|c++|css|diff|html|xml|http|ini|json|java|javascript|makefile|nginx|php|perl|python|ruby|sql|crmsh|dockerfile|lisp|yaml'"/>

  <xsl:param name="generate.header">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <!-- Create fixed header bar at the top?  -->
  <xsl:param name="generate.fixed.header">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <xsl:param name="generate.toolbar">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <xsl:param name="generate.bottom.navigation">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <!-- Create breadcrumbs navigation?  -->
  <xsl:param name="generate.breadcrumbs">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <xsl:param name="breadcrumbs.prev">&#9664;<!--&#9668;--></xsl:param>
  <xsl:param name="breadcrumbs.next">&#9654;<!--&#9658;--></xsl:param>

  <!--  Bubble TOC options -->

  <!-- Create bubbletoc?  -->
  <xsl:param name="generate.bubbletoc">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <xsl:param name="rootelementname">
    <xsl:choose>
      <xsl:when test="local-name(key('id', $rootid)) != ''">
        <xsl:value-of select="local-name(key('id', $rootid))"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="local-name(/*)"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <xsl:param name="bubbletoc.section.depth">2</xsl:param>
  <xsl:param name="bubbletoc.max.depth">2</xsl:param>

  <xsl:param name="bubbletoc.max.depth.shallow">
    <xsl:choose>
      <xsl:when test="$rootelementname = 'article'">
        <xsl:value-of select="1"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <!-- Generate links in said footer? -->
  <xsl:param name="generate.footer.links">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <!-- Separator for chapter and book name in the XHTML output -->
  <xsl:param name="head.content.title.separator"> | </xsl:param>

  <!-- Create version information before title? -->
  <xsl:param name="generate.version.info" select="1"/>

  <!-- Create the language and format areas in the header? -->
  <xsl:param name="generate.pickers">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <!-- These are only dummies currently, so we never want to gnerate them. -->
      <xsl:otherwise>0</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <!-- Separator between breadcrumbs links: -->
  <xsl:param name="daps.breadcrumbs.sep">&#xa0;›&#xa0;</xsl:param>

  <!--  Create permalinks?-->
  <xsl:param name="generate.permalinks">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>

  <!-- Should information from SVN properties be used? yes=1|no=0 -->
  <xsl:param name="use.meta" select="0"/>

  <!-- Should the tracker meta information be processed? yes=1|no=0 -->
  <xsl:param name="use.tracker.meta">
    <xsl:choose>
      <xsl:when test="$optimize.plain.text = 1">0</xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:param>
  <!-- Show arrows before and after a paragraph that applies only to a certain
       architecture? -->
  <xsl:param name="para.use.arch" select="1"/>

  <!-- Output a warning, if chapter/@lang is different from book/@lang ?
       0=no, 1=yes
  -->
<xsl:param name="warn.xrefs.into.diff.lang" select="1"/>

<xsl:param name="glossentry.show.acronym">yes</xsl:param>

  <!-- Wrap <img/> tags with a link (<a>), so you can click through to a big
       version of the image. -->
  <xsl:param name="wrap.img.with.a" select="1"/>

  <!-- Trim away empty lines from the beginning and end of screens -->
  <xsl:param name="trim.verbatim" select="1"/>

  <!-- The amount of characters allowed for teasers on part pages and within
       og:description/meta description tags.-->
  <xsl:param name="teaser.length" select="300"/>

</xsl:stylesheet>
