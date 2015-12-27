all: slider.html.h admin.html.h

%.html.h: %.html
	xxd -i $*.html > $@
