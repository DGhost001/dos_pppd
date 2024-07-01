.AUTODEPEND

#		*Translator Definitions*
CC = F:\BORLANDC\BIN\bcc +CHAT0.CFG
TASM = F:\BORLANDC\BIN\TASM
TLIB = F:\BORLANDC\BIN\tlib
TLINK = F:\BORLANDC\BIN\tlink
LIBPATH = F:\BORLANDC\LIB
INCLUDEPATH = F:\BORLANDC\INCLUDE


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 chatlowl.obj \
 chat8250.obj \
 chat0.obj

#		*Explicit Rules*
chat0.exe: chat0.cfg $(EXE_dependencies)
  $(TLINK) /s/c/d/P-/L$(LIBPATH) @&&|
c0s.obj+
chatlowl.obj+
chat8250.obj+
chat0.obj
chat0,chat0
cs.lib
|


#		*Individual File Dependencies*
chatlowl.obj: chat0.cfg chatlowl.asm 
	$(TASM) /MX /ZI /O CHATLOWL.ASM,CHATLOWL.OBJ

chat8250.obj: chat0.cfg chat8250.c 

chat0.obj: chat0.cfg chat0.c 

#		*Compiler Configuration File*
chat0.cfg: chat0.mak
  copy &&|
-f-
-ff-
-C
-w+
-j0
-g0
-O
-Oe
-Ob
-Z
-k-
-d
-vi-
-I$(INCLUDEPATH)
-L$(LIBPATH)
-DIOMACROS=1
-DDEBUGTTY=1
| chat0.cfg


