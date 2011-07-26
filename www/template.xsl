<?xml version="1.0" encoding="UTF-8"?>

<!DOCTYPE xsl:stylesheet [
<!ENTITY bull  "&#160;">
<!ENTITY nbsp  "&#160;">
<!ENTITY ndash "&#8211;">
<!ENTITY laquo "&#171;">
<!ENTITY raquo "&#187;">
]>

<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:exslt="http://exslt.org/common"
		extension-element-prefixes="exslt">

<xsl:output method="xml"
	    version="1.0"
	    encoding="UTF-8"
	    doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
	    doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"/>

<xsl:variable name="main_width">900</xsl:variable>
<xsl:variable name="logo_height">62</xsl:variable>
<xsl:variable name="menubar_height">40</xsl:variable>
<xsl:variable name="langbar_width">117</xsl:variable>

<xsl:template match="moment">
<html style="height: 100%">

<head>
  <title>
    <xsl:text>Moment Video Server - </xsl:text>
    <xsl:choose>
      <xsl:when test="content/pagename">
        <xsl:apply-templates select="content/pagename" mode="title"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="content/title" mode="title"/>
      </xsl:otherwise>
    </xsl:choose>
  </title>
<!--  <link rel="stylesheet" type="text/css" href="moment.css"/> -->
  <link rel="icon" type="image/vnd.microsoft.icon" href="favicon.ico"/>
  <style type="text/css">
    body {
      font-size: 14px;
    }

    dd {
      margin-bottom: 1em;
    }

    img {
      border: 0;
      vertical-align: bottom;
    }

    pre {
      padding-left: 3px;
      padding-right: 3px;
      background-color: #f3f3fb;
    }

    .main_div {
      margin-left: auto;
      margin-right: auto;
      padding-left: 20px;
      padding-right: 20px;
      position: relative;
      width: <xsl:value-of select="$main_width"/>px;
      min-height: 100%;
    }

    .logo_image {
      vertical-align: bottom;
      padding: 20px;
      height: <xsl:value-of select="$logo_height"/>px;
      position: relative;
      z-index: 2;
    }

    .menubar_image {
      vertical-align: bottom;
      height: <xsl:value-of select="$menubar_height"/>px;
      position: absolute;
      top: 0;
      left: 0;
      z-index: 2;
    }

    .menu a {
      text-decoration: none;
      color: #0b0b28;
    }

    .menu a:hover {
      color: /* #525278 */ /* #33335a */ /* #252569 */ #4b4b58;
    }

    .menuitem {
      float: left;
      position: relative;
      z-index: 5;
      padding-left: 20px;
      padding-right: 20px;
      height: <xsl:value-of select="$menubar_height"/>px;
      line-height: <xsl:value-of select="$menubar_height"/>px;
      font-size: larger;
    }

    .menuarrow {
      float: left;
      position: relative;
      z-index: 5;
      display: table-cell;
      vertical-align: middle;
      margin-left: 40px;
      height: <xsl:value-of select="$menubar_height"/>px;
      line-height: <xsl:value-of select="$menubar_height"/>px;
      font-size: larger;
    }

    .langbar {
      border-left: 1px solid rgba(180, 183, 218, .2);
      float: right;
      position: relative;
      z-index: 4;
      margin-right: 8px;
      padding-left: 6px;
      display: table-cell;
      vertical-align: middle;
      height: <xsl:value-of select="$menubar_height"/>px;
      line-height: <xsl:value-of select="$menubar_height"/>px;
      text-align: center;
      width: <xsl:value-of select="$langbar_width"/>px;
/*      color: #4b4b78; */
    }

    .langbar a {
      color: #4b4b68;
    }

    .langbar a:hover {
      color: #7b7ba8;
    }

    .langbar_image_div {
      position: absolute;
      top: 0;
      right: 0;
      z-index: 5;
      height: <xsl:value-of select="$menubar_height"/>px;
      display: table-cell;
      vertical-align: middle;
    }

    .download_outer {
      padding-right: 5px;
      position: absolute; top: 181px; left: <xsl:value-of select="$main_width - 165"/>px;
    }

    .download {
      margin-top: 20px;
      margin-left: 10px;
      padding: 13px;
      padding-top: 18px;
      padding-bottom: 20px;
      background-color: /* #f0f0f0 */ /* #e9e9f7 */ /* #e8e8ff */ /* #eeeefb */ #f3f3fb;
      -moz-border-radius: 15px;
      border-radius: 15px;
      text-align: center;
      font-size: small;
    }

    .content_div {
      clear: both;
      margin-top: <xsl:value-of select="$menubar_height"/>px;
      margin-right: auto;
      padding-bottom: 140px;
      margin-left: 80px;
      width: 600px;
      min-height: 180px;
/*      border: 1px solid red; */
    }

    .content_home {
      padding-top: 40px;
/*      margin-right: 150px; */
      line-height: 1.5;
    }

    .content {
      padding-top: 40px;
/*      margin-left: 150px;
      margin-right: 150px; */
      line-height: 1.5;
    }

    .footer {
      position: absolute;
      bottom: 0;
      width: <xsl:value-of select="$main_width"/>px;
/*      border-top: 2px solid #d1d3e9; */
      border-top: 2px solid #e4e5f1;
      padding-top: 20px;
      margin-top: 60px;
      margin-bottom: 20px;
      text-align: center;
      font-size: small;
      color: /* #636363 */ #616187;
    }
  </style>
</head>

