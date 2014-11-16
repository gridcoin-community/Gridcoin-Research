Imports System
Imports System.IO
Imports System.Security.Cryptography
Imports System.Text


Public Module ModuleZeroProofKnowledge
    Private Const hexDigits As String = "0123456789ABCDEF"

    Public Function BytesToHexString(ByVal bytes() As Byte) As String
        Dim sb As New StringBuilder(bytes.Length * 2)
        For Each b As Byte In bytes
            sb.AppendFormat("{0:X2}", b)
        Next b
        Return sb.ToString()
    End Function
    Public Function HexStringToBytes(ByVal str As String) As Byte()
        ' Determine the number of bytes
        Dim bytes(str.Length >> 1 - 1) As Byte
        For i As Integer = 0 To str.Length - 1 Step 2
            Dim highDigit As Integer = hexDigits.IndexOf(Char.ToUpperInvariant(str.Chars(i)))
            Dim lowDigit As Integer = hexDigits.IndexOf(Char.ToUpperInvariant(str.Chars(i + 1)))
            If highDigit = -1 OrElse lowDigit = -1 Then
                Throw New ArgumentException("The string contains an invalid digit.", "s")
            End If
            bytes(i >> 1) = CByte((highDigit << 4) Or lowDigit)
        Next i
        Return bytes
    End Function

End Module
Public Class ZeroProofKnowledge


End Class

Class Alice

    Public Shared Sub InitializeAlicesKey()

        Dim bob As New Bob()
        If (True) Then
            Using dsa As New ECDsaCng()


                '	</option><option value="secp256k1">secp256k1

                '<option value="SHA256withECDSA">SHA256withECDSA


                dsa.HashAlgorithm = CngAlgorithm.Sha256

                bob.key = dsa.Key.Export(CngKeyBlobFormat.EccPublicBlob)

                Dim pubkey As String = "0421c0d2c14467d6572aa18922580879fcc192bfe8f348fd7dd676a054324e1bf862b24135458c3b66ed505ebb86a42db2a41e002635af57ce1cb6bbba7495af2e"

                '    bob.key = HexStringToBytes(pubkey)


                Dim data() As Byte = {21, 5, 8, 12, 207}

                'Dim sAsciiData = "aaa"
                'Dim data() As Byte = StringToByteArray(sAsciiData)

                'Dim signature As Byte() = dsa.SignData(data)
                Dim sHexSig = "3045022100a735bad950a4e9d4a9a33fd735de999a24a6756e8a5072bef138c1805249e0090220624ba7732c799764d43c2b4b4398a08977243d1a427fc5b8e2c7b2734dc2006e"

                Dim signature() As Byte = HexStringToBytes(sHexSig)



                bob.Receive(data, signature)
            End Using
        End If

    End Sub 'Main
End Class 'Alice 


Public Class Bob
    Public key() As Byte

    Public Sub Receive(ByVal data() As Byte, ByVal signature() As Byte)
        Using ecsdKey As New ECDsaCng(CngKey.Import(key, CngKeyBlobFormat.EccPublicBlob))



            If ecsdKey.VerifyData(data, signature) Then
                Console.WriteLine("Data is good")
            Else
                Console.WriteLine("Data is bad")
            End If
        End Using

    End Sub 'Receive
End Class 'Bob 

