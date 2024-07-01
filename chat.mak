.AUTODEPEND

#		*Translator Definitions*
CC = F:\BORLANDC\BIN\bcc +CHAT.CFG
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
 chat.obj

#		*Explicit Rules*
chat.exe: chat.cfg $(EXE_dependencies)
  $(TLINK) /s/c/d/P-/L$(LIBPATH) @&&|
c0s.obj+
chat.obj
chat,chat
cs.lib
|


#		*Individual File Dependencies*
chat.obj: chat.cfg chat.c 

#		*Compiler Configuration File*
chat.cfg: chat.mak
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
| chat.cfg


