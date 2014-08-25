Imports System.Text

Public Class HMACSHA512Hasher

    Private Sub New()
        ' Prevent instantiation
    End Sub

    Public Shared Function Base64Hash(ByVal clearText As String) As String
        Return Base64Hash(clearText, String.Empty)
    End Function

    Public Shared Function Base64Hash(ByVal clearText As String, ByVal key As String) As String
        Dim hashedBytes As Byte() = computeHash(clearText, key)
        Return Convert.ToBase64String(hashedBytes)
    End Function

    Public Shared Function HexHash(ByVal clearText As String) As String
        Return HexHash(clearText, String.Empty)
    End Function

    Public Shared Function HexHash(ByVal clearText As String, ByVal key As String) As String
        Dim hashedBytes As Byte() = computeHash(clearText, key)

        ' Build the hex string by converting each byte.
        Dim hexString As New System.Text.StringBuilder()
        For i As Int32 = 0 To hashedBytes.Length - 1
            hexString.Append(hashedBytes(i).ToString("x2")) ' Use "x2" for lower case
        Next

        Return hexString.ToString()
    End Function

    Private Shared Function computeHash(ByVal clearText As String, ByVal key As String) As Byte()

        Dim encoder As New System.Text.UTF8Encoding()

        Dim sha512hasher As New System.Security.Cryptography.HMACSHA512(encoder.GetBytes(key))
        Return sha512hasher.ComputeHash(encoder.GetBytes(clearText))

    End Function

End Class