<body style="margin: 0; font-family: sans-serif; height: 100%">
<div class="main_div">

  <div>
    <img src="img/logo.png" alt="Moment Video Server" class="logo_image"/>
  </div>

  <div style="position: absolute; z-index: 1; top: 0; right: 0">
    <img src="img/slogan.png" alt="Live streaming made easy." style="vertical-align: bottom; padding-right: 40px"/>
  </div>

  <div class="menu" style="text-align: center; position: relative; z-index: 3">
    <img src="img/menubar.png" class="menubar_image"/>

    <eng><a href="./"><div class="menuitem" style="padding-left: 30px">Home</div></a></eng>
    <rus><a href="index.ru.html"><div class="menuitem" style="padding-left: 30px">Главная</div></a></rus>

    <eng><a href="quickstart.html"><div class="menuitem">Quick Start</div></a></eng>
    <rus><a href="quickstart.ru.html"><div class="menuitem">Установка</div></a></rus>

    <eng><a href="licensing.html"><div class="menuitem">Licensing</div></a></eng>
    <rus><a href="licensing.ru.html"><div class="menuitem">Лицензия</div></a></rus>

    <eng><a href="developers.html"><div class="menuitem">Developers</div></a></eng>
    <rus><a href="developers.ru.html"><div class="menuitem">Разработчикам</div></a></rus>

    <eng>
      <a href="http://momentvideo.org/wiki/">
        <div class="menuarrow">
	  <img src="img/arrow.png" style="vertical-align: middle; margin-right: 7px; width: 16px; height: 22px"/>
	  Wiki
	</div>
      </a>
    </eng>
    <rus>
      <a href="http://momentvideo.org/wiki/index.php?title=Moment:Main:Ru">
        <div class="menuarrow">
	  <img src="img/arrow.png" style="vertical-align: middle; margin-right: 7px; width: 16px; height: 22px"/>
	  Wiki
	</div>
      </a>
    </rus>

    <div class="langbar">
<!--      <div class="langbar_image_div"> -->
<!--        <img src="img/langbar.png" style="vertical-align: middle; width: 122; height: 37"/> -->
<!--        <img src="img/langbar.png" style="vertical-align: middle"/> -->
<!--      </div> -->
      <div style="position: relative; z-index: 5">
        <table border="0" cellpadding="0" cellspacing="0" style="width: 100%; padding-left: 13px; padding-right: 13px">
          <tr>
            <td>
              <xsl:element name="a">
                <xsl:attribute name="href">
                  <xsl:value-of select="$name"/><xsl:if test="$name = 'index'"><xsl:text>.en</xsl:text></xsl:if><xsl:text>.html</xsl:text>
                </xsl:attribute>
                <xsl:text>Eng</xsl:text>
              </xsl:element>
            </td>
            <td>
              <xsl:element name="a">
                <xsl:attribute name="href">
                  <xsl:value-of select="$name"/><xsl:text>.ru.html</xsl:text>
                </xsl:attribute>
                <xsl:text>Рус</xsl:text>
              </xsl:element>
            </td>
          </tr>
        </table>
      </div>
    </div>
  </div>


  <div class="content_div">
<!--    <table border="0" cellpadding="0" cellspacing="0" style="margin-left: 80px">
      <tr>
        <td style="vertical-align: top; width: 700px"> -->
          <xsl:element name="div">
            <xsl:attribute name="class">
              <xsl:choose>
                <xsl:when test="$name='index'">
                  content_home
                </xsl:when>
                <xsl:otherwise>
                  content
                </xsl:otherwise>
              </xsl:choose>
            </xsl:attribute>
            <xsl:apply-templates select="content/*"/>
          </xsl:element>
<!--        </td>
      </tr>
    </table> -->
  </div>

  <div class="download_outer">
    <div class="download">
      <a href="http://downloads.sourceforge.net/moment/moment-bin-1.0.tar.gz">
      <eng><img src="img/download.png" alt="Download Moment Video Server 1.0 for Linux" style="margin-bottom: 16px; width: 183px; height: 62px"/></eng>
      <rus><img src="img/download.ru.png" alt="Загрузить Moment Video Server 1.0 для Linux" style="margin-bottom: 16px; width: 183px; height: 62px"/></rus>
      </a>
      <br/>
      <div style="white-space: nowrap; line-height: 1.33">
        <eng>Follow <a href="quickstart.html">these quick instructions</a><br/>to get Moment VS up and running.</eng>
<!--    <rus>Следуйте <a href="quickstart.ru.html">этим простым инструкциям</a>,<br/> чтобы запустить &laquo;Moment&raquo;.</rus> -->
        <rus><a href="quickstart.ru.html">Простые инструкции</a> помогут<br/>запустить &laquo;Moment&raquo;.</rus>
      </div>
    </div>
  </div>

  <div class="footer">
    <eng>Copyright (c) 2011 Dmitry Shatrov</eng>
    <rus>(c) 2011 Дмитрий Шатров</rus>
  </div>

</div>
</body>

</html>
</xsl:template>

<xsl:template match="title">
  <div class="title" style="margin-left: 15px; padding-bottom: 5px; font-size: xx-large">
    <xsl:apply-templates select="@*|node()"/>
  </div>
</xsl:template>

<xsl:template match="title" mode="title">
  <xsl:apply-templates select="@*|node()"/>
</xsl:template>

<xsl:template match="pagename">
</xsl:template>

<xsl:template match="pagename" mode="title">
  <xsl:apply-templates select="@*|node()"/>
</xsl:template>

<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
  </xsl:copy>
</xsl:template>

</xsl:stylesheet>

