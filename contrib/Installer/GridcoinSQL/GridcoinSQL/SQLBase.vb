
Imports System
Imports System.Collections.Generic
Imports System.Web
Imports System.Data
Imports System.Data.SqlClient
Imports System.Configuration.ConfigurationManager
Imports System.Xml
Imports System.IO
Imports System.Security.Cryptography

Public Module SqlBasePublicVar
    Public db As SQLBase
    Public dr As SqlDataReader
    
    Public str As String
    Public sOut As String
    Public cr As String = vbCrLf
    Public modDR As SqlDataReader
    Public SqlConnection As SqlConnection
    Public mSql As String

End Module

Public Class SQLBase
    Public x As Long = 0
    Public sql As String = ""

    Public sSQLConn As String
    Dim dtConsensusStartDate As Date
    Dim dtConsensusEndDate As Date



    Private bIsDisposed As Boolean = False

    Public Sub OpenNewConnection()
        If SqlConnection Is Nothing Then
            SqlConnection = New SqlConnection(sSQLConn)

        ElseIf SqlConnection.State <> ConnectionState.Open Then
            SqlConnection = New SqlConnection(sSQLConn)

        End If
        Try
            SqlConnection.Open()

        Catch ex As Exception
            Try
                SqlConnection.Open()

            Catch exs As Exception

            End Try
        End Try

    End Sub
    Public Sub New()
        sSQLConn = "Server=" + AppSettings("DatabaseHost").ToString() + ";" & _
                   "Database=" + AppSettings("DatabaseName").ToString() + ";MultipleActiveResultSets=true;" & _
                   "Uid=" + AppSettings("DatabaseUser") + ";pwd=" + AppSettings("DatabasePass")
        OpenNewConnection()
        'Add base tables
        AddBaseTables()
    End Sub

    Public Sub New(sDBServer As String)
        sSQLConn = "Server=" + AppSettings("DatabaseHost").ToString() + ";" & _
                   "Database=" + sDBServer + ";MultipleActiveResultSets=true;" & _
                   "Uid=" + AppSettings("DatabaseUser") + ";pwd=" + AppSettings("DatabasePass")
        OpenNewConnection()
        'Add base tables
        AddBaseTables()
    End Sub
    Public Sub close()
        SqlConnection.Close()
    End Sub
    Public Sub Exec(ByVal sql As String)
        If SqlConnection.State <> ConnectionState.Open Then
            SqlConnection.Open()
        End If
        Dim myCommand As New SqlCommand(sql, SqlConnection)
        myCommand.ExecuteNonQuery()

    End Sub
    Public Function Read(ByVal sql As String, ByRef myDataReader As SqlDataReader) As Boolean
        If Not myDataReader Is Nothing Then
            If myDataReader.IsClosed = False Then
                myDataReader.Close()
            End If
        End If
        Dim myCommand As New SqlCommand(sql, SqlConnection)
        myDataReader = myCommand.ExecuteReader()
        Return myDataReader.HasRows
    End Function
    Public Function GetDataTable(sql As String) As System.Data.DataTable
        Using SqlConnection
            Using cmd As New SqlCommand(sql, SqlConnection)
                'sqlConn.Open()
                Dim dt As New DataTable()
                dt.Load(cmd.ExecuteReader())
                Return dt
            End Using
        End Using
    End Function
    Public Function Read(ByVal sql As String) As SqlDataReader
        '''

        Dim myReader As SqlDataReader
        Dim myCommand As New SqlCommand(sql, SqlConnection)
        myCommand.Cancel()

        If Not myReader Is Nothing Then
            If myReader.IsClosed = False Then
                myReader.Close()
            End If
        End If

        myCommand.CommandTimeout = 100000

        If Not SqlConnection Is Nothing Then
            If SqlConnection.State = ConnectionState.Closed Then
                OpenNewConnection()
                myCommand = New SqlCommand(sql, SqlConnection)


            End If
        End If

        myReader = myCommand.ExecuteReader()
        Return myReader
    End Function
    Public Function ReadFirstRow(ByVal sql As String, vCol As Object) As String
        'You may pass the column Ordinal or the Column Name as vCol
        Try

            Dim myCommand As SqlCommand = New SqlCommand(sql, SqlConnection)
            Using myCommand
                dr = myCommand.ExecuteReader()
                If Not dr.HasRows Or dr.FieldCount = 0 Then
                    dr.Close() : Return ""
                End If

                While dr.Read
                    Dim sOut As String = dr(vCol).ToString()
                    dr.Close()

                    Return sOut
                End While

            End Using
            Exit Function

        Catch ex As Exception
            Return "-1"

        End Try

    End Function
    Protected Sub Dispose(ByVal disposing As Boolean)
        If disposing Then
            SqlConnection.Close()
            bIsDisposed = True
        End If
    End Sub


    Public Function Scalar(sQuery As String) As Double
        Try
            x = Val(Me.ReadFirstRow(sQuery, 0))

        Catch ex As Exception
            x = -1
        End Try
        Return x


    End Function

    Public Sub CreateTable(sTableName As String, sFields As String, sFieldTypes As String)
        'If the table exists, leave it
        mSql = "Select count(*) from " + sTableName
        x = Scalar(mSql)
        If x >= 0 Then Exit Sub
        sFieldTypes = Replace(sFieldTypes, "(1,0)", "(1`0)")


        'Add on the Added,Updated,AddedBy, UpdatedBy, Id, Hash, LastHash fields
        Dim sAdhoc As String = ""
        mSql = "Create table " + sTableName + " (id uniqueidentifier,Organization uniqueidentifier,Hash varchar(100), LastHash varchar(100), Added datetime, " _
            & "AddedBy varchar(100), Updated datetime, " _
            & "UpdatedBy varchar(100),Deleted numeric(1,0),ConsensusRound uniqueidentifier,"
        Dim vFields() As String
        vFields = Split(sFields, ",")
        Dim vFieldTypes() As String
        vFieldTypes = Split(sFieldTypes, ",")
        For x = 0 To UBound(vFields)
            vFieldTypes(x) = Replace(vFieldTypes(x), "(1`0)", "(1,0)")

            sAdhoc += vFields(x) + " " + vFieldTypes(x) + ","
        Next x
        sAdhoc = Left(sAdhoc, Len(sAdhoc) - 1)
        sAdhoc += ")"

        mSql += sAdhoc
        Exec(mSql)

        'Add the table to the system table
        InsertRecord("System", "sCategory,sKey,sValue", "'Table','SysTables','" + sTableName + "'")



    End Sub

    Public Function DropTable(sTable As String)
        mSql = "DROP TABLE " + sTable
        Exec(mSql)

    End Function
    Public Sub AddBaseTables()
        'Ensure System, Consensus, Nodes exist:
        CreateTable("System", "sCategory,sKey,sValue", "varchar(100),varchar(100),varchar(100)")

        CreateTable("Node", "host,LastSeen,Master", "varchar(50),datetime,numeric(1,0)")
        CreateTable("Consensus", "Round,Started,Ended,ConsensusHash,Tables", "uniqueidentifier,datetime,datetime,varchar(100),varchar(1500)")
        CreateTable("Organization", "Name,City", "varchar(100),varchar(100)")
        CreateTable("Confirm", "GRCFrom,GRCTo,txid,amount,confirmed", "varchar(100),varchar(100),varchar(100),money,numeric(1,0)")

    End Sub
    Public Sub DropBaseTables()
        DropTable("System")

        DropTable("Node")
        DropTable("Consensus")
        DropTable("Organization")
        DropTable("COnfirm")

    End Sub
    Public Function DataToFile(sData As String, sFileName As String)
        Dim rootPath As String = Directory.GetCurrentDirectory() & "\WWWRoot\"
        Dim sPath As String = rootPath + sFileName
        Dim objWriter As New System.IO.StreamWriter(sPath)
        objWriter.Write(sData)
        objWriter.Close()
    End Function

    Public Function TableToData(sTable As String, sStart As String, sEnd As String) As String

        mSql = "Select * from " + sTable + " WHERE ADDED between '" + Trim(sStart) + "' and '" + Trim(sEnd) + "' Order by added"

        dr = Me.Read(mSql)
        If dr.HasRows = False Then Exit Function
        Dim sHeader As String
        For x = 0 To dr.FieldCount - 1
            'Dim dc As DataColumn
            ' dc = dr.GetSchemaTable.Columns(CInt(x))
            Dim sCol As String
            sCol = dr.GetName(x) + "<~>" + dr.GetFieldType(x).ToString

            sHeader += sCol + "<|>"
        Next
        Dim sRow As String

        Dim sOut As String
        sOut += sHeader + vbCrLf

        While dr.Read
            sRow = ""
            For x = 0 To dr.FieldCount - 1
                Dim sData As String
                sData = dr(CInt(x)).ToString()
                sRow += sData + "<|>"

            Next x
            sRow += vbCrLf
            sOut += sRow
        End While
        Return sOut

    End Function
    Public Function GetLastHash(sTable As String) As String
        mSql = "Select top 1 hash from " + sTable + " where added < getdate() order by added desc"


        Dim sLast As String
        sLast = ReadFirstRow(mSql, 0)

        If sLast = "" Then sLast = Sha1("")


        Return sLast

    End Function
    Public Function InsertRecord(sTable As String, sFields As String, sValues As String) As Boolean
        'Get the last hash inserted before this record
        Dim sLastHash As String = GetLastHash(sTable)

        Dim sAddedby As String
        Dim sUpdatedBy As String
        sAddedby = "SYS" : sUpdatedBy = "SYS"
        Dim sOrg As String
        sOrg = "F0FB9E21-0EDE-4AD4-8F88-EFE3BB917481"

        Dim sStandardFields As String
        Dim sHash As String

        sStandardFields = "id,Organization,added,addedby,updated,updatedby,deleted,Hash,LastHash"
        Dim sStandardValues As String

        'Row Hash = User Fields hashed
        Dim vValues() As String
        vValues = Split(sValues, ",")

        Dim dHash As Double
        Dim sSqlValues As String
        
        Dim sId As String
        sId = Guid.NewGuid().ToString()
        dHash += sId.ToString().GetHashCode



        For x = 0 To UBound(vValues)
            dHash += vValues(x).GetHashCode()
            sSqlValues += "" + vValues(x) + ","
        Next
        sSqlValues = Left(sSqlValues, Len(sSqlValues) - 1)
        sHash = Sha1(Trim(dHash))


        sStandardValues = "newid(),'" + sOrg + "',getdate(),'" + sAddedby + "',getdate(),'" + sUpdatedBy + "',0,'" + Trim(sHash) + "','" + Trim(sLastHash) + "'"

        mSql = "INSERT INTO " + sTable + "(" + sFields + "," + sStandardFields + ") values (" + sSqlValues + "," + sStandardValues + ")"

        Exec(mSql)

    End Function
    Public Function GetConsensusDatePair()
        mSql = "Select max(ConsensusEndDate) from Consensus "
        Dim sConsStart
        sConsStart = Me.ReadFirstRow(mSql, 0)
        If sConsStart = "-1" Then dtConsensusStartDate = CDate("1-1-2000")
        dtConsensusEndDate = Now
    End Function
    'Start a new consensus Round
    Public Function StartConsensusRound()
        'Add a row to consensus
        'What tables will be assimilated ?
        Dim sTableList As String = ""

        Dim gConsensusRound As String

        gConsensusRound = Guid.NewGuid.ToString()
        GetConsensusDatePair()


        mSql = "Select sValue from System where scategory='table' and sKey = 'sysTables' order by sValue"
        dr = Read(mSql)
        Dim sConsensusHash As String
        Dim dHash As Double = 0
        Dim sTable As String = ""


        While dr.Read

            sTable = dr("sValue")

            sTableList += sTable + ","
            mSql = "Update " + sTable + " Set ConsensusRound = '" + gConsensusRound + "' where ConsensusRound is null and Added between '" _
                + Trim(dtConsensusStartDate) + "' and '" + Trim(dtConsensusEndDAte) + "'"

            Exec(mSql)

            mSql = "Select * from " + sTable + " WHERE ConsensusRound = '" + Trim(gConsensusRound) + "'"
            'Get the Hash for the rows
            Dim drHashes As SqlDataReader
            drHashes = Read(mSql)

            While drHashes.Read
                dHash += drHashes("hash").GetHashCode
            End While


        End While
        'Add the consensus Record
        sConsensusHash = Sha1(Trim(dHash))
        sTableList = Left(sTableList, Len(sTableList) - 1)

        InsertRecord("Consensus", "Round,Started,Ended,ConsensusHash,Tables", "newid(),'" + Trim(dtConsensusStartDate) + "','" _
                     + Trim(dtConsensusEndDAte) + "','" + sConsensusHash + "','" + sTableList + "'")


    End Function

    Public Function Sha1(s As String) As String

        Dim sha As New HMACSHA1

        sha.Key = System.Text.Encoding.UTF8.GetBytes("GRC")

        Dim hashValue As Byte() = sha.ComputeHash(System.Text.Encoding.UTF8.GetBytes(s))
        Dim sHex As String = ByteArrayToHexString(hashValue)
        Return sHex

    End Function
    Private Function ByteArrayToHexString(ByVal arrInput() As Byte) As String
        Dim strOutput As New System.Text.StringBuilder(arrInput.Length)
        For i As Integer = 0 To arrInput.Length - 1
            strOutput.Append(arrInput(i).ToString("X2"))
        Next
        Return strOutput.ToString().ToLower
    End Function
    'Ask for Consensus between Last consensus and Now
    Public Function GetConsensusFromMaster()
        GetConsensusDatePair()
        'HTTP Request from Master
        Dim sData As String = GetData("Consensus")
        Replicate(sData, "Consensus")


    End Function
    Public Function Snip(sData As String)
        If Len(sData) > 0 Then
            sData = Left(sData, Len(sData) - 1)
        End If


        Return sData

    End Function
    Public Function Replicate(data As String, sTable As String)
        'Insert rows if they meet the spec and are missing
        Dim fields As String = ""

        Dim vData() As String
        vData = Split(data, vbCrLf)
        Dim vRow() As String
        vRow = Split(vData(0), "<|>")

        x = 0

        For x = 0 To UBound(vRow) - 1

            Dim sCol As String
            Dim sType As String
            Dim vCol() As String
            vCol = Split(vRow(x), "<~>")
            sCol = vCol(0)
            sType = vCol(1)

            fields = fields + sCol + ","
        Next
        fields = Snip(fields)
        Dim sInsert As String

        For x = 1 To vData.Length - 1


            Dim sRow As String
            sRow = vData(x)
            vRow = Split(sRow, "<|>")
            sInsert = ""

            For y = 0 To UBound(vRow) - 1
                Dim sValue As String
                sValue = "'" + Trim(vRow(y)) + "'"
                If sValue = "''" Then sValue = "null"
                sInsert = sInsert + sValue + ","

            Next
            sInsert = Snip(sInsert)

            Dim sSql As String
            sSql = "Insert into " + sTable + " (" + fields + " ) values (" + sInsert + ")"

            If Len(sInsert) > 1 Then
                Me.Exec(sSql)
            End If


        Next x
    End Function
    Public Function GetMasterNode()
        sql = "Select Host From Node where master=-1 and deleted = 0"
        Dim sHost As String
        sHost = ReadFirstRow(sql, "Host")
        If sHost = "" Then
            'No master is known, use the seed node
            sHost = "grid10:8080"
        End If
        Return sHost

    End Function
    Public Function GetData(sTable As String)
        'ToDo; ask for data from the Master - once a new master is chosen - master will be updated automatically
        Dim sMasterHost As String
        sMasterHost = GetMasterNode()

        Dim sURL As String = "http://" + sMasterHost + "?table=" + Trim(sTable) + "&startdate=" + Trim(dtConsensusStartDate) + "&enddate=" + Trim(dtConsensusEndDate)

        Dim w As New MyWebClient
        Dim sData As String
        sData = w.DownloadString(sURL)

        Return sData

    End Function


End Class

Public Class MyWebClient
    Inherits System.Net.WebClient
    Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
        Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
        w.Timeout = 47000
        Return w
    End Function
End Class