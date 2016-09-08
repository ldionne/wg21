clean:
	rm -rf build

build:
	mkdir build

%.pdf: build %.tex
	cd build && latexmk -shell-escape -pdf ../$*.tex
	mv build/$*.pdf generated
