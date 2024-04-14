   1 rem \{1}\{40}\{96}\{201}
  10 rem TEST BASIC PROGRAM FOR P2SPECTRUM
  11 rem THE CALL TO THE GREY UDG LOADER SHOULD BE INSERTED AT LINE 2 ABOVE
  20 rem THE PLOT 4X SUBROUTINE SHOULD APPEAR AT LINE 171
  21 rem THE UNPLOT 4X ROUTINE SHOULD APPEAR AT LINE 172
  30 cls
  40 fast
  50 print at 12,0;"a b c d e f"
  60 scroll
  70 print at 12,0;"\!: \!. \!' \|: \|. \|'"
  71 rem UDG LOADER SHOULD BE INSERTED AT LINE 173
  80 poke 16516,65
  81 let X=peek 16514
  90 let C$=chr$ 12
 100 let K$=inkey$
 110 let C=code C$
 120 let A$="QUOTE IMAGE: `"
 130 let Y=3**2
 140 plot 32,11
 141 plot 33,10
 142 unplot 32,11
 150 let R=USR 16514
 160 print "RESULT="; R
 170 stop
 200 save "TEST\2"
 210 run
