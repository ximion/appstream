<?xml version="1.0" encoding="UTF-8"?>
<!-- 
  Purpose:
     Create table of content structures

   Author:    Thomas Schraitle <toms@opensuse.org>
   Copyright: 2012, Thomas Schraitle

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:exsl="http://exslt.org/common"
  xmlns="http://www.w3.org/1999/xhtml"
  exclude-result-prefixes="exsl">


  <xsl:template name="make.toc">
    <xsl:param name="toc-context" select="."/>
    <xsl:param name="toc.title.p" select="true()"/>
    <xsl:param name="nodes" select="/NOT-AN-ELEMENT"/>

    <xsl:variable name="nodes.plus" select="$nodes | qandaset"/>

    <xsl:choose>
      <xsl:when test="$manual.toc != ''">
        <xsl:variable name="id">
          <xsl:call-template name="object.id"/>
        </xsl:variable>
        <xsl:variable name="toc" select="document($manual.toc, .)"/>
        <xsl:variable name="tocentry" select="$toc//tocentry[@linkend=$id]"/>
        <xsl:if test="$tocentry and $tocentry/*">
          <div class="toc">
            <!--<xsl:copy-of select="$toc.title"/>-->
            <xsl:element name="{$toc.list.type}" namespace="http://www.w3.org/1999/xhtml">
              <xsl:call-template name="manual-toc">
                <xsl:with-param name="tocentry" select="$tocentry/*[1]"/>
              </xsl:call-template>
            </xsl:element>
          </div>
        </xsl:if>
      </xsl:when>
      <xsl:otherwise>
        <xsl:choose>
          <xsl:when test="$qanda.in.toc != 0">
            <xsl:if test="$nodes.plus">
              <div class="toc">
                <!--<xsl:copy-of select="$toc.title"/>-->
                <xsl:element name="{$toc.list.type}" namespace="http://www.w3.org/1999/xhtml">
                  <xsl:choose>
                    <xsl:when test="local-name($toc-context) = 'part' or local-name($toc-context) = 'set'">
                      <xsl:apply-templates select="$nodes.plus" mode="toc-abstract">
                        <xsl:with-param name="toc-context" select="$toc-context"/>
                      </xsl:apply-templates>
                    </xsl:when>
                    <xsl:otherwise>
                      <xsl:apply-templates select="$nodes.plus" mode="toc">
                        <xsl:with-param name="toc-context" select="$toc-context"/>
                      </xsl:apply-templates>
                    </xsl:otherwise>
                  </xsl:choose>
                </xsl:element>
              </div>
            </xsl:if>
          </xsl:when>
          <xsl:otherwise>
            <xsl:if test="$nodes">
              <div class="toc">
                <xsl:element name="{$toc.list.type}" namespace="http://www.w3.org/1999/xhtml">
                  <xsl:choose>
                    <xsl:when test="local-name($toc-context) = 'part' or local-name($toc-context) = 'set'">
                      <xsl:apply-templates select="$nodes" mode="toc-abstract">
                        <xsl:with-param name="toc-context" select="$toc-context"/>
                      </xsl:apply-templates>
                    </xsl:when>
                    <xsl:otherwise>
                      <xsl:apply-templates select="$nodes" mode="toc">
                        <xsl:with-param name="toc-context" select="$toc-context"/>
                      </xsl:apply-templates>
                    </xsl:otherwise>
                  </xsl:choose>
                </xsl:element>
              </div>
            </xsl:if>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="toc.line">
    <xsl:param name="toc-context" select="."/>
    <xsl:param name="depth" select="1"/>
    <xsl:param name="depth.from.context" select="8"/>

    <xsl:variable name="label.in.toc">
    <!-- Flag all elements which do not need an label 
        0 = no, don't create a label
        1 = yes, create a label
    -->
      <xsl:choose>
        <xsl:when test="self::article">0</xsl:when>
        <xsl:when test="self::book">0</xsl:when>
        <xsl:when test="ancestor-or-self::preface and
                        number($preface.autolabel) = 0">0</xsl:when>
        <xsl:when test="ancestor-or-self::glossary">0</xsl:when>
        <xsl:when test="ancestor-or-self::bibliography">0</xsl:when>
        <xsl:otherwise>1</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

 <!-- <xsl:message>label.in.toc: <xsl:value-of select="concat(local-name(),
    ':', $label.in.toc )"/></xsl:message> -->

    <span>
      <xsl:attribute name="class"><xsl:value-of select="local-name(.)"/></xsl:attribute>
  <!-- * if $autotoc.label.in.hyperlink is zero, then output the label -->
  <!-- * before the hyperlinked title (as the DSSSL stylesheet does) -->
      <a>
        <xsl:attribute name="href">
          <xsl:call-template name="href.target">
            <xsl:with-param name="context" select="$toc-context"/>
            <xsl:with-param name="toc-context" select="$toc-context"/>
          </xsl:call-template>
        </xsl:attribute>

  <!-- * if $autotoc.label.in.hyperlink is non-zero, then output the label -->
  <!-- * as part of the hyperlinked title -->
        <xsl:if test="not($autotoc.label.in.hyperlink = 0) and 
                          number($label.in.toc) != 0">
          <xsl:variable name="label">
            <span class="number">
              <xsl:apply-templates select="." mode="label.markup"/>
              <xsl:text> </xsl:text>
            </span>
          </xsl:variable>
          <xsl:copy-of select="$label"/>
        </xsl:if>
        <span class="name">
          <xsl:apply-templates select="." mode="titleabbrev.markup"/>
        </span>
      </a>
    </span>
  </xsl:template>


  <!-- http://sagehill.net/docbookxsl/TOCcontrol.html#BriefSetToc -->
  <xsl:template match="book|part/appendix|chapter|toc|lot|index|glossary|
                       bibliography|article|preface|refentry|reference" mode="toc-abstract">
    <xsl:param name="toc-context" select="."/>

    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="EMPTY"/>
    </xsl:call-template>
    <xsl:choose>
      <xsl:when test="self::book">
        <xsl:apply-templates select="(bookinfo/abstract|bookinfo/highlights|abstract|highlights)[1]" mode="toc-abstract"/>
      </xsl:when>
      <xsl:when test="self::appendix|self::chapter|self::toc|self::lot|
                      self::index|self::glossary|self::bibliography|
                      self::article|self::preface|self::refentry|self::reference
                  and local-name($toc-context) = 'part'">
        <xsl:choose>
          <xsl:when test="*[contains(local-name(), 'info')]/abstract|
                          *[contains(local-name(), 'info')]/highlights|
                          abstract|highlights">
            <xsl:apply-templates select="(*[contains(local-name(), 'info')]/abstract|
                                          *[contains(local-name(), 'info')]/highlights|
                                          abstract|highlights)[1]" mode="toc-abstract"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-templates select="(descendant::para|descendant::simpara)[1]" mode="toc-abstract">
              <xsl:with-param name="trim" select="1"/>
            </xsl:apply-templates>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="abstract|highlights|para|simpara" mode="toc-abstract">
    <xsl:param name="trim" select="0"/>
    <xsl:param name="teaser">
      <xsl:apply-templates/>
    </xsl:param>
    <dd class="toc-abstract">
      <xsl:choose>
        <xsl:when test="$trim = 1
                    and string-length(normalize-space($teaser)) &gt; $teaser.length">
            <p>
            <xsl:value-of select="substring(normalize-space($teaser),1,$teaser.length)"/>
            <xsl:value-of select="'…'"/>
            </p>
        </xsl:when>
        <xsl:when test="$trim = 1">
            <p>
            <xsl:apply-templates/>
            </p>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates/>
        </xsl:otherwise>
      </xsl:choose>
    </dd>
  </xsl:template>


  <xsl:template match="figure|table|example|equation|procedure" mode="toc">
    <xsl:param name="toc-context" select="."/>

    <xsl:element name="{$toc.listitem.type}"
      namespace="http://www.w3.org/1999/xhtml">
      <xsl:variable name="label">
        <span class="number">
          <xsl:apply-templates select="." mode="label.markup"/>
          <xsl:text> </xsl:text>
        </span>
      </xsl:variable>
      <span>
        <xsl:attribute name="class"><xsl:value-of select="local-name(.)"/></xsl:attribute>
        <a>
          <xsl:attribute name="href">
            <xsl:call-template name="href.target">
              <xsl:with-param name="toc-context" select="$toc-context"/>
            </xsl:call-template>
          </xsl:attribute>
          <xsl:copy-of select="$label"/>
          <span class="name">
            <xsl:apply-templates select="." mode="titleabbrev.markup"/>
          </span>
        </a>
      </span>
    </xsl:element>
  </xsl:template>


</xsl:stylesheet>
