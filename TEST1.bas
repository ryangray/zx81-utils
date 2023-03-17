   1 REM Z80 CODE:\{1}\{40}\{65}\{201}
   2 REM REM CODES: 1,40,65,201
   3 LET A$="CODES IN \{213}"
   4 LET B$="\{234}IN A STRING"
  10 REM BLOCK GRAPHICS
  11 REM 1 2 3 4 5 6 7 8
  12 REM \'  \ ' \ . \.  \:  \.. \'' \ :
  14 REM Q W E R T Y
  15 REM \.: \:. \:' \': \.' \'.
  17 REM A S D F G H
  18 REM \!: \!' \!. \|' \|. \|:
  20 REM SPECIAL
  21 REM ` QUOTE IMAGE
  22 LET B$="HE SAID, `STOP`."
  23 REM \\ POUND STERLING
  24 LET Y=2**3
  25 LET C$="\{216}STARS\{216}"
  30 REM INVERSES
  31 REM abcdefghijklm
  32 REM nopqrstuvwxyz
  33 REM \0\1\2\3\4\5\6\7\8\9
  34 REM \$\(\)\"\-\+\=\:\;\?\/\*\<\>\.\,
  35 REM \@ INVERSE POUND
  40 let REF=65*256+40
  50 print "MC RESULT SHOULD BE "; REF;"."
  60 let MC= usr 16523
  70 print "THE MC RESULT IS ";MC;"."
  80 print "ESCAPE CODE TEST ";
  90 if MC<>REF then print "FAIL."
 100 if MC=REF then print "PASS."
