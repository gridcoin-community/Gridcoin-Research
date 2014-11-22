
Imports BoincStake

Imports System.Net.HttpWebRequest
Imports System.Text
Imports System.IO
Imports System.Data
Imports System.Object
Imports System.Security.Cryptography
Imports System.Data.SqlClient


Imports System.Net

Public Class Form1
    Dim oSql As New SQLBase

  
    Public night As Boolean

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

    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = GetGridFolder() + "sql_debug.log"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(Now) + ", " + sData)
            sw.Close()
        Catch ex As Exception
        End Try

    End Sub
    Private Sub Form1_Load(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Load



    End Sub

    Private Sub Button2_Click(sender As System.Object, e As System.EventArgs)
       

    End Sub

    Private Sub Button1_Click(sender As System.Object, e As System.EventArgs) Handles Button1.Click
        Dim sql As String




        sql = "Select * from grid1"
        'Dim gr As New SqlDataReader
        Dim dt As New DataTable


        dt = oSql.GetDataTable(sql)


        Dim ws As New WebServer

        Dim sData As String
        sData = oSql.TableToData("node")
        oSql.DataToFile(sData, "node.txt")

        'Add test records
        oSql.InsertRecord("Confirm", "GRCFrom,GRCTo,txid,amount,Confirmed", "'a','b','123','100','0'")


        Stop



    End Sub

    Private Sub Button2_Click_1(sender As System.Object, e As System.EventArgs) Handles Button2.Click
        
        oSql.DropBaseTables() : End

    End Sub

    Private Sub Button3_Click(sender As System.Object, e As System.EventArgs) Handles Button3.Click
        oSql.StartConsensusRound()

    End Sub
End Class
