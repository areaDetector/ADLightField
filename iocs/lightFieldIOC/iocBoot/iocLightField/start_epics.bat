setlocal
start medm -x -macro "P=13LF1:, R=cam1:" LightField.adl
call dllPath.bat
set "PATH=%LIGHTFIELD_ROOT%;%PATH%"
..\..\bin\windows-x64\LightFieldApp st.cmd
pause

