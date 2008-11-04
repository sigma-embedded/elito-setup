TAR =	tar
TAR_FLAGS = --owner root --group root --mode go-w,a+rX

include ../common/Makefile.common

dist:
	tar cjf $(NAME)-$(VERSION).tar.bz2 $(DIST_FILES) $(TAR_FLAGS) --transform 's!^!$(NAME)-$(VERSION)/!'

clean:	clean-dist

clean-dist:
	rm -f $(NAME)-*.tar.bz2
