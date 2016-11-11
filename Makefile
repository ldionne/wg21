%.pdf: %.tex
	latexmk -shell-escape -pdf $*.tex
	mv $*.pdf generated
