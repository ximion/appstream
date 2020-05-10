<?xml version="1.0" encoding="ASCII"?>
<!--
    Purpose:
    Add a wrapper div around procedures, so the title can be left-aligned while
    everything else in the procedure gets a little border on the left and thus
    is padded a bit more.
    Also make sure, variablelist entries can be linked to.

    Author(s): Thomas Schraitle <toms@opensuse.org>,
      Stefan Knorr <sknorr@suse.de>, Janina Setz <jsetz@suse.com>
    Copyright: 2012, 2016, Thomas Schraitle, Stefan Knorr, Janina Setz

-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:exsl="http://exslt.org/common"
    xmlns="http://www.w3.org/1999/xhtml"
    exclude-result-prefixes="exsl">


  <xsl:template match="varlistentry">
    <dt>
      <xsl:call-template name="id.attribute">
        <xsl:with-param name="force" select="1"/>
      </xsl:call-template>
      <xsl:apply-templates select="term"/>

      <xsl:choose>
        <xsl:when test="(@id or @xml:id)">
          <xsl:text>&#160;</xsl:text>
          <xsl:call-template name="create.permalink">
            <xsl:with-param name="object" select="."/>
          </xsl:call-template>
        </xsl:when>
      </xsl:choose>
    </dt>
    <dd>
      <xsl:apply-templates select="listitem"/>
    </dd>
  </xsl:template>

  <xsl:template match="procedure">

    <!-- Preserve order of PIs and comments -->
    <xsl:variable name="preamble"
        select="*[not(self::step
                  or self::title
                  or self::titleabbrev)]
                  |comment()[not(preceding-sibling::step)]
                  |processing-instruction()[not(preceding-sibling::step)]"/>

    <div>
      <xsl:call-template name="common.html.attributes"/>
      <xsl:call-template name="id.attribute">
        <xsl:with-param name="conditional">
          <xsl:choose>
             <xsl:when test="title">0</xsl:when>
             <xsl:otherwise>1</xsl:otherwise>
          </xsl:choose>
        </xsl:with-param>
      </xsl:call-template>

      <xsl:if test="(title or info/title)">
        <xsl:call-template name="formal.object.heading"/>
      </xsl:if>
      <div class="procedure-contents">
      <xsl:apply-templates select="$preamble"/>

      <xsl:choose>
        <xsl:when test="count(step) = 1">
          <ul>
            <xsl:call-template name="generate.class.attribute"/>
            <xsl:apply-templates
              select="step
                     |comment()[preceding-sibling::step]
                     |processing-instruction()[preceding-sibling::step]"/>
          </ul>
        </xsl:when>
        <xsl:otherwise>
          <ol>
            <xsl:call-template name="generate.class.attribute"/>
            <xsl:attribute name="type">
                <xsl:value-of select="substring($procedure.step.numeration.formats,1,1)"/>
            </xsl:attribute>
            <xsl:apply-templates
              select="step
                     |comment()[preceding-sibling::step]
                     |processing-instruction()[preceding-sibling::step]"/>
          </ol>
        </xsl:otherwise>
      </xsl:choose>
      </div>
    </div>
  </xsl:template>

  <!-- Handle step performance="optional" -->

  <!-- For the common case, where there is a para as the first item within step,
  we handle this in the para template further down. For the much less common
  case of "anything else" (e.g. a list as first element), we handle this
  directly in the step template. -->
  <xsl:template match="step">
   <li>
     <xsl:call-template name="common.html.attributes"/>
     <xsl:call-template name="id.attribute"/>
     <xsl:call-template name="anchor"/>
     <xsl:if test="@performance='optional'
                   and *[1][local-name()!='para' and local-name()!='simpara']">
     <p class="step-optional">
      <xsl:call-template name="gentext">
       <xsl:with-param name="key" select="'step.optional'"/>
      </xsl:call-template>
     </p>
    </xsl:if>
    <xsl:apply-templates/>
   </li>
  </xsl:template>

  <xsl:template match="step/*[1][local-name()='para' or local-name()='simpara']">
   <xsl:call-template name="paragraph">
     <xsl:with-param name="class">
       <xsl:if test="@role and $para.propagates.style != 0">
         <xsl:value-of select="@role"/>
       </xsl:if>
     </xsl:with-param>
     <xsl:with-param name="content">
       <xsl:if test="position() = 1 and parent::listitem">
         <xsl:call-template name="anchor">
           <xsl:with-param name="node" select="parent::listitem"/>
         </xsl:call-template>
       </xsl:if>
       <xsl:call-template name="anchor"/>
       <xsl:if test="../@performance='optional'">
         <span class="step-optional">
           <xsl:call-template name="gentext">
             <xsl:with-param name="key" select="'step.optional'"/>
           </xsl:call-template>
         </span>
         <xsl:text> </xsl:text>
       </xsl:if>
       <xsl:apply-templates/>
     </xsl:with-param>
   </xsl:call-template>
  </xsl:template>

<xsl:template match="listitem/simpara" priority="10">
  <!-- Unlike the original DocBook stylesheets, if a listitem contains only a
  single simpara, we still want to output the <p> wrapper... This is essentially
  a copy of the match="para" template. -->
  <xsl:call-template name="paragraph">
    <xsl:with-param name="class">
      <xsl:if test="@role and $para.propagates.style != 0">
        <xsl:value-of select="@role"/>
      </xsl:if>
    </xsl:with-param>
    <xsl:with-param name="content">
      <xsl:if test="position() = 1 and parent::d:listitem">
        <xsl:call-template name="anchor">
          <xsl:with-param name="node" select="parent::d:listitem"/>
        </xsl:call-template>
      </xsl:if>
      <xsl:call-template name="anchor"/>
      <xsl:apply-templates/>
    </xsl:with-param>
  </xsl:call-template>
</xsl:template>

</xsl:stylesheet>
