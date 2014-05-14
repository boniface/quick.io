# Makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
BUILDDIR      = _build
ALLSPHINXOPTS = -d $(BUILDDIR)/doctrees $(SPHINXOPTS) .

.PHONY: help clean html man

all: man html

man:
	$(SPHINXBUILD) -b man $(ALLSPHINXOPTS) .
	@echo
	@echo "Build finished."

html:
	$(SPHINXBUILD) -b html $(ALLSPHINXOPTS) ../gh-pages
	@echo
	@echo "Build finished. The HTML pages are in ../gh-pages."

clean:
	rm -f *.1 *.8
	rm -rf $(BUILDDIR)/*
