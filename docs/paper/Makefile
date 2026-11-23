# Build the paper draft. Needs Python 3 (standard library) and tectonic (brew install tectonic),
# which fetches the TeX packages it needs on first use. `make ENGINE=latexmk` uses latexmk with a TeX Live install instead.
ENGINE ?= tectonic

all: kolmo.pdf

# always regenerate (under a second): a data file removed must also remove its table
generated/numbers.tex: FORCE
	python3 figures/make_figures.py

kolmo.pdf: kolmo.tex refs.bib generated/numbers.tex
ifeq ($(ENGINE),tectonic)
	tectonic kolmo.tex
else
	latexmk -pdf kolmo.tex
endif

clean:
	rm -rf generated kolmo.pdf kolmo.aux kolmo.bbl kolmo.blg kolmo.log kolmo.out kolmo.fdb_latexmk kolmo.fls

.PHONY: all clean FORCE
FORCE:
