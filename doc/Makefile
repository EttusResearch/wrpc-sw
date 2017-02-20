all : wrpc.pdf

.PHONY : all clean

wrpc.pdf : wrpc.tex
	pdflatex $^
	pdflatex $^

clean :
	rm -f *.eps *.pdf *.dat *.log *.out *.aux *.dvi *.ps *.toc