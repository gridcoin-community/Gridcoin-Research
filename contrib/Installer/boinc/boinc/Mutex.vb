Imports System.IO
Imports System.Collections.Generic
Imports System.IO.MemoryMappedFiles
Imports System.Threading
Imports System.Net
Public Class SharedMemory
    'This was originally written with .NET's MemoryMapped File Service and a mutex, and it caused a few problems, so I updated it with this method until the need arises to move this to shared memory.
    Private _name As String
    Private _mmfpath As String
    Public Function GetGridPath(ByVal sType As String) As String
        Dim sTemp As String
        sTemp = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch\" + sType
        If System.IO.Directory.Exists(sTemp) = False Then
            Try
                System.IO.Directory.CreateDirectory(sTemp)
            Catch ex As Exception
            End Try
        End If
        Return sTemp
    End Function
    Public Sub New(name As String)
        _name = name
        _mmfpath = GetGridPath("database") + "\" + _name
        Write("")
    End Sub
    Public Sub Write(data As String)
        Try
            Dim fs As New StreamWriter(_mmfpath)
            fs.Write(data)
            fs.Close()
        Catch ex As Exception
        End Try
    End Sub
    Public Function Read() As String
        Try
            Dim fs As New StreamReader(_mmfpath)
            Dim sOut As String
            sOut = fs.ReadToEnd()
            fs.Close()

            Return sOut
        Catch ex As Exception
            Return ""
        End Try
        Return ""

    End Function
End Class
