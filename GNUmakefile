all: build always
	cd build; make

build:
	mkdir build;
	cd build; ../configure --prefix=/data/usr --enable-debug

.PHONY: always
always:
	cd build; make

%:
	cd build; make $@
