
main.pdf: main.tex refs.bib
	mkdir build
	cp $^ build/
	cd build && \
		xelatex main && \
		bibtex main && \
		xelatex main && \
		xelatex main
	mv build/main.pdf main.pdf
	rm -r build/
