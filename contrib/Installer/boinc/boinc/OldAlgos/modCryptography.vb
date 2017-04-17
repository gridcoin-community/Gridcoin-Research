Imports System.Runtime.InteropServices
Imports System.Threading
Imports System.IO
Imports System.Collections.Generic
Imports System.Data
Imports System.Text
Imports System.Object
Imports System.Security.Cryptography



Public Module modCryptography
    Private TripleDes As New TripleDESCryptoServiceProvider
    Public MerkleRoot As String = "0xda43abf15a2fcd57ceae9ea0b4e0d872981e2c0b72244466650ce6010a14efb8"

    Public Function GetMd5(ByVal sFN As String) As String

        Try

            Dim md5 As Object
            md5 = System.Security.Cryptography.MD5.Create()
            Dim fs As Stream
            fs = File.OpenRead(sFN)
            md5 = md5.ComputeHash(fs)
            fs.Close()
            Dim sOut As String
            sOut = ByteArrayToHexString(md5)
            Return sOut
        Catch ex As Exception
            Return "MD5Error"
        End Try

    End Function
    Private Function TruncateHash(ByVal key As String, ByVal length As Integer) As Byte()
        Dim sha1 As New SHA1CryptoServiceProvider
        ' Hash the key. 
        Dim keyBytes() As Byte =
            System.Text.Encoding.Unicode.GetBytes(key)
        Dim hash() As Byte = sha1.ComputeHash(keyBytes)
        ' Truncate or pad the hash. 
        ReDim Preserve hash(length - 1)
        Return hash
    End Function

    Public Function ByteArrayToHexString(ByVal ba As Byte()) As String
        Dim hex As StringBuilder
        hex = New StringBuilder(ba.Length * 2)
        For Each b As Byte In ba
            hex.AppendFormat("{0:x2}", b)
        Next
        Return hex.ToString()
    End Function

    Public Function modBoincMD5() As String
        If Len(_BoincMD5) > 0 Then Return _BoincMD5
        Dim sPath As String = GetBoincFolder()
        Dim sMD5 As String
        Dim sFilePath As String = sPath & "boinc.exe"
        sMD5 = GetMd5(sFilePath)
        _BoincMD5 = sMD5
        Return sMD5
    End Function
    Public Function VerifyBoincAuthenticity() As Long
        If _BoincAuthenticity <> 0 Then Return _BoincAuthenticity

        '1.  Retrieve the Boinc MD5 Hash
        '2.  Verify the boinc.exe contains the Berkeley source libraries
        '3.  Verify the exe is an official release
        '4.  Verify the size of the exe is above the threshold
        Try

            Dim sFolder As String = GetBoincFolder()
            Dim sPath As String = sFolder & "boinc.exe"
            Dim s As String = File.ReadAllText(sPath)
            Dim info As New FileInfo(sPath)
            Dim sz As Long = info.Length
            'Verify windows & linux size, greater than .758528 mb (758,528)

            If sz < (758528 / 2) Then _BoincAuthenticity = -1 : Return -1 'Invalid executable

            If InStr(1, s, "http://boinc.berkeley.edu") = 0 Then
                _BoincAuthenticity = -2
                Return -2 'failed authenticity check for libraries
            End If

            If InStr(1, s, "LIBEAY32.dll") = 0 Then _BoincAuthenticity = -3 : Return -3 'Failed authenticity check for libraries
            Dim dir As DirectoryInfo
            dir = New DirectoryInfo(sPath)
            Dim sTrayPath As String
            sTrayPath = System.IO.Path.GetDirectoryName(sPath) + "\boinctray.exe"

            info = New FileInfo(sTrayPath)
            sz = info.Length
            If sz < 30000 Then _BoincAuthenticity = -4 : Return -4 'Failed to find Boinc Tray EXE
            Return 1 'Success
        Catch ex As Exception
            _BoincAuthenticity = -10
            Return -10 'Error
        End Try

    End Function



    Private Sub Merkle(ByVal sSalt As String)
        TripleDes.Key = TruncateHash(MerkleRoot + Right(sSalt, 4), TripleDes.KeySize \ 8)
        TripleDes.IV = TruncateHash("", TripleDes.BlockSize \ 8)
    End Sub

    Public Function Des3EncryptData(ByVal plaintext As String) As String

        ' Convert the plaintext string to a byte array. 
        Call Merkle(MerkleRoot)
        Dim plaintextBytes() As Byte = System.Text.Encoding.Unicode.GetBytes(plaintext)

        ' Create the stream. 
        Dim ms As New System.IO.MemoryStream
        ' Create the encoder to write to the stream. 
        Dim encStream As New CryptoStream(ms,
            TripleDes.CreateEncryptor(),
            System.Security.Cryptography.CryptoStreamMode.Write)

        ' Use the crypto stream to write the byte array to the stream.
        encStream.Write(plaintextBytes, 0, plaintextBytes.Length)
        encStream.FlushFinalBlock()

        ' Convert the encrypted stream to a printable string. 
        Return Convert.ToBase64String(ms.ToArray)
    End Function
    Public Function Des3DecryptData(ByVal encryptedtext As String) As String

        ' Convert the encrypted text string to a byte array. 
        Merkle(MerkleRoot)

        Dim encryptedBytes() As Byte = Convert.FromBase64String(encryptedtext)

        ' Create the stream. 
        Dim ms As New System.IO.MemoryStream
        ' Create the decoder to write to the stream. 
        Dim decStream As New CryptoStream(ms, TripleDes.CreateDecryptor(), System.Security.Cryptography.CryptoStreamMode.Write)

        ' Use the crypto stream to write the byte array to the stream.
        decStream.Write(encryptedBytes, 0, encryptedBytes.Length)
        decStream.FlushFinalBlock()

        ' Convert the plaintext stream to a string. 
        Return System.Text.Encoding.Unicode.GetString(ms.ToArray)
    End Function



    Public Function GetBoincFolder() As String
        Dim sTemp As String
        sTemp = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles)
        sTemp = Trim(Replace(sTemp, "(x86)", ""))
        sTemp = sTemp + "\Boinc\"
        If Not System.IO.Directory.Exists(sTemp) Then
            sTemp = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86) + "\Boinc\"
        End If
        Return sTemp
    End Function
    Public Function GetBoincAppDataFolder() As String
        Dim sTemp As String
        sTemp = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData)
        sTemp = sTemp + "\Boinc\"
        If Not System.IO.Directory.Exists(sTemp) Then
            sTemp = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86) + "\Boinc\"
        End If
        Return sTemp
    End Function
    Public Function KeyValue(ByVal sKey As String) As String
        Try
            Dim sFolder As String
            sFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\Gridcoin"
            Dim sPath As String
            sPath = sFolder + "\gridcoin.conf"
            Dim sr As New StreamReader(sPath)
            Dim sRow As String
            Dim vRow() As String
            Do While sr.EndOfStream = False
                sRow = sr.ReadLine
                vRow = Split(sRow, "=")
                If LCase(vRow(0)) = LCase(sKey) Then
                    sr.Close()

                    Return vRow(1)
                End If
            Loop
            sr.Close()

        Catch ex As Exception
            Return ""

        End Try
    End Function



End Module
