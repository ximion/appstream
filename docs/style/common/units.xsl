<?xml version="1.0" encoding="UTF-8"?>
<!--
    Purpose:
     Different templates dealing with numbers and units

    Named Templates:
    * get.value.from.unit(string)
      Returns the number of a string without the unit; for example,
      if a string contains "40mm", using this template will return 40.

    * get.unit.from.unit(string)
      Returns the unit of the string; for example, if a string contains
      "40mm", using this template will return "mm".

    Author:     Thomas Schraitle <toms@opensuse.org>,
                Stefan Knorr <sknorr@suse.de>
    Copyright:  2013

-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:exsl="http://exslt.org/common"
  exclude-result-prefixes="exsl">
  
  <xsl:template name="get.value.from.unit">
    <xsl:param name="string"/>
    <xsl:variable name="lasttwo" 
                  select="substring($string, string-length($string)-1, 2)"/>

    <!--
    <xsl:message>get.numbers.from.unit:
    string="<xsl:value-of select="$string"/>"
    lasttwo="<xsl:value-of select="$lasttwo"/>"
    </xsl:message>
    -->

    <xsl:choose>
      <!-- These are the possible XSL-FO units -->
      <xsl:when test="$lasttwo = 'cm' or 
                      $lasttwo = 'em' or
                      $lasttwo = 'in' or
                      $lasttwo = 'mm' or
                      $lasttwo = 'pc' or
                      $lasttwo = 'pt' or
                      $lasttwo = 'px'">
        <xsl:value-of select="substring-before($string, $lasttwo)"/>
      </xsl:when>
      <xsl:when test="contains($string, '%')">
        <xsl:value-of select="substring-before($string, '%')"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message>Unknown unit in <xsl:value-of select="$string"/></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  
  <xsl:template name="get.unit.from.unit">
    <xsl:param name="string"/>
    <xsl:variable name="lasttwo" 
                  select="substring($string, string-length($string)-1, 2)"/>
    <xsl:choose>
      <!-- These are the possible XSL-FO units -->
      <xsl:when test="$lasttwo = 'cm' or 
                      $lasttwo = 'em' or
                      $lasttwo = 'in' or
                      $lasttwo = 'mm' or
                      $lasttwo = 'pc' or
                      $lasttwo = 'pt' or
                      $lasttwo = 'px'">
        <xsl:value-of select="$lasttwo"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message>Unknown unit in <xsl:value-of select="$string"/></xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
