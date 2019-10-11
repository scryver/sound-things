@echo off

set opts=-FC -GR- -EHa- -nologo -Zi
set code=%cd%
pushd gebouw
cl %opts% %code%\flac_decode.cpp -Feflacdecode
popd
