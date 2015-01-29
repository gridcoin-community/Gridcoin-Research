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
