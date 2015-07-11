
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

        Dim sVotes As String = "<POLL><TITLE>default_speech_behavior</TITLE><EXPIRATION>07-20-2015 15:36:51</EXPIRATION><SHARETYPE>Both</SHARETYPE><QUESTION>Should_Gridcoin_Speech_Synthesizer_be_Off_by_Default?</QUESTION><ANSWERS> Advertising_And_Promotions;General_Development;E-Commerce_Development;Dividend_Reimbursement;New_User_Bonuses;Education_Endowment;On_By_Default</ANSWERS><ARRAYANSWERS><RESERVED></RESERVED><ANSWERNAME>Off_By_Default</ANSWERNAME><PARTICIPANTS>0</PARTICIPANTS><SHARES>0</SHARES><RESERVED></RESERVED><ANSWERNAME>On_By_Default</ANSWERNAME><PARTICIPANTS>1</PARTICIPANTS><SHARES>224828</SHARES></ARRAYANSWERS><TOTALPARTICIPANTS>1</TOTALPARTICIPANTS><TOTALSHARES>224828</TOTALSHARES><BESTANSWER>On_By_Default</BESTANSWER></POLL><POLL><TITLE>favorite_color</TITLE><EXPIRATION>07-04-2015 15:06:35</EXPIRATION><SHARETYPE>Magnitude</SHARETYPE><QUESTION>What_Is_Your_Favorite_Color</QUESTION><ANSWERS>Red;Green;Blue;Black;Yellow;White</ANSWERS><ARRAYANSWERS><RESERVED></RESERVED><ANSWERNAME>Red</ANSWERNAME><PARTICIPANTS>0</PARTICIPANTS><SHARES>0</SHARES><RESERVED></RESERVED><ANSWERNAME>Green</ANSWERNAME><PARTICIPANTS>0</PARTICIPANTS><SHARES>0</SHARES><RESERVED></RESERVED><ANSWERNAME>Blue</ANSWERNAME><PARTICIPANTS>0</PARTICIPANTS><SHARES>0</SHARES><RESERVED></RESERVED><ANSWERNAME>Black</ANSWERNAME><PARTICIPANTS>0</PARTICIPANTS><SHARES>0</SHARES><RESERVED></RESERVED><ANSWERNAME>Yellow</ANSWERNAME><PARTICIPANTS>1</PARTICIPANTS><SHARES>604</SHARES><RESERVED></RESERVED><ANSWERNAME>White</ANSWERNAME><PARTICIPANTS>0</PARTICIPANTS><SHARES>0</SHARES></ARRAYANSWERS><TOTALPARTICIPANTS>1</TOTALPARTICIPANTS><TOTALSHARES>604</TOTALSHARES><BESTANSWER>Yellow</BESTANSWER></POLL><POLL><TITLE>gender_poll</TITLE><EXPIRATION>07-04-2015 12:38:19</EXPIRATION><SHARETYPE>Both</SHARETYPE><QUESTION>Am_I_Male_Or_Female</QUESTION><ANSWERS>Male;Female</ANSWERS><ARRAYANSWERS><RESERVED></RESERVED><ANSWERNAME>Male</ANSWERNAME><PARTICIPANTS>0</PARTICIPANTS><SHARES>0</SHARES><RESERVED></RESERVED><ANSWERNAME>Female</ANSWERNAME><PARTICIPANTS>1</PARTICIPANTS><SHARES>168659</SHARES></ARRAYANSWERS><TOTALPARTICIPANTS>1</TOTALPARTICIPANTS><TOTALSHARES>168659</TOTALSHARES><BESTANSWER>Female</BESTANSWER></POLL>"
        mU = New Utilization
        mU.UpdateMagnitudesOnly()

        Dim sContract As String
        '   sContract = mU.GetNeuralContract()


        '     mU.SyncCPIDsWithDPORNodes("")
        '  sContract = mU.GetNeuralContract()

        '  mU.SetGenericVotingData(sVotes)
        ' mU.ShowVotingConsole()
        '
        ' mU.ShowMiningConsole()
        mU.ShowMiningConsole()


        Exit Sub



        End

    End Sub

    Public Function WriteGenericData(sKey As String, sValue As String)
        

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
        mU.SyncCPIDsWithDPORNodes("")

    End Sub
End Class
