pcc debug8
if errorlevel 1 goto oops
pcc d8_dis
if errorlevel 1 goto oops
pcc d8_asm
if errorlevel 1 goto oops
pcc d8sprite
if errorlevel 1 goto oops
pcc d8_help
pccl debug8 d8_dis d8_asm d8sprite d8_help
if errorlevel 1 goto oops
goto exit
:oops
echo Error in compile/link
:exit
