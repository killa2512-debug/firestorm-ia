; ══════════════════════════════════════════════════════════════
;  FirestormIA — Instalador Windows
;  Generado con NSIS (Nullsoft Scriptable Install System)
; ══════════════════════════════════════════════════════════════

Unicode True

!define APP_NAME      "FirestormIA"
!define APP_VERSION   "7.2.3.80036-IA14"
!define APP_PUBLISHER "annethepassion"
!define APP_URL       "https://www.firestormviewer.org"
!define APP_EXE       "Firestorm-Releasex64.exe"
!define INSTALL_DIR   "$LOCALAPPDATA\FirestormIA"
!define UNINST_KEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\FirestormIA"

SetCompressor /SOLID lzma

Name              "${APP_NAME}"
OutFile           "FirestormIA-Setup.exe"
InstallDir        "${INSTALL_DIR}"
InstallDirRegKey  HKCU "${UNINST_KEY}" "InstallLocation"
RequestExecutionLevel user
BrandingText      "FirestormIA — annethepassion"

; ── Includes ──────────────────────────────────────────────────
!include "MUI2.nsh"
!include "LogicLib.nsh"

; ── Apariencia ────────────────────────────────────────────────
!define MUI_ABORTWARNING
!define MUI_ICON                        "assets\firestorm.ico"
!define MUI_UNICON                      "assets\firestorm.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP    "assets\welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP          "assets\header.bmp"
!define MUI_HEADERIMAGE_RIGHT

!define MUI_WELCOMEPAGE_TITLE           "Bienvenido a FirestormIA"
!define MUI_WELCOMEPAGE_TEXT            "Este instalador va a configurar FirestormIA en tu computadora.$\r$\n$\r$\nFirestormIA es el visor de Second Life con traducción automática por inteligencia artificial.$\r$\n$\r$\nPodés chatear con personas de cualquier idioma — todo se traduce en tiempo real.$\r$\n$\r$\nHacé clic en Siguiente para continuar."

!define MUI_FINISHPAGE_TITLE            "¡Instalación completada!"
!define MUI_FINISHPAGE_TEXT             "FirestormIA fue instalado correctamente.$\r$\n$\r$\nUsá el ícono del escritorio para abrirlo.$\r$\n$\r$\nEn el primer inicio te va a pedir tu clave gratuita de Groq para activar la traducción automática."
!define MUI_FINISHPAGE_LINK             "Sitio web de Firestorm"
!define MUI_FINISHPAGE_LINK_LOCATION    "${APP_URL}"

; ── Páginas del instalador ────────────────────────────────────
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE           "assets\LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Spanish"

; ══════════════════════════════════════════════════════════════
;  INSTALACIÓN
; ══════════════════════════════════════════════════════════════
Section "FirestormIA" SecMain

    SectionIn RO

    SetOutPath "$INSTDIR"
    SetOverwrite on

    DetailPrint "Instalando Firestorm..."
    File /r "FirestormIA\*.*"

    DetailPrint "Instalando motor de traducción..."
    File "groq-translate.py"
    File "setup.py"
    File "lanzar.bat"
    File "lanzar.vbs"

    ; ── Python (si no está instalado) ─────────────────────────
    DetailPrint "Verificando Python..."
    nsExec::ExecToStack 'python --version'
    Pop $0
    ${If} $0 != 0
        DetailPrint "Descargando Python 3.11..."
        NSISdl::download "https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe" \
            "$TEMP\python-installer.exe"
        Pop $0
        ${If} $0 == "success"
            DetailPrint "Instalando Python..."
            ExecWait '"$TEMP\python-installer.exe" /quiet InstallAllUsers=0 PrependPath=1'
            Delete "$TEMP\python-installer.exe"
            DetailPrint "Python instalado."
        ${Else}
            DetailPrint "Python no se pudo descargar. Instalalo manualmente desde python.org"
        ${EndIf}
    ${Else}
        DetailPrint "Python OK."
    ${EndIf}

    ; ── Acceso directo escritorio ──────────────────────────────
    DetailPrint "Creando acceso directo..."
    CreateShortcut "$DESKTOP\FirestormIA.lnk" \
        "$SYSDIR\wscript.exe" \
        "$\"$INSTDIR\lanzar.vbs$\"" \
        "$INSTDIR\${APP_EXE}" 0

    ; ── Menú inicio ───────────────────────────────────────────
    CreateDirectory "$SMPROGRAMS\FirestormIA"
    CreateShortcut  "$SMPROGRAMS\FirestormIA\FirestormIA.lnk" \
        "$SYSDIR\wscript.exe" \
        "$\"$INSTDIR\lanzar.vbs$\"" \
        "$INSTDIR\${APP_EXE}" 0
    CreateShortcut  "$SMPROGRAMS\FirestormIA\Desinstalar.lnk" \
        "$INSTDIR\uninstall.exe"

    ; ── Registro ──────────────────────────────────────────────
    WriteRegStr   HKCU "${UNINST_KEY}" "DisplayName"      "${APP_NAME}"
    WriteRegStr   HKCU "${UNINST_KEY}" "DisplayVersion"   "${APP_VERSION}"
    WriteRegStr   HKCU "${UNINST_KEY}" "Publisher"        "${APP_PUBLISHER}"
    WriteRegStr   HKCU "${UNINST_KEY}" "InstallLocation"  "$INSTDIR"
    WriteRegStr   HKCU "${UNINST_KEY}" "UninstallString"  "$INSTDIR\uninstall.exe"
    WriteRegStr   HKCU "${UNINST_KEY}" "DisplayIcon"      "$INSTDIR\${APP_EXE}"
    WriteRegStr   HKCU "${UNINST_KEY}" "URLInfoAbout"     "${APP_URL}"
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify"         1
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair"         1

    WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

; ══════════════════════════════════════════════════════════════
;  DESINSTALACIÓN
; ══════════════════════════════════════════════════════════════
Section "Uninstall"

    Delete "$DESKTOP\FirestormIA.lnk"
    Delete "$SMPROGRAMS\FirestormIA\FirestormIA.lnk"
    Delete "$SMPROGRAMS\FirestormIA\Desinstalar.lnk"
    RMDir  "$SMPROGRAMS\FirestormIA"

    RMDir /r "$INSTDIR"

    DeleteRegKey HKCU "${UNINST_KEY}"

    MessageBox MB_OK "FirestormIA fue desinstalado correctamente."

SectionEnd
