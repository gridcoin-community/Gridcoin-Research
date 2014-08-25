Imports Finisar.SQLite
Imports System.Text
Imports System.IO
Imports Microsoft.VisualBasic
Public Class GridcoinReader
    Public Rows As Long

    Public Structure GridcoinRow
        Public Values As Dictionary(Of Integer, Object)
        Public FieldNames As Dictionary(Of Integer, Object)
    End Structure
    Private dReader As Dictionary(Of Integer, GridcoinRow)
    Public Sub New()
        dReader = New Dictionary(Of Integer, GridcoinRow)
    End Sub
    Public Sub AddRow(gr As GridcoinRow)
        Rows = Rows + 1
        dReader.Add(Rows, gr)
    End Sub
    Public Function GetRow(iRow As Integer) As GridcoinRow
        Dim gr As New GridcoinRow
        gr = dReader(iRow)
        Return gr
    End Function
    Public Function Value(iRow As Integer, sColName As String) As Object
        Dim gr As New GridcoinRow
        gr = dReader(iRow)
        For x = 0 To gr.FieldNames.Count - 1
            If LCase(Trim(gr.FieldNames(x).ToString())) = LCase(Trim(sColName)) Then
                Return gr.Values(x)
            End If
        Next x
    End Function

End Class

Public Class Sql

    Private sOptDatabaseOptions As String = ""

    Private sDatabaseName As String = "gridcoin"
    Private bClean As Boolean
    Private mSql As String = ""
    Private mStr As String = ""
    Public bThrowUIErrors As Boolean = False
    Private msBlockData As String = ""
    Private Function CONNECTION_STR() As String
        Dim sPath As String
        Dim sFolder As String = GetGridFolder() + "Sql\"
        Try
            If Not System.IO.Directory.Exists(sFolder) Then MkDir(sFolder)
        Catch ex As Exception
        End Try
        sPath = sFolder + sDatabaseName
        sPath = Replace(sPath, "\", "//")
        Dim s As String = "Data Source=" + sPath + ";Version=3;" + sOptDatabaseOptions

        Return s
    End Function
    Private Function DatabaseExists() As Boolean
        Try
            Dim sql As String
            sql = "Select Value From System where id='1';"
            Dim gr As New GridcoinReader
            gr = GetGridcoinReader(sql)
            If gr.Rows = 0 Then Return False
            mStr = CStr(gr.Value(1, "Value"))
        Catch ex As Exception
            If LCase(ex.Message) Like "*locked*" Then Return True
            Return False
        End Try
        If Val(mStr) > 0 Then Return True
    End Function
    Public Sub ExecHugeQuery(sbSql As System.Text.StringBuilder)
        Dim vSql As String()
        Try

            vSql = Split(sbSql.ToString(), vbCrLf)
            If UBound(vSql) < 0 Then Exit Sub
            For y As Integer = 0 To UBound(vSql)
                Dim sql As String = vSql(y)
                Exec(sql)
            Next y
        Catch ex As Exception
            Log("Exec huge query" + ex.Message)

        End Try

    End Sub
    Public Function UpdateUserSummary()
        Dim sql As String
        sql = "Delete from UserSummary"
        Exec(sql)

        sql = "Insert Into UserSummary (ProjectCount,Address)"
        sql += "select count(Project) as ProjectCount,Address from ("
        sql += "select distinct project,address"
        sql += " from leaderboard "
        sql += "group by project,address) group by address"
        Exec(sql)

    End Function

    Public Sub Exec(Sql As String)
        Try
            Using c As New SQLiteConnection(CONNECTION_STR)
                c.Open()
                Using cmd As New SQLiteCommand(Sql, c)
                    cmd.ExecuteNonQuery()
                End Using
            End Using
        Catch ex As Exception
            'Log("Exec: " + Sql + ":" + ex.Message)

        End Try

    End Sub
    
    Public Function GetGridcoinReader(Sql As String) As GridcoinReader

        Try

            Dim g As New GridcoinReader

            Using c As New SQLiteConnection(CONNECTION_STR)
                c.Open()
                Using cmd As New SQLiteCommand(Sql, c)
                    Using rdr As SQLiteDataReader = cmd.ExecuteReader()
                        While rdr.Read
                            Dim gr As New GridcoinReader.GridcoinRow
                            For y = 0 To rdr.FieldCount - 1
                                If gr.FieldNames Is Nothing Then gr.FieldNames = New Dictionary(Of Integer, Object)


                                gr.FieldNames.Add(y, rdr.GetName(y))
                                If gr.Values Is Nothing Then gr.Values = New Dictionary(Of Integer, Object)

                                gr.Values.Add(y, rdr(y))
                            Next
                            g.AddRow(gr)
                        End While
                    End Using
                End Using
            End Using
            Return g
        Catch ex As Exception
            Log("GetGridcoinReader:" + Sql + ":" + ex.Message)

            If bThrowUIErrors Then Throw ex
        End Try


    End Function

    Public Function SQLKey(ByVal sKey As String) As String
        Try
            mSql = "Select Value from System where key='" + sKey + "'"
            Dim o As Object
            o = Me.QueryFirstRow(mSql, "Value")
            Return o.ToString()


        Catch ex As Exception

        End Try
        Return ""

    End Function
    Public Sub SQLKey(ByVal sKey As String, ByVal sValue As String)
        Try


            mSql = "Delete from System where key='" + sKey + "'" + vbCrLf
            mSql += "Insert into System (key,value,added) values ('" + sKey + "','" + sValue + "',date('now'));"
            Me.Exec(mSql)
        Catch ex As Exception

        End Try

    End Sub
    Private Sub CreateDatabase()
        If DatabaseExists() = True Then
            Exit Sub
        End If

        Try
            sOptDatabaseOptions = "New=True" ''New=True to create a new database

            mSql = "create table System (id integer primary key, key varchar(30),value varchar(30),Added datetime);"
            Exec(mSql)
            sOptDatabaseOptions = ""

            mSql = "Insert into System (id,key,value,added) values (1,'Gridcoin_Version','1.0',date('now'));"
            Exec(mSql)
            mSql = "CREATE TABLE Peers (id integer primary key, ip varchar(30), version varchar(40), Added datetime);"
            Exec(mSql)
            mSql = "INSERT INTO Peers (id, ip, version, Added) VALUES (1,'127.0.0.1','71002',date('now'));"
            Exec(mSql)
            mSql = "INSERT INTO Peers (id, ip, version, Added) VALUES (2,'127.0.0.1','71002',date('now','start of month','+1 month','-2 day'));"
            Exec(mSql)
            CreateLeaderboardTable()

            mSql = "CREATE TABLE Blocks (height integer primary key,hash varchar(100),confirmations varchar(30)," _
            & "blocksize varchar(10),version varchar(10), merkleroot varchar(200), tx varchar(900), " _
            & "blocktime varchar(12), nonce varchar(15), bits varchar(15), difficulty varchar(20), boinchash varchar(900), previousblockhash varchar(200), nextblockhash varchar(200)); "
            Exec(mSql)
            sOptDatabaseOptions = ""

            Exit Sub
        Catch ex As Exception
            sOptDatabaseOptions = ""
            MsgBox(ex.Message)
        End Try
    End Sub
    Public Function CreateLeaderboardTable()

        mSql = "Drop table Leaderboard"
        Try
            Exec(mSql)

        Catch ex As Exception

        End Try

        mSql = "CREATE TABLE Leaderboard (id integer primary key, Added datetime, Address varchar(100), Host varchar(50), Project varchar(40), Credits numeric(12,2), ProjectName varchar(100), " _
            & " ProjectURL varchar(150), ProjectCount numeric(5,0), Factor numeric(12,2), AdjCredits numeric(12,2), ScryptSleepChance numeric(6,2));"
        Exec(mSql)

        Try
            'mSql = "Create Table UserSummary (id integer primary key, ProjectCount integer,Address varchar(100), Host varchar(50), Credits numeric(12,2))"
            'Exec(mSql)
        Catch ex As Exception

        End Try

    End Function
   
   
   
    Public Function QueryFirstRow(ByVal sSql As String, sCol As String) As Object
        Dim o As Object

        Try
            Dim gr As New GridcoinReader
            gr = GetGridcoinReader(sSql)
            If gr.Rows = 0 Then Exit Function
            o = gr.Value(1, sCol)
            Return o
        Catch ex As Exception
            Log("Sql.QueryFirstRow:" + CONNECTION_STR() + ":" + sSql + ":" + ex.Message)
        Finally
        End Try
        Return o
    End Function
    Public Sub New()
        sDatabaseName = "gridcoinGVM"
        CreateDatabase()

    End Sub

    Public Sub New(strDatabaseName As String)
        sDatabaseName = strDatabaseName
        CreateDatabase()

    End Sub

    Public Sub Close()

    End Sub
    Protected Overrides Sub Finalize()
        Close()
        MyBase.Finalize()
    End Sub
End Class
