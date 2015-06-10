
Imports BoincStake

Imports System.Net.HttpWebRequest
Imports System.Text
Imports System.IO
Imports System.Data
Imports System.Object
Imports System.Security.Cryptography
Imports System.Speech.Synthesis


Imports System.Net

Public Class Form1
    Public mU As BoincStake.Utilization


    Public night As Boolean
  
    Public Structure Crypt
        Dim Success As Double
        Dim [Return] As Object
    End Structure
    Public Structure Symbol
        Public marketid As String
        Public label As String
        Public lasttradeprice As Double
        Public volume As Double
        Public lasttradetime As String
        Public primaryname As String
        Public primarycode As String
        Public secondaryname As String
        Public secondarycode As String
        Public rencenttrades As Object
    End Structure
    Public Function ByteToString(b() As Byte)
        Dim sReq As String
        sReq = System.Text.Encoding.UTF8.GetString(b)
        Return sReq
    End Function
    Public Function RetrieveSiteSecurityInformation(sURL As String) As String
        Dim u As New Uri(sURL)
        Dim sp As ServicePoint = ServicePointManager.FindServicePoint(u)
        Dim groupName As String = Guid.NewGuid().ToString()
        Dim req As HttpWebRequest = TryCast(HttpWebRequest.Create(u), HttpWebRequest)
        req.ConnectionGroupName = groupName
        Try
            Using resp As WebResponse = req.GetResponse()
            End Using
            sp.CloseConnectionGroup(groupName)
            Dim key As Byte() = sp.Certificate.GetPublicKey()
            Dim sOut As String
            sOut = ByteArrayToHexString(key)
            Return sOut
        Catch ex As Exception
            'Usually due to either HTTP, 501, Not Implemented...etc.
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


    Public bu As New BoincStake.Utilization

    Public Function ParseDate(sDate As String)
        'parses microsofts IIS date to a date, globally
        Dim vDate() As String
        vDate = Split(sDate, " ")
        If UBound(vDate) > 0 Then
            Dim sEle1 As String = vDate(0)
            Dim vEle() As String
            vEle = Split(sEle1, "/")
            If UBound(vEle) > 1 Then
                'Handle mm-dd-yyyy or dd-mm-yyyy or web server PST or web server UST:
                Dim sTr As String
                Dim sTime1 As String
                Dim sTime2 As String
                sTime1 = vDate(UBound(vDate) - 1)
                sTime2 = vDate(UBound(vDate) - 0)
                sTr = Trim(DateSerial(vEle(2), vEle(0), vEle(1))) + " " + Trim(sTime1) + " " + Trim(sTime2)
                If IsDate(sTr) Then Return CDate(sTr) Else Return CDate("1-1-2031")
            End If
        End If
        Return CDate("1-1-2031")

    End Function
  
    Private Sub Button1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button1.Click

        Dim sCPIDs As String = ""

        Dim bOut As Boolean
        Dim m As New md5

        bOut = m.CompareCPID("aeed96e8131dca89a6b48b7843fa3585", _
                           "aeed96e8131dca89a6b48b7843fa35853f67ca9738c4376b70959d6c3c683c6a3a3b939f6634ca379a9b986c3b36663e786a6f7475706f417566782f7468", _
                           "1d0ee24fde6150f64721a40093bd68b8cb720457cc358b85f15fffecdf2cda22")

        mU = New Utilization

        mU.ShowMiningConsole()

        Dim sBeacon As String = "784afb35d92503160125feb183157378<COL>Nzg0YWZiMzVkOTI1MDMxNjAxMjVmZWIxODMxNTczNzgzYWM1NjgzYWM2MzM2NjM0OWI2YjQyNjY2NTMzOTczNTk3M2Y0MDNhMzZjOTMxNjkzZGM2MzczOGM4Mzg2NzY4NzM3MDYzNjk2MjZkNjc3MDczNjUzMzQxNjg3MzZhNjU2NDcwNmE2ZjJmNzY3NDs1ZmFiNDBiZWE4ZGQxYTAwZmRkZTc5N2YxMDdmM2Q3YTEwN2MzNjA2ZGI1Y2U5NWZhYTA5OTg2YjQwNDk2OTlkO1NISHJGYUVaWXg2VDFhTVhXV3F5RHpvWFhZS0pXVWZyR1A=<ROW>"


        mU.SyncCPIDsWithDPORNodes(sBeacon)



        Exit Sub


        If False Then

            Dim sDir As String = "C:\Users\AppData\Roaming\GridcoinResearch\reports\magnitude_1433284769.csv"

            Dim sr As New StreamReader(sDir)
            Dim sTemp As String
            sr.ReadLine()

            While sr.EndOfStream = False
                sTemp = sr.ReadLine
                Dim vTemp() As String
                vTemp = Split(sTemp, ",")

                sCPIDs = sCPIDs + vTemp(0) + ","
            End While
            Dim vCPIDs() As String
            vCPIDs = Split(sCPIDs, ",")
           

        End If


        'Dim sContract As String = GetMagnitudeContract()
        Stop

       

        End

    End Sub

    Public Function WriteGenericData(sKey As String, sValue As String)
        '  Dim r As New Row
        ' r.Added = Now
        'r.Expiration = DateAdd(DateInterval.Minute, 1, Now)
        'r.Database = "Generic"
        'r.Table = "DummyTable" + Mid(sKey, 1, 1) + Mid(sValue, 1, 1)

        'r.PrimaryKey = sKey
        'r.DataColumn1 = sValue
        'modPersistedDataSystem.Store(r)


    End Function
    


    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = "c:\cryptsy.txt"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(Now) + ", " + sData)
            sw.Close()
        Catch ex As Exception
        End Try

    End Sub

    Private Sub Button2_Click(sender As System.Object, e As System.EventArgs) Handles Button2.Click

        mU.ShowMiningConsole()
    End Sub
End Class
