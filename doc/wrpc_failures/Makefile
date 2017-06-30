all : wrpc_failures.pdf

.PHONY : all clean

RELEASE = $(shell git describe --always --dirty)

wrpc_failures.pdf : wrpc_failures.tex fail.tex intro.tex snmp_objects.tex snmp_exports.tex
	@echo '\\newcommand{\\gitrevinfo}{'$(RELEASE)'}' > revinfo.tex
	pdflatex wrpc_failures.tex
	pdflatex wrpc_failures.tex
# To speed up generation of document for development, please comment out:
#   % print alphabetical list
#   \printnoidxglossary[type=snmp_all,style=tree,sort=letter]
# from doc/wrpc_failures/snmp_exports.tex file.

clean :
	rm -f *.eps  *.dat *.log *.out *.aux *.dvi *.ps *.toc *.pdf revinfo.tex

