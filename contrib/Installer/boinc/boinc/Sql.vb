Imports Finisar.SQLite
Imports System.Text
Imports System.IO
Imports Microsoft.VisualBasic
Imports System.Net
Imports System.Windows.Forms
Imports System.Globalization

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


    Public Sub Exec(Sql As String)
        Try
            Using c As New SQLiteConnection(CONNECTION_STR)
                c.Open()
                Using cmd As New SQLiteCommand(Sql, c)
                    cmd.ExecuteNonQuery()
                End Using
            End Using
        Catch ex As Exception
            ' Log("Exec: " + Sql + ":" + ex.Message)

        End Try

    End Sub
    Public Sub SqlHousecleaning()
        If bSqlHouseCleaningComplete Then Exit Sub
        Dim sql As String
        bSqlHouseCleaningComplete = True
        Try
            sql = "Delete from Peers where added < date('now','start of month','+1 month','+1 day');"
            Exec(sql)
            Close()
        Catch ex As Exception
        End Try
    End Sub
    Public Function GetMasterNodeURL()
        'Find the node that is currently the leader, with a synced consensus:
        Dim sHost As String
        sHost = DefaultHostName("p2psql.gridcoin.us", False) + ":" + Trim(DefaultPort(32500, False))

        '        sHost = "localhost:0


        Return sHost
    End Function
    Public Function SQLQuery(sHost As String, sSQL As String, sParams() As Byte, sToken As String) As String
        Dim sURL As String
        sURL = "http://" + sHost + "/p2psql.aspx?query="
        sSQL = Replace(sSQL, vbCr, "<CR>")


        sSQL = Replace(sSQL, vbLf, "<LF>")
        For x = 1 To 3
            Try
                Dim wc As New GRCWebClient
                Using wc
                    wc.Headers(HttpRequestHeader.ContentType) = "application/x-www-form-urlencoded"
                    wc.Headers.Add("Query:<QUERY>" + sSQL + "</QUERY>")
                    wc.Headers.Add("Token:" + sToken)

                    Dim result As String
                    Dim responseArray As Byte()
                    If sParams Is Nothing Then
                        result = wc.UploadString(sURL, "")

                    Else
                        Dim sBase64 As String = System.Convert.ToBase64String(sParams, 0, sParams.Length)
                        Dim sPreEncoded As String = ""
                        sPreEncoded = sBase64

                        sBase64 = Replace(sBase64, "/", "HTMLSLASH")
                        sBase64 = Replace(sBase64, "==", "HTMLDOUBLEEQUALS")

                        Dim p2 As Byte()
                        p2 = Encoding.ASCII.GetBytes(sBase64)

                        responseArray = wc.UploadData(sURL, p2)

                        result = Encoding.ASCII.GetString(responseArray)

                    End If


                    Dim sErr As String
                    sErr = ExtractXML(result, "<ERROR>", "</ERROR>")
                    If Len(sErr) > 0 Then MsgBox(sErr)
                    Return result

                End Using
            Catch ex As Exception
                If x = 3 Then Throw ex
            End Try
        Next
    End Function

    Public Function ExecuteP2P(sCommand As String, sParams() As Byte, lSource As Long) As String
        Dim sHost As String
        Dim sData As String = ""
        System.Windows.Forms.Cursor.Current = Cursors.WaitCursor

        Dim sToken As String = GetSecurityToken(lSource)
        For x = 1 To 3
            Try
                sHost = GetMasterNodeURL()
                sData = SQLQuery(sHost, sCommand, sParams, sToken)
                If sData = "" Then
                    System.Windows.Forms.Cursor.Current = Cursors.Default
                    Return sData

                End If
                If sData <> "" And x = 3 Then
                    System.Windows.Forms.Cursor.Current = Cursors.Default

                    Return sData

                End If

            Catch ex As Exception
                Log(ex.Message)
                'Throw ex
                System.Windows.Forms.Cursor.Current = Cursors.Default

                If x = 3 Then Return ex.Message
            End Try
        Next x
        System.Windows.Forms.Cursor.Current = Cursors.Default
    End Function
    Public Function BoincBlob(sBlobId As String) As String
        Dim sBlob As String
        sBlob = BoincHarmonyP2PExecute("select BlobData from attachment where id = '" + sBlobId + "' and deleted=0")
        If Len(sBlob) > 100 Then
            Return sBlob
        Else
            Return "<ERROR>" + sBlob + "</ERROR>"
        End If

    End Function
    Public Function BoincHarmonyP2PExecute(Sql As String)
        Dim sBoincBytes As String = ""

        Try
            System.Windows.Forms.Cursor.Current = Cursors.WaitCursor
            Dim sHarmonyP2PServerHost = GetMasterNodeURL()
            Dim sToken As String = GetSecurityToken(1)
            sBoincBytes = SQLQuery(sHarmonyP2PServerHost, Sql, Nothing, sToken)
            System.Windows.Forms.Cursor.Current = Cursors.Default
        Catch ex As Exception
            Log("BoincHarmonyP2PExecute:" + Sql + ":" + ex.Message)
            System.Windows.Forms.Cursor.Current = Cursors.Default
            If bThrowUIErrors Then Throw ex
            Return sBoincBytes
        End Try
        System.Windows.Forms.Cursor.Current = Cursors.Default
        Return sBoincBytes
    End Function
    Public Function UnCleanBody(sData As String) As String
        sData = Replace(sData, "[Q]", "'")
        sData = Replace(sData, "[LESSTHAN]", "<")
        sData = Replace(sData, "[GREATERTHAN]", ">")
        sData = Replace(sData, "[DQ]", Chr(34))


        Return sData

    End Function

    Public Function GetGridcoinReader(Sql As String, lSource As Long) As GridcoinReader

        Try
            System.Windows.Forms.Cursor.Current = Cursors.WaitCursor

            Dim g As New GridcoinReader

            Dim sData As String
            Dim sHost As String
            sHost = GetMasterNodeURL()
            
            Dim sToken As String = GetSecurityToken(lSource)

            sData = SQLQuery(sHost, Sql, Nothing, sToken)

            Dim vData() As String
            vData = Split(sData, "<ROW>")
            Dim vHeader() As String
            vHeader = Split(vData(0), "<COL>")

            For z = 1 To UBound(vData) - 1
                Dim gr As New GridcoinReader.GridcoinRow
                Dim sRow As String
                sRow = vData(z)
                Dim vRow() As String
                vRow = Split(sRow, "<COL>")

                For y = 0 To UBound(vRow) - 1
                    If gr.FieldNames Is Nothing Then gr.FieldNames = New Dictionary(Of Integer, Object)
                    Dim vDataType() As String
                    vDataType = Split(vHeader(y), "<TYPE>")
                    Dim sType As String = UCase(vDataType(1))
                    Dim sFieldName As String = vDataType(0)
                    gr.FieldNames.Add(y, sFieldName)
                    If gr.Values Is Nothing Then gr.Values = New Dictionary(Of Integer, Object)
                    'Cast string back to type
                    Dim oValue As Object
                    If sType = "SYSTEM.STRING" Then
                        oValue = vRow(y).ToString()
                        oValue = UnCleanBody(oValue)
                    ElseIf sType = "SYSTEM.INTEGER" Then
                        oValue = CInt(vRow(y))
                    ElseIf sType = "SYSTEM.DATETIME" Then
                        '12-17-2014 - Add support for dd-mm-yyyy date format for global cultures
                        'Dim global_date_style As System.Globalization.DateTimeStyles
                        'Dim culture As Globalization.CultureInfo = Globalization.CultureInfo.CreateSpecificCulture("en-US")
                        'oValue = CDate(vRow(y))
                        oValue = ConvertGlobalDate(vRow(y))
                        'Dim good_date As Boolean = DateTime.TryParseExact(vRow(y), "MM/dd/yyyy hh:mm:ss tt", culture, Globalization.DateTimeStyles.None, oValue)

                    ElseIf sType = "SYSTEM.DECIMAL" Then
                        oValue = CDbl("0" & vRow(y))

                    ElseIf sType = "SYSTEM.GUID" Then
                        oValue = Trim(vRow(y).ToString())

                    End If
                    gr.Values.Add(y, oValue)
                Next
                g.AddRow(gr)
            Next z
            System.Windows.Forms.Cursor.Current = Cursors.Default

            Return g
        Catch ex As Exception
            Log("GetGridcoinReader:" + Sql + ":" + ex.Message)
            System.Windows.Forms.Cursor.Current = Cursors.Default


            If bThrowUIErrors Then Throw ex
        End Try
        System.Windows.Forms.Cursor.Current = Cursors.Default


    End Function
    Public Function ConvertGlobalDate(ByVal strdate As String) As DateTime

        Dim dtOut As DateTime

        Try

            Dim vD() As String
            vD = Split(strdate, " ")

            If UBound(vD) < 1 Then Return Now

            Dim vDate() As String
            vDate = Split(vD(0), "/")

            Dim vHour() As String
            vHour = Split(vD(1), ":")

            Dim sAM As String = UCase(vD(2))
            If sAM = "PM" Then vHour(0) = Trim(Val(vHour(0) + 12))


            dtOut = New DateTime(Val(vDate(2)), Val(vDate(0)), Val(vDate(1)), Val(vHour(0)), Val(vHour(1)), Val(vHour(2)))

        Catch ex As Exception
            Log("Invalid date specified " + Trim(strdate))
            Return Now

        End Try
        Return dtOut

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
        Catch ex As Exception
        End Try

    End Function
    Public Function InsertBlockData()


        Dim vBlocks() As String = Split(msBlockData, "{ROW}")
        If UBound(vBlocks) < 5 Then Exit Function
        Try
            For i As Integer = 0 To UBound(vBlocks)
                If Len(vBlocks(i)) > 100 Then
                    InsertBlock(vBlocks(i))
                End If
            Next
            msBlockData = ""
        Catch ex As Exception
            Log("InsertBlocks: " + msBlockData + ":" + ex.Message)
        End Try

    End Function
    Public Function InsertBlocks(ByVal data As String)
        msBlockData = data
        If Trim(data) = "" Then Exit Function

        Dim thInsertBlocks As New Threading.Thread(AddressOf InsertBlockData)
        thInsertBlocks.Start()
    End Function
    Public Function InsertBlock(data As String)

        If KeyValue("UpdatingLeaderboard") = "true" Then Exit Function

        Dim sql As String
        sql = "Insert into Blocks (hash,confirmations,blocksize,height,version,merkleroot,tx,blocktime,nonce,bits,difficulty,boinchash,previousblockhash,nextblockhash) VALUES "
        Dim vData() As String
        vData = Split(data, "|")
        Dim sValues As String
        If UBound(vData) < 13 Then Exit Function
        vData(1) = vData(3) 'Confirms = Height since it is dynamic
        vData(11) = Replace(vData(11), "'", "")
        vData(11) = Replace(vData(11), Chr(0), "")
        For x = 0 To UBound(vData)
            sValues = sValues + "'" + vData(x) + "',"
        Next
        If Len(sValues) > 1 Then sValues = Left(sValues, Len(sValues) - 1)
        sql = sql + "(" + sValues + ")"
        If Val(vData(3)) > 0 Then mlSqlBestBlock = Val(vData(3))

        Exec(sql)
    End Function
    Public Function HighBlockNumber() As Long

        If KeyValue("UpdatingLeaderboard") = "true" Then Return mlSqlBestBlock

        Try
            mSql = "Select max(height) as HighBlock from Blocks"
            Dim lBlock As Long
            lBlock = Val(QueryFirstRow(mSql, "HighBlock"))
            mlSqlBestBlock = Val(lBlock)
            Return lBlock
        Catch ex As Exception
            Log(ex.Message)
            Return 0
        End Try
    End Function
    Public Function QueryFirstRow(ByVal sSql As String, sCol As String) As Object
        Dim o As Object

        Try
            Dim gr As New GridcoinReader
            gr = GetGridcoinReader(sSql, 10)

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
        sDatabaseName = "gridcoin"

    End Sub

    Public Sub New(strDatabaseName As String)
        sDatabaseName = strDatabaseName

        If bClean = False Then SqlHousecleaning()
    End Sub

    Public Sub Close()

    End Sub
    Protected Overrides Sub Finalize()
        Close()
        MyBase.Finalize()
    End Sub


    Public Sub CreateTable(sTableName As String, sFields As String, sFieldTypes As String)
        'Add on the Added,Updated,AddedBy, UpdatedBy, Id, Hash, LastHash fields
        Dim sAdhoc As String = ""
        mSql = "Create table " + sTableName + " (id integer primary key, Added datetime,AddedBy varchar(100), Updated datetime, UpdatedBy varchar(100),"
        Dim vFields() As String
        vFields = Split(sFields, ",")
        Dim vFieldTypes() As String
        vFieldTypes = Split(sFieldTypes, ",")
        For x = 0 To UBound(vFields)
            sAdhoc += vFields(x) + " " + vFieldTypes(x) + ", "

        Next x
        sAdhoc = Left(sAdhoc, Len(sAdhoc) - 1)
        mSql += sAdhoc
        Exec(mSql)

    End Sub


End Class
