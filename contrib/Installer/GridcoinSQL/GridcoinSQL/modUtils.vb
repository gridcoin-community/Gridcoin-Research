Imports System.Text
Imports System.IO

Public Module utils
    Public bSqlHouseCleaningComplete As Boolean = False
    Public mlSqlBestBlock As Long = 0
    Public nBestBlock As Long = 0
    Public mWebServer As WebServer

    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = GetGridFolder() + "debug2.log"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(Now) + ", " + sData)
            sw.Close()
        Catch ex As Exception
        End Try

    End Sub

    Public Function GetGridFolder() As String
        Dim sTemp As String
        sTemp = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch\"
        Return sTemp
    End Function
    Public Function ConfigPath() As String
        Dim sFolder As String
        sFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch"
        Dim sPath As String
        sPath = sFolder + "\gridcoinresearch.conf"
        Return sPath
    End Function

    Public Function KeyValue(ByVal sKey As String) As String
        Try
            Dim sPath As String = ConfigPath()
            Dim sr As New StreamReader(sPath)
            Dim sRow As String
            Dim vRow() As String
            Do While sr.EndOfStream = False
                sRow = sr.ReadLine
                vRow = Split(sRow, "=")
                If LCase(vRow(0)) = LCase(sKey) Then
                    sr.Close()
                    Return Trim(vRow(1) & "")
                End If
            Loop
            sr.Close()
            Return ""

        Catch ex As Exception
            Return ""
        End Try
    End Function
    Public Function UpdateKey(ByVal sKey As String, ByVal sValue As String)
        Try
            Dim sInPath As String = ConfigPath()
            Dim sOutPath As String = ConfigPath() + ".bak"

            Dim bFound As Boolean

            Dim sr As New StreamReader(sInPath)
            Dim sw As New StreamWriter(sOutPath, False)

            Dim sRow As String
            Dim vRow() As String
            Do While sr.EndOfStream = False
                sRow = sr.ReadLine
                vRow = Split(sRow, "=")
                If UBound(vRow) > 0 Then
                    If LCase(vRow(0)) = LCase(sKey) Then
                        sw.WriteLine(sKey + "=" + sValue)
                        bFound = True
                    Else
                        sw.WriteLine(sRow)
                    End If
                End If

            Loop
            If bFound = False Then
                sw.WriteLine(sKey + "=" + sValue)
            End If
            sr.Close()
            sw.Close()
            Kill(sInPath)
            FileCopy(sOutPath, sInPath)
            Kill(sOutPath)
        Catch ex As Exception
            Return ""
        End Try
    End Function
    
    Public Function Base64Hash(ByVal clearText As String) As String
        Return Base64Hash(clearText, String.Empty)
    End Function

    Public Function Base64Hash(ByVal clearText As String, ByVal key As String) As String
        Dim hashedBytes As Byte() = computeHash(clearText, key)
        Return Convert.ToBase64String(hashedBytes)
    End Function

    Public Function HexHash(ByVal clearText As String) As String
        Return HexHash(clearText, String.Empty)
    End Function

    Public Function HexHash(ByVal clearText As String, ByVal key As String) As String
        Dim hashedBytes As Byte() = computeHash(clearText, key)

        ' Build the hex string by converting each byte.
        Dim hexString As New System.Text.StringBuilder()
        For i As Int32 = 0 To hashedBytes.Length - 1
            hexString.Append(hashedBytes(i).ToString("x2")) ' Use "x2" for lower case
        Next

        Return hexString.ToString()
    End Function

    Private Function computeHash(ByVal clearText As String, ByVal key As String) As Byte()

        Dim encoder As New System.Text.UTF8Encoding()

        Dim sha512hasher As New System.Security.Cryptography.HMACSHA512(encoder.GetBytes(key))
        Return sha512hasher.ComputeHash(encoder.GetBytes(clearText))

    End Function

End Module

