Dim fso, dir, sh
Set fso = CreateObject("Scripting.FileSystemObject")
Set sh  = CreateObject("WScript.Shell")
dir = fso.GetParentFolderName(WScript.ScriptFullName)

If Not fso.FileExists(dir & "\groq_config.ini") Then
    sh.Run "cmd /c """ & dir & "\lanzar.bat""", 1, True
Else
    sh.Run "cmd /c """ & dir & "\lanzar.bat""", 0, False
End If
