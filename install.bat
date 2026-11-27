@echo off
cd /d "%~dp0"
set SRC=build\MateoHarmonizer_artefacts\Release\VST3\Mateo Harmonizer.vst3
if not exist "%SRC%" (echo Najpierw uruchom build.bat & pause & exit /b 1)
xcopy "%SRC%" "%CommonProgramFiles%\VST3\Mateo Harmonizer.vst3\" /E /I /Y || (echo Uruchom jako administrator! & pause & exit /b 1)
echo Zainstalowano w %CommonProgramFiles%\VST3
pause
