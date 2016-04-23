Imports System.IO

Module p2pool

    Public Structure MiningKey
        Public MD5 As String
        Public CPID As String
        Public BPK As String
        Public GRC As String
        Public CPIDValid As Boolean
        Public ProjectName As String
        Public RAC As Double
    End Structure

    Public mbTestNet As Boolean = False


    Public Function ReceiveMiningKey(sKey As String) As MiningKey
        Dim m As New MiningKey
        If Len(sKey) < 10 Then Throw New Exception("Key too short")
        Dim data As String = FromBase64String(sKey)
        Dim vPool() As String
        vPool = Split(data, ";")
        If UBound(vPool) < 1 Then Throw New Exception("Malfordmed Key")
        Dim sGRC As String
        sGRC = vPool(0)
        Dim sBPK As String
        sBPK = vPool(1)
        Dim sMD5 As String
        sMD5 = GetMd5String(sBPK) 'cpid
        If Len(sGRC) < 5 Then Throw New Exception("GRC Wallet too short")
        If Len(sBPK) < 5 Then Throw New Exception("BPK Too short")
        m.BPK = sBPK
        m.CPID = sMD5
        m.GRC = sGRC
        m.MD5 = sMD5
        'Check validity of cpid here
        '...

        'Call out to GetNextGPUProject here...
        'Set RAC & Project Name
        m.CPIDValid = True
        Return m

    End Function
    Public Function ConfigPath() As String
        Dim sFolder As String
        sFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch"
        If mbTestNet Then sFolder += "\testnet"
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
        Dim sr As StreamReader
        Dim sw As StreamWriter
        Dim sInPath As String = ""
        Try
            sInPath = ConfigPath()
            Dim sOutPath As String = ConfigPath() + ".bak"

            Dim bFound As Boolean
            sr = New StreamReader(sInPath)
            sw = New StreamWriter(sOutPath, False)

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
            Try
                sr.close()
                sw.close()
            Catch ex1 As Exception
                Log("Unable to close " + sInPath + " " + ex1.Message)
            End Try
            Return ""
        End Try
    End Function

    Public Function GetSessionGuid() As String
        Dim sSessionGuid As String = KeyValue("SessionGuid")
        If sSessionGuid = "" Then
            UpdateKey("SessionGuid", Guid.NewGuid.ToString())
            sSessionGuid = KeyValue("SessionGuid")
        End If
        Return sSessionGuid
    End Function

    Public Function FileToBytes(sSourceFileName As String) As Byte()
        If sSourceFileName Is Nothing Then Exit Function

        Try

            Dim sFilePath As String = sSourceFileName
            Dim b() As Byte
            b = System.IO.File.ReadAllBytes(sFilePath)
            Return b
        Catch ex As Exception
            MsgBox("File may be open in another program, please close it first.", MsgBoxStyle.Critical)

        End Try

    End Function

    Private Function ByteArrayToHexString2(ByVal arrInput() As Byte) As String
        Dim strOutput As New System.Text.StringBuilder(arrInput.Length)
        For i As Integer = 0 To arrInput.Length - 1
            strOutput.Append(arrInput(i).ToString("X2"))
        Next
        Return strOutput.ToString().ToLower
    End Function
    Public Function GetMd5String(b As Byte()) As String
        Try
            Dim objMD5 As New System.Security.Cryptography.MD5CryptoServiceProvider
            Dim arrHash() As Byte
            arrHash = objMD5.ComputeHash(b)
            objMD5 = Nothing
            Dim sOut As String = ByteArrayToHexString2(arrHash)
            Return sOut

        Catch ex As Exception
            Return "MD5Error"
        End Try
    End Function

    Public Function GetMd5OfFile(sFile As String) As String
        Dim b() As Byte
        b = FileToBytes(sFile)
        Dim bHash As String
        bHash = GetMd5String(b)
        Return bHash
    End Function

    Public Function GetMd5String(ByVal sData As String) As String
        Try
            Dim objMD5 As New System.Security.Cryptography.MD5CryptoServiceProvider
            Dim arrData() As Byte
            Dim arrHash() As Byte

            ' first convert the string to bytes (using UTF8 encoding for unicode characters)
            arrData = System.Text.Encoding.UTF8.GetBytes(sData)

            ' hash contents of this byte array
            arrHash = objMD5.ComputeHash(arrData)

            ' thanks objects
            objMD5 = Nothing
            Dim sOut As String = ByteArrayToWierdHexString(arrHash)

            ' return formatted hash
            Return sOut
        Catch ex As Exception
            Return "MD5Error"
        End Try
    End Function

    ' utility function to convert a byte array into a hex string
    Private Function ByteArrayToWierdHexString(ByVal arrInput() As Byte) As String
        Dim strOutput As New System.Text.StringBuilder(arrInput.Length)
        For i As Integer = 0 To arrInput.Length - 1
            strOutput.Append(arrInput(i).ToString("X2"))
        Next
        Return strOutput.ToString().ToLower
    End Function


    Public Function StringToByte(sData As String)
        Dim keyBytes() As Byte = System.Text.Encoding.Unicode.GetBytes(sData)
        Return keyBytes
    End Function

    Public Function FromBase64String(sData As String) As String
        Dim ba() As Byte = Convert.FromBase64String(sData)
        Dim sOut As String = ByteToString(ba)
        Return sOut
    End Function
    Public Function ByteToString(b() As Byte)
        Dim sReq As String
        sReq = System.Text.Encoding.UTF8.GetString(b)
        Return sReq
    End Function

    Public Function StringToByteArray(sData As String) As Byte()
        Return StringToByte(sData)
    End Function

End Module
