all: slider.html.h admin.html.h

# WOW, xxd does not produce \0 terminated strings! We fix this with a hacky sed call.
%.html.h: %.html
	xxd -i $*.html | sed 's@^};@,0};@' > $@

clean: 
	rm -f *.html.h
