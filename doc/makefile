
main.pdf: main.tex refs.bib
	@rm -rf build/
	mkdir build
	cp $^ build/
	cd build && \
		xelatex main && \
		bibtex main && \
		xelatex main && \
		xelatex main
	mv build/main.pdf main.pdf
	rm -r build/
