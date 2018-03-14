clean:
	rm -rf _minted-* *.xtr *.out *.log *.fls *.aux *.ext *.fdb_latexmk *.toc *.pyg

.PHONY: phony_explicit
phony_explicit:

%.tex: phony_explicit
	latexmk -shell-escape -pdf $*.tex
	{ 																																					\
		docnumber=$$(cat $*.tex | perl -n -e '/docnumber\{(.+)\}/ && print $$1');	\
		filename=$$(echo $${docnumber} | tr '[:upper:]' '[:lower:]').pdf; 				\
		mv $*.pdf generated/$${filename}; 																				\
	}

%.bs: phony_explicit
	curl https://api.csswg.org/bikeshed/ -F file=@$*.bs -F force=1 > generated/$*.html
