CC = pdflatex
SRC = informe
DEP = $(SRC:=.tex)
PDF = $(SRC:=.pdf)
TOC = $(SRC:=.toc)

.phony: all clean

all: $(PDF)

FORCE:

$(PDF): $(DEP) $(TOC)
	$(CC) $<

%.toc: $(DEP) FORCE
	$(CC) $<

clean:
	rm -f $(PDF) *.aux *.log *.toc *.out


