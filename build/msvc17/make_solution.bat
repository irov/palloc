@echo off

if ["%~1"]==[""] (
  @echo invalid arguments, please select configuration
  goto end
)

set "CONFIGURATION=%1"
set "SOLUTION_DIR=%~dp0..\..\solutions\palloc_msvc17_%CONFIGURATION%"

@mkdir %SOLUTION_DIR%

@pushd %SOLUTION_DIR%
CMake -G "Visual Studio 17 2022" -A Win32 "%CD%\..\.." -DCMAKE_BUILD_TYPE:STRING=%CONFIGURATION% -DCMAKE_CONFIGURATION_TYPES:STRING=%CONFIGURATION% -DPALLOC_TEST=OFF -DPALLOC_THREAD=ON -DPALLOC_LOCKFREE=OFF -DPALLOC_MUTEX=ON -DPALLOC_SANITIZE=ON
@popd

:end

@exit /b %errorlevel%