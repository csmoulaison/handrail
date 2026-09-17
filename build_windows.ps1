$EXE              = $args[0]
$TARGET           = $args[1]
$DEBUG_OR_RELEASE = $args[2]

# Common arguments to static and dynamic builds
$FLAGS   = "/std:c11", "/W1", "/WX"
$INCLUDE = "/I.\code", "/I.\extern"

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
    if(Test-Path -Path "build\build.exe") {
        build\build
        if(!$?) {
			Error-On-Exit
		}
	}
    End-Step
}

function Bootstrap {
    Start-Step "Bootstrap"
	cl code\build.c $FLAGS /Fe:build\build.exe /Fo:build\ /nologo
    End-Step
}

function Static {
    prebuild

    Start-Step "Static (asset resource)"
	#NOW: generate or link or something asset pack
    End-Step

    Start-Step "Static (build)"
	# NOW: add asset pack
    cl code\csm_core\main.c extern\GL\gl3w.c `
        /Fe:bin\template.exe /Fo:build\ `
        $INCLUDE `
        $FLAGS `
		/DGAME_NAME="template" /DGAME_LIB_NAME="template.so" `
		/nologo `
		/link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib opengl32.lib
    End-Step
}

function Dynamic {
    #Start-Step "Game library (intermediate)"
    #cl code\game.c `
    #    /Fo:build\${EXE}.o `
    #    $INCLUDE `
    #    $FLAGS -c -fPIC
    #End-Step

    Start-Step "Game library (final)"
    #cl build\${EXE}.o `
    #    /Fe:bin\${EXE}_tmp.so `
    #    $INCLUDE `
    #    $FLAGS -shared
	cl code\game.c `
		/Fo:build\ `
		/nologo `
        $INCLUDE `
	    $FLAGS /LD /link /EXPORT:game_init /EXPORT:game_update /EXPORT:game_audio_callback `
		/OUT:bin\template_tmp.dll /IMPLIB:build\template.lib
    End-Step

    Move-Item -Path bin\template_tmp.dll -Destination bin\template.dll -Force
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
