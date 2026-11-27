@echo off
setlocal
cd /d "%~dp0"
echo === Mateo Harmonizer - budowanie VST3 64-bit ===
where cmake >nul 2>nul || (echo Brak CMake. Zainstaluj Visual Studio 2022 z "Desktop development with C++". & pause & exit /b 1)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :err
cmake --build build --config Release --target MateoHarmonizer_VST3 MateoHarmonizer_Standalone --parallel || goto :err
echo.
echo Gotowe. Wtyczka:
echo   build\MateoHarmonizer_artefacts\Release\VST3\Mateo Harmonizer.vst3
echo Aby zainstalowac, uruchom install.bat jako administrator.
pause
exit /b 0
:err
echo BLAD budowania.
pause
exit /b 1
