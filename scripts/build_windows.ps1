$EXE              = $args[0]
$TARGET           = $args[1]
$DEBUG_OR_RELEASE = $args[2]

# Common arguments to static and dynamic builds
$FLAGS   = "/std:c11", "/W1", "/WX", "/Zi"
$INCLUDE = "/Ihandrail\code\", "/Icode\", "/Ihandrail\extern\include\", "/Ihandrail\extern\include\VK\"

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
        if(!$?) {
			Error-On-Exit
		}
	}
    End-Step
}

function Bootstrap {
    Start-Step "Bootstrap"
	cl code\prebuild.c $INCLUDE $FLAGS /Fe:build\prebuild.exe /Fo:build\ /nologo
    End-Step
}

function Static {
    prebuild

    Start-Step "Static (asset resource)"
	#NOW: generate or link or something asset pack
    End-Step

    Start-Step "Static (build)"
	# NOW: add asset pack
    cl handrail\code\main.c handrail\extern\include\GL\gl3w.c `
        /Fe:bin\game.exe /Fo:build\ /Fd:bin\ `
        $INCLUDE `
        $FLAGS `
		/D'GAME_NAME=\"game\"' /D'GAME_LIB_NAME=\"game.so\"' `
		/nologo `
		/link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib ole32.lib uuid.lib mmdevapi.lib opengl32.lib handrail\extern\libs\vulkan-1.lib 
    End-Step
}

function Dynamic {
    Start-Step "Game library (final)"
	cl code\game.c `
		/Fo:build\ `
		/nologo `
        $INCLUDE `
	    $FLAGS /LD /link /EXPORT:game_init /EXPORT:game_update /EXPORT:game_audio_callback `
		/OUT:bin\game_tmp.dll /IMPLIB:build\game.lib
    End-Step

    Move-Item -Path bin\game_tmp.dll -Destination bin\game.dll -Force
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
