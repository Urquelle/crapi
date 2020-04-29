@echo off

set compiler_flags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4214 -wd4101 -wd4189 -wd4505 -wd4127 -wd4706 -wd4702 -wd4204 -FC -Z7 -I%PROJECT_PATH%/src -I"D:\Dev\mysql-8.0.19-winx64\include"
set linker_flags= -incremental:no -opt:ref -libpath:"D:\Dev\mysql-8.0.19-winx64\lib" Ws2_32.lib Secur32.lib libmysql.lib httpapi.lib

IF NOT exist %BUILD_PATH% ( mkdir %BUILD_PATH% )
pushd %BUILD_PATH%

cl %compiler_flags% %PROJECT_PATH%\src\main.c -Fecrapi.exe /link %linker_flags%

popd
