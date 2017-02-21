RELEASE=$(shell git describe --always --dirty)

all : wrpc.pdf



.PHONY : all clean

wrpc.pdf : wrpc.tex
	echo $(RELEASE) > version.tex
	pdflatex --enable-write18 $^
	pdflatex --enable-write18 $^

clean :
	rm -f *.eps *.pdf *.dat *.log *.out *.aux *.dvi *.ps *.toc version.tex