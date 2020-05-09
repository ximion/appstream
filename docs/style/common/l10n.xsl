<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE xsl:stylesheet >
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" >


<xsl:template name="gentext.guimenu.startquote">
   <xsl:call-template name="gentext.dingbat">
    <xsl:with-param name="dingbat">guimenustartquote</xsl:with-param>
  </xsl:call-template>
</xsl:template>


<xsl:template name="gentext.guimenu.endquote">
   <xsl:call-template name="gentext.dingbat">
    <xsl:with-param name="dingbat">guimenuendquote</xsl:with-param>
  </xsl:call-template>
</xsl:template>
    

</xsl:stylesheet>

