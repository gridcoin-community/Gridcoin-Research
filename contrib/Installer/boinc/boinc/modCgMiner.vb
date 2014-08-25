
Imports System.Net.HttpWebRequest
Imports System.Text
Imports System.IO
Imports System.Net

Module modCgMiner

    Public Function StringToByte(value As String) As Byte()
        Dim array() As Byte = System.Text.Encoding.ASCII.GetBytes(value)
        Return array
    End Function
    


End Module
