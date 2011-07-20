.PHONY: all clean

all:	index.html              \
        quickstart.html         \
        licensing.html          \
        developers.html         \
        gpl.html

%.html: %.xml template.xsl postprocess.xsl entities.dtd
	xsltproc --nonet -o "$*.tmp.xml" --stringparam name "$*" --stringparam suffix "" template.xsl "$<"
	xsltproc --nonet -o "$*.html" --stringparam lang eng postprocess.xsl "$*.tmp.xml"
	rm -f "$*.tmp.xml"
	xsltproc --nonet -o "$*.tmp.xml" --stringparam name "$*" --stringparam suffix ".ru" template.xsl "$<"
	xsltproc --nonet -o "$*.ru.html" --stringparam lang rus postprocess.xsl "$*.tmp.xml"
	rm -f "$*.tmp.xml"

clean:
	rm -f *.tmp.xml
	rm -f *.html

