FILE=wrpc_hdl

all : $(FILE).pdf

.PHONY : all clean

$(FILE).pdf : $(FILE).tex *.tex references.bib
	pdflatex $(FILE)
	pdflatex $(FILE)
	bibtex $(FILE)
	pdflatex $(FILE)
	pdflatex $(FILE)

clean :
	rm -f *.eps *.pdf *.dat *.log *.out *.aux *.dvi *.ps *.nav *.snm *.toc *.blg *.bbl

