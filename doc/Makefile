###########################################################
#
# Makefile for LaTeX docs
#
# $Id: 
#

.SUFFIXES:
.SUFFIXES: .ps .dvi .tex

TEXINPUTS := .:${TEXINPUTS}
BIBINPUTS := .:${TEXINPUTS}

# Figures
PRNS =			$(wildcard *.prn)
PDFS :=			$(addsuffix .pdf, $(basename $(PRNS)))
VISIOEPSES :=		$(addsuffix .eps, $(basename $(PRNS)))
SHRINK =		shrink.sh


all: eps dvi ps pdf # res
eps: fig obj ${VISIOEPSES}

PAPER=manual

ps:  $(PAPER).ps
pdf: $(PAPER).pdf
dvi: $(PAPER).dvi

obj: $(patsubst %.obj, %.eps, $(wildcard *.obj))
fig: $(patsubst %.fig, %.eps, $(wildcard *.fig))

res:
	make -C results
	make -C figs

clean:
	$(RM) *.aux *.log *.blg *~ *.bbl #*\# *.toc *.idx
	$(RM) $(patsubst %.tex, %.ps, $(wildcard *.tex))
	$(RM) $(patsubst %.tex, %.dvi, $(wildcard *.tex))
	$(RM) $(patsubst %.tex, %.pdf, $(wildcard *.tex))
	$(RM) venn_diagram.eps multi_variable_bug.eps thread_counter_example.eps

distclean: clean
	$(RM) $(patsubst %.fig, %.eps, $(wildcard *.fig))
	$(RM) $(patsubst %.obj, %.eps, $(wildcard *.obj))

tarball: $(PAPER).tgz

$(PAPER).tgz: pdf
	tar czvf $@ ./$(PAPER).tex ./$(PAPER).bib ./Makefile ./$(PAPER).pdf


# only need -Ppdf if using CM fonts
%.pdf: %.dvi $(wildcard *.bib)
	#dvips -Ppdf -Pcmz -G0 -t letter -o $*.tmp.ps $<
	dvips -Ppdf -Pcmz -Pamz -t letter -D 600 -G0 -o $*.tmp.ps $<
	ps2pdf14 $*.tmp.ps $*.pdf
	$(RM) $*.tmp.ps $*.dvi *.aux *.log *.blg *.toc #*.bbl 

%.eps:	%.prn Figures
	$(SHRINK) $< Figures

%.dvi: ${VISIOEPSES} $(wildcard *.tex) $(wildcard *.bib) 
	latex $*.tex
	bibtex $*
	latex $*.tex
	if [ -e $*.toc ] ; then latex $* ; fi
	if [ -e $*.bbl ] ; then latex $* ; fi
	if egrep Rerun $*.log ; then latex $* ; fi
	if egrep Rerun $*.log ; then latex $* ; fi
	if egrep Rerun $*.log ; then latex $* ; fi
	#$(RM) *.aux *.log *.bbl *.blg *.toc

%.ps: %.dvi
	#dvips -Ppdf -Pcmz -G0 -tletter -o $@ $<
	dvips -Pcmz -G1 -tletter -o $@ $<

%.eps: %.fig
	fig2eps $<

%.ps.gz: %.ps
	gzip -v9 $<

%.eps: %.obj
	tgif -print -eps $*

%.eps: %.fig
	fig2dev -L eps $< $@
