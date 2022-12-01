SUBDIRS = include source

.PHONY: binary
binary: all
	make && make install prefix=$(abs_top_srcdir)/$(distdir)
	tar zcf $(abs_top_srcdir)/$(PACKAGE).tar.gz -C $(abs_top_srcdir)/$(distdir) `ls $(abs_top_srcdir)/$(distdir)`
	rm -rf $(abs_top_srcdir)/$(distdir)

