Imports System.Text

Public Class MD5
    'Returns MD5 Hash of a string or cpid with blockhash

    Private RAM As New List(Of Guid)
    Public Function GetHex(i As Integer) As String
        Dim hex As New StringBuilder
        hex.AppendFormat("{0:x2}", i)
        Return hex.ToString()
    End Function
    Public Function UpdateMD5(longcpid As String, hash_block As String) As String
        Dim hexpos As Integer = 0
        Dim shash As String = HashHex(hash_block)
        For i1 As Integer = 0 To Len(longcpid) Step 2
            Dim iSalt As Integer = ROL(shash, i1, longcpid, hexpos)
            Append1(iSalt)
            hexpos += 1
        Next
        Return HexDigest()
    End Function
    Private Function HexDigest() As String
        Dim sOut As String = ""
        Try
            Dim objMD5 As New System.Security.Cryptography.MD5CryptoServiceProvider
            Dim arrData(RAM.Count - 1) As Byte
            Dim arrHash() As Byte
            Dim i As Long = 0
            For Each Guid In RAM
                arrData(i) = "&h" + Mid(Guid.ToString(), 10, 2)
                i += 1
            Next
            arrHash = objMD5.ComputeHash(arrData)
            objMD5 = Nothing
            sOut = ByteArrayToHexString2(arrHash)
            Return sOut

        Catch ex As Exception
            Return "MD5Error"
        End Try

    End Function

    Public Function HashHex(BlockHash As String) As String
        Dim sHash As String = GetMd5String(BlockHash)
        Return sHash
    End Function

    Public Function ROL(blockhash As String, iPos As Integer, hash As String, hexpos As Integer) As Integer
        Dim cpid3 As String = ""
        If iPos <= Len(hash) - 1 Then
            Dim hex As String = Mid(hash, iPos + 1, 2)
            Dim rorcount As Integer = 0
            rorcount = BitwiseCount(blockhash, hexpos)
            Dim b As Integer = HexToByte(hex) - rorcount
            If (b >= 0) Then
                Return b
            End If
            Return HexToByte("00")
        End If
        Return 0
    End Function
    Public Function HexToByte(sData As String) As Integer
        Dim lByte As Long = "&h" + sData
        Return lByte
    End Function
    Public Function BitwiseCount(Str As String, pos As Integer) As Integer
        Dim ch As String = ""
        If pos < Len(Str) Then
            ch = Mid(Str, pos + 1, 1)
            Dim asc1 As Integer = Asc(ch)
            If (asc1 > 47 And asc1 < 71) Then asc1 = asc1 - 47
            Return asc1
        End If
        Return 1
    End Function
    Public Function CompareCPID(usercpid As String, longcpid As String, blockhash As String) As Boolean
        usercpid = LCase(usercpid)
        longcpid = LCase(longcpid)
        blockhash = LCase(blockhash)
        If Len(longcpid) < 34 Then Return False
        Dim cpid1 As String = Mid(longcpid, 1, 32)
        Dim cpid2 As String = Mid(longcpid, 33, Len(longcpid) - 31)
        Dim shortcpid As String = UpdateMD5(cpid2, blockhash)
        If shortcpid = "" Then Return False
        If shortcpid = cpid1 And cpid1 = usercpid And shortcpid = usercpid Then Return True
        Return False
    End Function
    Public Function GetMd5(ByVal sData As String) As String
        Try
            Dim objMD5 As New System.Security.Cryptography.MD5CryptoServiceProvider
            Dim arrData() As Byte
            Dim arrHash() As Byte
            arrData = System.Text.Encoding.UTF8.GetBytes(sData)
            arrHash = objMD5.ComputeHash(arrData)
            objMD5 = Nothing
            Dim sOut As String = ByteArrayToHexString2(arrHash)
            Return sOut
        Catch ex As Exception
            Return "MD5Error"
        End Try
    End Function
    Private Sub Append1(salt As Integer)
        If salt = 0 Then Exit Sub
        Dim g As Guid = Guid.NewGuid()
        Dim s As String = g.ToString()
        Mid(s, 10, 2) = GetHex(salt)
        RAM.Add(New Guid(s))
    End Sub
    Private Function ByteArrayToHexString2(ByVal arrInput() As Byte) As String
        Dim strOutput As New System.Text.StringBuilder(arrInput.Length)
        For i As Integer = 0 To arrInput.Length - 1
            strOutput.Append(arrInput(i).ToString("X2"))
        Next
        Return strOutput.ToString().ToLower
    End Function


End Class
