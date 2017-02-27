all : wrpc_hdl.pdf

.PHONY : all clean

wrpc_hdl.pdf : wrpc_hdl.tex
	pdflatex $^
	pdflatex $^

clean :
	rm -f *.eps *.pdf *.dat *.log *.out *.aux *.dvi *.ps *.nav *.snm *.toc

