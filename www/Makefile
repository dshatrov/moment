.PHONY: all clean

all:	index.html              \
        quickstart.html         \
        licensing.html          \
        developers.html         \
        gpl.html		\
	fix

fix: index.html index.ru.html
	if head -n 1 index.html | grep '^<?xml'; then echo "cutting xml tag"; tail -n +2 index.html > index.html.new; fi
	if head -n 1 index.ru.html | grep '^<?xml'; then echo "cutting xml tag"; tail -n +2 index.ru.html > index.ru.html.new; fi
	if [ -f index.html.new ]; then mv index.html.new index.html; fi
	if [ -f index.ru.html.new ]; then mv index.ru.html.new index.ru.html; fi
	mv index.html index.en.html

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

