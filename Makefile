clean:
	rm -rf _minted-* *.xtr *.out *.log *.fls *.aux *.ext *.fdb_latexmk

%:
	latexmk -shell-escape -pdf $*.tex
	{ 																																					\
		docnumber=$$(cat $*.tex | perl -n -e '/docnumber\{(.+)\}/ && print $$1');	\
		filename=$$(echo $${docnumber} | tr '[:upper:]' '[:lower:]').pdf; 				\
		mv $*.pdf generated/$${filename}; 																				\
	}
