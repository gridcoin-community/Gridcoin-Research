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
    Public merkleroot2 As String = "0xda43abf15abcdefghjihjklmnopq872981e2c0b72244466650ce6010a14efb8"

    Private GBoincThreadCount As Guid
    Private GBoincCPUUtilization As Guid
    Private GBoincAvgCredits As Guid

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
    Public Function ReturnGVMGuid(ByVal sType As String) As String
        Try

        GBoincThreadCount = Guid.NewGuid
        GBoincCPUUtilization = Guid.NewGuid
        GBoincAvgCredits = Guid.NewGuid
        Dim sHash As String
        sHash = GBoincThreadCount.ToString() + GBoincCPUUtilization.ToString + GBoincAvgCredits.ToString
        Dim sKey As String
        sKey = Des3DecryptData(sType)
        sHash = sHash + sKey
            Return sHash
        Catch ex As Exception

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
    Public Function ToBase64(ByVal data As String) As String
        Try
            Return Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(data))

        Catch ex As Exception
            Return ""
        End Try
    End Function
    Public Function FromBase64(ByVal data As String) As String
        Try
            Return Encoding.UTF8.GetString(Convert.FromBase64String(data))

        Catch ex As Exception
            Return ""
        End Try
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
        Dim sPath As String = GetBoincProgFolder()
        Dim sMD5 As String
        Dim sFilePath As String = sPath & "boinc.exe"
        sMD5 = GetMd5(sFilePath)
        _BoincMD5 = sMD5
        Return sMD5
    End Function
    Public Function Deprecated_ReturnOldBA() As Long
        If _OldBA <> 0 Then Return _OldBA


        '1.  Retrieve the Boinc MD5 Hash
        '2.  Verify the boinc.exe contains the Berkeley source libraries
        '3.  Verify the exe is an official release
        '4.  Verify the size of the exe is above the threshold
        Try

            Dim sFolder As String = GetBoincProgFolder()
            Dim sPath As String = sFolder & "boinc.exe"
            Dim s As String = File.ReadAllText(sPath)
            Dim info As New FileInfo(sPath)
            Dim sz As Long = info.Length
            'Verify windows & linux size, greater than .758528 mb (758,528)

            If sz < (758528 / 2) Then _OldBA = -1 : Return -1 'Invalid executable

            If InStr(1, s, "http://boinc.berkeley.edu") = 0 Then
                _OldBA = -2
                Return -2 'failed authenticity check for libraries
            End If

            If InStr(1, s, "LIBEAY32.dll") = 0 Then _OldBA = -3 : Return -3 'Failed authenticity check for libraries
            Dim dir As DirectoryInfo
            dir = New DirectoryInfo(sPath)
            Dim sTrayPath As String
            sTrayPath = System.IO.Path.GetDirectoryName(sPath) + "\boinctray.exe"

            info = New FileInfo(sTrayPath)
            sz = info.Length
            If sz < 30000 Then _OldBA = -4 : Return -4 'Failed to find Boinc Tray EXE
            Return 1 'Success
        Catch ex As Exception
            _OldBA = -10
            Return -10 'Error
        End Try

    End Function

    Private Sub Merkle(ByVal sSalt As String)
        Try
            TripleDes.Key = TruncateHash(MerkleRoot + Right(sSalt, 4), TripleDes.KeySize \ 8)
            TripleDes.IV = TruncateHash("", TripleDes.BlockSize \ 8)
        Catch ex As Exception

        End Try

    End Sub

    Private Sub MerkleAES(ByVal sSalt As String, ByRef Encryptor As Aes)
        Try
            Aes.Create.Key = TruncateHash(MerkleRoot + Right(sSalt, 4), TripleDes.KeySize \ 8)
            Aes.Create.IV = TruncateHash("", TripleDes.BlockSize \ 8)

        Catch ex As Exception

        End Try

    End Sub

    Public Function Des3EncryptData(ByVal plaintext As String) As String

        Try

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
        Try
            Return Convert.ToBase64String(ms.ToArray)

        Catch ex As Exception
        
            End Try


        Catch ex As Exception

        End Try
    End Function

    Public Function Des3DecryptData(ByVal encryptedtext As String) As String
        Try

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
        Catch ex As Exception
        
        End Try

    End Function





    Public Function KeyValue(ByVal sKey As String) As String
        Try
            Dim sFolder As String
            sFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch"
            Dim sPath As String
            sPath = sFolder + "\gridcoinresearch.conf"
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
            Return ""

        Catch ex As Exception
            Return ""
        End Try
    End Function

    Public Function UnixTimestamp() As Long
        Dim span As TimeSpan
        span = (Now - New DateTime(1970, 1, 1, 0, 0, 0, 0).ToLocalTime())
        Return Val(span.TotalSeconds)
    End Function

    Public Function Dep1(ByVal sGridBlockHash1 As String, ByVal sGridBlockHash2 As String, _
                              ByVal sGridBlockHash3 As String, sGridBlockHash4 As String, _
                              ByVal sBoincHash As String) As Double
        'DepResultCodes

        '+1 Valid
        '-1 CPU Hash does not contain gridcoin block hash
        '-2 CPU Source Hash Invalid
        '-10 Boinc Hash Invalid
        '-11 Boinc Hash Present but invalid
        '-12 MD5 Error
        '-14 Rehashed output error
        '-15 CPU hash does not match SHA computed hash
        '-16 General Error
        '-17 ScryptSleep Exception

        Try


            If Len(sBoincHash) < 80 Then Return -10
            Dim vBoincHash() As String
            vBoincHash = Split(sBoincHash, ",")
            If UBound(vBoincHash) < 8 Then Return -11 'Invalid Boinc Hash
            Dim sCPUSourceHash As String
            Dim sCPUHash As String

            sCPUSourceHash = vBoincHash(9)
            sCPUHash = vBoincHash(8)
            Dim sMD5 As String
            sMD5 = vBoincHash(0)
            If Len(sMD5) < 7 Then Return -12 'MD5 Error



            'Verify CPUSourceHash contains Gridcoin block hash
            If (Not sCPUSourceHash.Contains(sGridBlockHash1) And Not sCPUSourceHash.Contains(sGridBlockHash2) And Not sCPUSourceHash.Contains(sGridBlockHash3) And Not sCPUSourceHash.Contains(sGridBlockHash4)) Then
                Return -1 'CPU Hash does not contain Gridcoin block hash
            End If
            'Extract MD5 from Source Hash
            Dim vCPUSourceHash() As String
            sCPUSourceHash = Replace(sCPUSourceHash, "\\", "\")

            vCPUSourceHash = Split(sCPUSourceHash, "\")
            If UBound(vCPUSourceHash) < 7 Then Return -2 'CPU Source Hash Invalid
            Dim bHash() As Byte
            Dim cHash As String
            'Retrieve solvers GRC address
            Dim sGRCAddress As String
            If UBound(vBoincHash) > 4 Then sGRCAddress = vBoincHash(5) '

            ''Retrieve solved blockhash
            Dim sOriginalBlockHash As String
            If UBound(vCPUSourceHash) > 0 Then sOriginalBlockHash = vCPUSourceHash(0)

            'Verify sleep level
            Dim bSleepVerification As Boolean
            Dim dSLL As Double = 0
            Dim dNL As Double = 0

           

            'ReHash the Source Hash
            bHash = System.Text.Encoding.ASCII.GetBytes(sCPUSourceHash)
            Dim objSHA1 As New SHA1CryptoServiceProvider()

            cHash = Replace(BitConverter.ToString(objSHA1.ComputeHash(bHash)), "-", "")
            'Extract difficulty
            Dim diff As String
            Dim targetms As Long = 10000 'This will change as soon as we implement the Moore's Law equation
            diff = Trim(Math.Round(targetms / 5000, 0))
            Dim sBoincAvgCredits As String
            Dim sCPUUtilization As String
            sCPUUtilization = vCPUSourceHash(2)
            sBoincAvgCredits = vCPUSourceHash(3)
            Dim sThreadCount As String
            sThreadCount = vCPUSourceHash(4)
            If Len(cHash) < 10 Then Return -14
            If cHash <> sCPUHash Then Return -15
            'Check Work

            If cHash.Contains(Trim(diff)) And cHash.Contains(String.Format("{0:000}", sCPUUtilization)) _
                And cHash.Contains(Trim(Val(sBoincAvgCredits))) _
                And cHash.Contains(Trim(Val(sThreadCount))) Then
                Return 1
            End If

            Return -16



        Catch ex As Exception
            Log("(Cryptography): " + ex.Message + ":" + ex.Source)

            Return -18
        End Try


    End Function

    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = GetGridFolder() + "debugGVM.log"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(Now) + ", " + sData)
            sw.Close()
        Catch ex As Exception
        End Try

    End Sub
   

    Public Function GetGridFolder() As String
        Dim sTemp As String
        sTemp = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\Gridcoin\"
        Return sTemp
    End Function


End Module
