PFA = xccmi10.pfa xccsy10.pfa ccr10.pfa xccam10.pfa 
OTF = xccmi10.otf xccsy10.otf ccr10.otf xccam10.otf 

xccl.otf: merge.py $(OTF)
	fontforge -script merge.py
xccmi10.pfa: itall.mf

.SUFFIXES: .otf .pfa .mf
.PHONY: clean

.mf.otf: mftrace
	python2 mftrace -f otf --simplify $(<)

.mf.pfa: mftrace
	python2 mftrace $(<)

clean:
	rm -f xccl.otf $(PFA) $(OTF)
