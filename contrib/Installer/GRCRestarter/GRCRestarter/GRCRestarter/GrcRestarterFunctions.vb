Imports System.Collections.Generic
Imports System.Data
Imports System.IO
Imports System.Text
Imports System.Object
Imports System.Security.Cryptography

Module GrcRestarterFunctions

    Public MerkleRoot As String = "0xda43abf15a2fcd57ceae9ea0b4e0d872981e2c0b72244466650ce6010a14efb8"


    Public Function ExtractXML(ByVal sData As String, ByVal sKey As String)
        Return ExtractXML(sData, sKey, "</" + Mid(sKey, 2, Len(sKey) - 1))
    End Function


    Public Function ExtractXML(ByVal sData As String, ByVal sStartKey As String, ByVal sEndKey As String)
        Dim iPos1 As Integer = InStr(1, sData, sStartKey)
        iPos1 = iPos1 + Len(sStartKey)
        Dim iPos2 As Integer = InStr(iPos1, sData, sEndKey)
        iPos2 = iPos2
        If iPos2 = 0 Then Return ""
        Dim sOut As String = Mid(sData, iPos1, iPos2 - iPos1)
        Return sOut
    End Function

    Public Function mRetrieveUpgrade(ByVal mData As GRCSec.GridcoinData, ByVal sId As String, _
                                        ByVal sTargetPath As String, ByVal sName As String, ByVal sPass As String) As String
        Dim sFullPath As String = sTargetPath + "\" + sName
        Dim sData As String = mData.UpgradeBlob2(sId)
        If InStr(1, sData, "<ERROR>") > 0 Then MsgBox(sData, MsgBoxStyle.Critical) : Return ""
        Dim sBase64 As String = ExtractXML(sData, "<BLOB>", "</BLOB>")
        mData.DecryptAES512AttachmentToFile(sFullPath, sBase64, sPass)
        Return sFullPath
    End Function

    Public Function GetMd5OfFile(ByVal sFN As String) As String
        If File.Exists(sFN) = False Then Return ""

        Dim md5 As Object
        md5 = System.Security.Cryptography.MD5.Create()
        Dim fs As Stream
        fs = File.OpenRead(sFN)
        md5 = md5.ComputeHash(fs)
        fs.Close()
        Dim sOut As String
        sOut = ByteArrayToHexString(md5)
        Return sOut
    End Function

    Public Function ByteArrayToHexString(ByVal ba As Byte()) As String
        Dim hex As StringBuilder
        hex = New StringBuilder(ba.Length * 2)
        For Each b As Byte In ba
            hex.AppendFormat("{0:x2}", b)
        Next
        Return hex.ToString()
    End Function

End Module
