$EXE              = $args[0]
$TARGET           = $args[1]
$DEBUG_OR_RELEASE = $args[2]

# Common arguments to static and dynamic builds
$FLAGS   = "/std:c11", "/W1", "/WX", "/Zi"
$INCLUDE = "/Ihandrail\code\", "/Icode\", "/Ihandrail\extern\", "/I$env:VULKAN_SDK\Include"

switch($DEBUG_OR_RELEASE) {
    "debug" { $FLAGS += ""; break }
    "release" { $FLAGS += "/02"; break }
}

New-Item -Path bin -ItemType Directory -Force
New-Item -Path build -ItemType Directory -Force
New-Item -Path build\asset -ItemType Directory -Force
New-Item -Path code\generated -ItemType Directory -Force

function Error-On-Exit {
    if (!$?) {
        Write-Host "Build was " -NoNewLine
        Write-Host "unsuccessful" -ForegroundColor Red
        exit 1
    }
}

function Start-Step {
    Write-Host $args[0] -NoNewLine
    Write-Host "..."
}

function End-Step {
    Error-On-Exit
    Write-Host "Done" -ForegroundColor Green
    Write-Host ""
}

function Prebuild {
    Start-Step "Prebuild"
    if(Test-Path -Path "build\prebuild.exe") {
        build\prebuild
        Error-On-Exit
    }
    End-Step
}

# Prebuild processes fonts with the vendored FreeType, but only when
# code\prebuild.c defines HANDRAIL_FONT_PROCESSING. Build it once into
# handrail\extern\freetype\out, static (with the static CRT, matching cl's
# default /MT) and without optional dependencies.
$FREETYPE_DIR = "handrail\extern\freetype"
$FREETYPE_OUT = "$FREETYPE_DIR\out"

function FreeType {
    if(Test-Path -Path "$FREETYPE_OUT\lib\freetype.lib") {
        return
    }
    Start-Step "FreeType"
    cmake -S $FREETYPE_DIR -B build\freetype `
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
        "-DCMAKE_INSTALL_PREFIX=$PWD\$FREETYPE_OUT" -DCMAKE_INSTALL_LIBDIR=lib `
        -DFT_DISABLE_ZLIB=TRUE -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_PNG=TRUE `
        -DFT_DISABLE_HARFBUZZ=TRUE -DFT_DISABLE_BROTLI=TRUE > build\freetype.log
    Error-On-Exit
    cmake --build build\freetype --config Release --parallel >> build\freetype.log
    Error-On-Exit
    cmake --install build\freetype --config Release >> build\freetype.log
    End-Step
}

function Bootstrap {
    $FREETYPE_INCLUDE = @()
    $FREETYPE_LIB     = @()
    if(Select-String -Path code\prebuild.c -Pattern "^#define HANDRAIL_FONT_PROCESSING" -Quiet) {
        FreeType
        $FREETYPE_INCLUDE = "/I$FREETYPE_OUT\include\freetype2"
        $FREETYPE_LIB     = "$FREETYPE_OUT\lib\freetype.lib"
    }

    Start-Step "Bootstrap"
    cl code\prebuild.c $INCLUDE $FREETYPE_INCLUDE $FLAGS /Fe:build\prebuild.exe /Fo:build\ /nologo /link $FREETYPE_LIB
    End-Step
}

function Static {
    prebuild

    Start-Step "Static (build)"
    cl handrail\code\main.c build\asset\pack.obj `
        /Fe:bin\$EXE.exe /Fo:build\ /Fd:bin\ `
        $INCLUDE `
        $FLAGS `
        /D"GAME_NAME=\`"$EXE\`"" /D"GAME_LIB_NAME=\`"$EXE.dll\`"" `
        /nologo `
        /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib ole32.lib avrt.lib onecore.lib xinput.lib
    End-Step
}

function Dynamic {
    # A unique PDB name per build, so a debugger holding the last one open
    # doesn't block rebuilding the library for hot reload
    $PDB_NAME = "${EXE}_$(Get-Date -Format 'yyyyMMddHHmmssfff').pdb"

    Start-Step "Game library (final)"
    cl code\game.c `
        /Fo:build\ `
        /nologo `
        $INCLUDE `
        $FLAGS /LD /link /EXPORT:game_init /EXPORT:game_update /EXPORT:game_audio_callback `
        /OUT:bin\${EXE}_tmp.dll /IMPLIB:build\$EXE.lib /PDB:bin\$PDB_NAME
    End-Step

    Move-Item -Path bin\${EXE}_tmp.dll -Destination bin\$EXE.dll -Force
}

switch($TARGET) {
    "bootstrap" { Bootstrap; break }
    "static" { Static; break }
    "dynamic" { Dynamic; break }
    "all" { Bootstrap; Static; Dynamic; break }
    "clean" { Remove-Item -Path bin\* -Recurse -Force -ErrorAction Ignore; Remove-Item -Path build\* -Recurse -Force -ErrorAction Ignore; exit 0 }
}

Write-Host "Build was " -NoNewLine
Write-Host "successful" -ForegroundColor Green
