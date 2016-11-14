clean:
	rm -rf _minted-* *.xtr *.out *.log *.fls *.aux *.ext *.fdb_latexmk

%.pdf: %.tex
	latexmk -shell-escape -pdf $*.tex
	mv $*.pdf generated
