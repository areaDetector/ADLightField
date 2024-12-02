setlocal
start medm -x -macro "P=13LF2:, R=cam1:" LightField.adl
call dllPath.bat
set "PATH=%LIGHTFIELD_ROOT%;%PATH%"
..\..\bin\windows-x64-dynamic\LightFieldApp st.cmd

