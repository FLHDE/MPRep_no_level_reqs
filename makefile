# Makefile (VC6) for MPREP.
# Jason Hood, 25 January, 2011.  Updated 6 July, 2022.

CPPFLAGS = /nologo /W3 /GX /O2 /MD

MPRep.dll: MPRep.obj MPRep.res
	cl $(CPPFLAGS) /LD $** /link /base:0x6230000 /filealign:512

clean:
	del *.obj *.res
