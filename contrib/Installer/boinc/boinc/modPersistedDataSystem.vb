
Imports Microsoft.VisualBasic

Module modPersistedDataSystem

    'Rob Halford
    '6-6-2015
    'Add ability to persist off chain data from the gridcoin neural network, with primary keys, data added date, data expiration date, and to support contracts
    'The system must be fast, preserve data integrity, and must come to a true consensus with other nodes
    'When the node shuts down, the neural network needs to save its state
    'It must have the ability to restore state upon startup

    'Store data on the file system, with base64 encoding, and use the primary key as the filename for fast indexing

    'Stale data older than the Expiration shall be purged automatically

    'Data is stored across all Gridcoin windows nodes, in a decentralized store

    Private lUseCount As Long = 0
    Public Structure Row
        Public Database As String
        Public Table As String
        Public PrimaryKey As String
        Public Added As Date
        Public Expiration As Date
        Public Synced As Date
        Public DataColumn1 As String
        Public DataColumn2 As String
        Public DataColumn3 As String
        Public DataColumn4 As String
        Public DataColumn5 As String
        Public Magnitude As String
        Public RAC As String

    End Structure
    Public Structure CPID
        Public cpid As String
        Public RAC As Double
        Public Magnitude As Double
        Public Expiration As Date

    End Structure
    Public msCurrentNeuralHash = ""

    Public Function GetMagnitudeContractDetails() As String
        Dim surrogateRow As New Row
        surrogateRow.Database = "CPID"
        surrogateRow.Table = "CPIDS"
        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim sOut As String = ""
        For Each cpid As Row In lstCPIDs
            Dim r As Double = RoundedMag(201.11)

            Dim sRow As String = cpid.PrimaryKey + "," + Trim(RoundedMag(Val(cpid.Magnitude))) _
                                 + "," + Trim(Math.Round(Val(cpid.RAC), 2)) + "," + Trim(cpid.Expiration) + "," + Trim(cpid.Synced) + "," + Trim(cpid.DataColumn4) + "," + Trim(cpid.DataColumn5) + ";"
            sOut += sRow
        Next
        Return sOut
    End Function

    Public Function GetMagnitudeContract() As String
        Dim surrogateRow As New Row
        surrogateRow.Database = "CPID"
        surrogateRow.Table = "CPIDS"
        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim sOut As String = ""
        For Each cpid As Row In lstCPIDs
            If cpid.DataColumn5 = "True" Then
                If CDate(cpid.Added) < DateAdd(DateInterval.Day, -1, Now) Then
                    Dim sRow As String = cpid.PrimaryKey + "," + Trim(RoundedMag(Val(cpid.Magnitude))) + ";"
                    sOut += sRow
                End If

            End If

        Next
        Return sOut
    End Function
    Public Function RoundedMag(num As Double)
        'Rounds magnitude to nearest 10
        Return Math.Round(num * 0.2, 0) / 0.2
    End Function
    Public Function UpdateNetworkAverages() As Boolean
        'loop through all projects on file, persist network averages
        'Get a collection of Projects
        Dim surrogateRow As New Row
        surrogateRow.Database = "Project"
        surrogateRow.Table = "Projects"
        Dim lstProjects As List(Of Row) = GetList(surrogateRow, "*")
        Dim lstProjectCPIDs As List(Of Row)
        Dim units As Double = 0
        For Each prj As Row In lstProjects
            'Tally all of the RAC in this project
            'Get the RAC container
            surrogateRow = New Row
            surrogateRow.Database = "Project"
            surrogateRow.Table = prj.PrimaryKey + "cpid"
            lstProjectCPIDs = GetList(surrogateRow, "*")
            Dim prjTotalRAC = 0
            units = 0
            For Each prjCPID As Row In lstProjectCPIDs
                If Val(prjCPID.RAC) > 99 Then
                    prjTotalRAC += Val(prjCPID.RAC)
                    units += 1
                End If
            Next
            'Persist this total
            prj.Database = "Project"
            prj.Table = "Projects"
            prj.RAC = Trim(prjTotalRAC / (units + 0.01))
            Store(prj)
        Next
        Return True

    End Function
    Public Sub SyncDPOR2()
        Dim t As New Threading.Thread(AddressOf CompleteSync)
        t.Start()

    End Sub
    Public Function NeedsSynced(dr As Row) As Boolean

        If Now > dr.Synced Then Return True Else Return False
    End Function
    Public Sub CompleteSync()
        UpdateMagnitudes()
    End Sub
    Public Function UpdateMagnitudes() As Boolean
        'Loop through the researchers
        Dim surrogateRow As New Row
        surrogateRow.Database = "CPID"
        surrogateRow.Table = "CPIDS"

        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        Dim TotalRAC As Double = 0
        Dim lstProjectsCt As List(Of Row) = GetList(surrogateRow, "*")

        Dim WhitelistedProjects As Double = lstProjectsCt.Count
        Dim WhitelistedWithRAC As Double = lstProjectsCt.Count
        For Each cpid As Row In lstCPIDs
            If NeedsSynced(cpid) Then
                Dim bResult As Boolean = GetRACViaNetsoft(cpid.PrimaryKey)
                If bResult Then
                    cpid.Expiration = DateAdd(DateInterval.Day, 14, Now)
                    cpid.Synced = Tomorrow()
                    Store(cpid)
                End If
            End If
        Next
        UpdateNetworkAverages()
        lstCPIDs = GetList(surrogateRow, "*")

        For Each cpid As Row In lstCPIDs
            Dim surrogatePrj As New Row
            surrogatePrj.Database = "Project"
            surrogatePrj.Table = "Projects"
            Dim lstProjects As List(Of Row) = GetList(surrogatePrj, "*")
            TotalRAC = 0
            For Each prj As Row In lstProjects
                Dim surrogatePrjCPID As New Row
                surrogatePrjCPID.Database = "Project"
                surrogatePrjCPID.Table = prj.PrimaryKey + "CPID"
                surrogatePrjCPID.PrimaryKey = prj.PrimaryKey + "_" + cpid.PrimaryKey
                Dim rowRAC = Read(surrogatePrjCPID)
                Dim CPIDRAC As Double = Val(rowRAC.RAC)
                Dim PrjRAC As Double = Val(prj.RAC)
                Dim avgRac As Double = CPIDRAC / (PrjRAC + 0.01) * 100
                TotalRAC += avgRac
            Next
            'Now we can store the magnitude
            Dim Magg As Double = (TotalRAC / WhitelistedProjects) * WhitelistedWithRAC
            cpid.Database = "CPID"
            cpid.Table = "CPIDS"
            cpid.RAC = Trim(TotalRAC)
            cpid.Magnitude = Trim(Math.Round(Magg, 2))
            Store(cpid)
        Next
        Return True

    End Function
    Public Function GetList(DataRow As Row, sWildcard As String) As List(Of Row)
        Dim sPath As String = GetPath(DataRow)
        Dim sTemp As String = ""
        Dim d As String = "<COL>"
        Dim x As New List(Of Row)
        If System.IO.File.Exists(sPath) Then
            Using objReader As New System.IO.StreamReader(sPath)
                While objReader.EndOfStream = False
                    sTemp = objReader.ReadLine
                    Dim r As Row = DataToRow(sTemp)
                    r.Database = DataRow.Database
                    r.Table = DataRow.Table
                    If LCase(r.PrimaryKey) Like LCase(sWildcard) Then
                        x.Add(r)
                    End If
                End While
                objReader.Close()
            End Using

        End If
        Return x
    End Function
    Public Function GetRACViaNetsoft(sCPID As String) As Boolean

        If sCPID = "" Then Return False
        msCurrentNeuralHash = ""

        Dim sURL As String = "http://boinc.netsoft-online.com/get_user.php?cpid=" + sCPID

        Dim w As New MyWebClient2
        Dim sData As String = w.DownloadString(sURL)
        'Push all team gridcoin rac in
        Dim vData() As String
        vData = Split(sData, "<project>")
        Dim sName As String
        Dim Rac As Double
        Dim Team As String

        If InStr(1, sData, "<error>") > 0 Then
            Return False
        End If
        For y As Integer = 0 To UBound(vData)
            'For each project
            sName = ExtractXML(vData(y), "<name>", "</name>")
            Rac = ExtractXML(vData(y), "<expavg_credit>", "</expavg_credit>")
            Team = LCase(Trim(ExtractXML(vData(y), "<team_name>", "</team_name>")))
            'Store the :  PROJECT_CPID, RAC
            If Rac > 99 And Team = "gridcoin" Then
                PersistProjectRAC(sCPID, Rac, sName)
            End If
        Next y

        Return True


    End Function
    Public Function Tomorrow() As Date
        Dim dt As Date = DateAdd(DateInterval.Day, 1, Now)
        Dim dtTomorrow As Date = New Date(Year(dt), Month(dt), Day(dt), 6, 0, 0)
        Return dtTomorrow

    End Function
    Public Function PersistProjectRAC(sCPID As String, rac As Double, Project As String) As Boolean
        'Store the CPID_PROJECT RAC:
        Dim d As New Row
        d.Expiration = Tomorrow()
        d.Added = Now
        d.Database = "Project"
        d.Table = Project + "CPID"
        d.PrimaryKey = Project + "_" + sCPID
        d.RAC = Trim(rac)
        Store(d)
        'Store the Project record
        d = New Row
        d.Expiration = Tomorrow()
        d.Added = Now
        d.Database = "Project"
        d.Table = "Projects"
        d.PrimaryKey = Project
        d.DataColumn1 = Project
        Store(d)
        d = New Row

        'd.Expiration = Tomorrow()
        'd.Added = Now
        'd.Database = "CPID"
        'd.Table = "CPIDS"
        'd.PrimaryKey = sCPID
        'Store(d)
        Return True
    End Function


    Public Function ExtractXML2(sData As String, sStartKey As String, sEndKey As String)
        Dim iPos1 As Integer = InStr(1, sData, sStartKey)
        iPos1 = iPos1 + Len(sStartKey)
        Dim iPos2 As Integer = InStr(iPos1, sData, sEndKey)
        iPos2 = iPos2
        If iPos2 = 0 Then Return ""
        Dim sOut As String = Mid(sData, iPos1, iPos2 - iPos1)
        Return sOut
    End Function
    Public Function SerializeDate(dt As Date) As String
        Dim sOut As String = Trim(Year(dt)) + "/" + Trim(Month(dt)) + "/" + Trim(Day(dt)) + "/" + Trim(Hour(dt)) + "/" + Trim(Minute(dt)) + "/" + Trim(Second(dt))
        Return sOut
    End Function
    Public Function DeserializeDate(s As String) As Date
        Dim vDate() As String
        vDate = Split(s, "/")
        Dim dtOut As Date
        dtOut = New Date(Val(vDate(0)), Val(vDate(1)), Val(vDate(2)), Val(vDate(3)), Val(vDate(4)), Val(vDate(5)))

        Return dtOut

    End Function
    Public Function DataToRow(sData As String) As Row
        Dim vData() As String
        Dim d As String = "<COL>"

        vData = Split(sData, d)
        Dim r As New Row
        r.Added = DeserializeDate(vData(0))
        r.Expiration = DeserializeDate(vData(1))
        r.Synced = DeserializeDate(vData(2))

        r.PrimaryKey = vData(3)

        r.DataColumn1 = vData(4)
        r.DataColumn2 = vData(5)
        r.DataColumn3 = vData(6)
        r.DataColumn4 = vData(7)
        r.DataColumn5 = vData(8)
        r.Magnitude = vData(9)
        r.RAC = vData(10)

        Return r

    End Function
    Public Function GetCPID(cpid As String) As CPID
        Dim d As New Row
        d.Database = "CPID"
        d.Table = "CPIDS"
        d.PrimaryKey = cpid
        d = Read(d)
        Dim c As New CPID
        If d.Expiration > Now Then
            c.cpid = cpid
            c.Magnitude = Val(d.Magnitude)
            c.RAC = Val(d.RAC)
            c.Expiration = d.Expiration
            Return c
        Else
            c.Expiration = Nothing
            Return c
        End If

    End Function
    Public Function Read(dataRow As Row) As Row
        Dim sPath As String = GetPath(dataRow)
        Dim sTemp As String = ""
        Dim d As String = "<COL>"
        If System.IO.File.Exists(sPath) Then

            Using objReader As New System.IO.StreamReader(sPath)
                While objReader.EndOfStream = False
                    sTemp = objReader.ReadLine
                    Dim r As Row = DataToRow(sTemp)
                    r.Database = dataRow.Database
                    r.Table = dataRow.Table

                    If LCase(dataRow.PrimaryKey) = LCase(r.PrimaryKey) Then
                        Return r
                    End If
                End While
                objReader.Close()
            End Using

        End If
        Return dataRow

    End Function


    Public Function Insert(dataRow As Row, bOnlyWriteIfNotFound As Boolean)

        Dim sPath As String = GetPath(dataRow)
        Dim sWritePath As String = sPath + ".backup"
        Dim sTemp As String = ""
        Dim d As String = "<COL>"
        Dim bFound As Boolean
        If dataRow.Added = New Date Then dataRow.Added = Now

        
        Using objWriter As New System.IO.StreamWriter(sWritePath)

            If System.IO.File.Exists(sPath) Then
                Dim stream As New System.IO.FileStream(sPath, IO.FileMode.Open)

                Using objReader As New System.IO.StreamReader(stream)

                    While objReader.EndOfStream = False
                        sTemp = objReader.ReadLine
                        Dim r As Row = DataToRow(sTemp)
                        If r.Expiration < Now Then
                            'Purge
                            Dim sExpired As String = ""
                        Else
                            If LCase(dataRow.PrimaryKey) <> LCase(r.PrimaryKey) Then
                                Dim sNewData As String = SerializeRow(r)
                                objWriter.WriteLine(sNewData)
                            Else
                                bFound = True
                            End If
                        End If
                    End While
                    objReader.Close()
                End Using

                Try
                    Kill(sPath)

                Catch ex As Exception

                End Try

            End If
            If (bOnlyWriteIfNotFound And Not bFound) Or Not bOnlyWriteIfNotFound Then
                objWriter.WriteLine(SerializeRow(dataRow))
            End If

            objWriter.Close()

        End Using

        FileCopy(sWritePath, sPath)
        Kill(sWritePath)
        Return True

    End Function
    Public Function SerializeRow(dataRow As Row) As String
        Dim d As String = "<COL>"
       
        Dim sSerialized As String = SerializeDate(dataRow.Added) + d + SerializeDate(dataRow.Expiration) _
            + d + SerializeDate(dataRow.Synced) + d + LCase(dataRow.PrimaryKey) + d + dataRow.DataColumn1 + d + dataRow.DataColumn2 _
            + d + dataRow.DataColumn3 + d + dataRow.DataColumn4 + d + dataRow.DataColumn5 + d + dataRow.Magnitude + d + dataRow.RAC

        Return sSerialized
    End Function


    Public Function GetPath(dataRow As Row) As String
        Dim sFilename As String = LCase(dataRow.Database) + "_" + LCase(dataRow.Table) + ".dat"
        Dim sPath As String = GetGridFolder() + "NeuralNetwork\"
        If Not System.IO.Directory.Exists(sPath) Then
            Try
                MkDir(sPath)

            Catch ex As Exception
                Log("Unable to create neural network directory " + sPath)
                Return False
            End Try
        End If
        Return sPath + sFilename
    End Function
    Public Function Store(dataRow As Row) As Boolean
        '       Dim sPath As String = GetPath(dataRow)
        Insert(dataRow, False)
        Return True
    End Function
    Public Function GetMd5String2(ByVal sData As String) As String
        Try
            Dim objMD5 As New System.Security.Cryptography.MD5CryptoServiceProvider
            Dim arrData() As Byte
            Dim arrHash() As Byte
            arrData = System.Text.Encoding.UTF8.GetBytes(sData)
            arrHash = objMD5.ComputeHash(arrData)
            objMD5 = Nothing
            Dim sOut As String = ByteArrayToHexString2(arrHash)
            Return sOut

        Catch ex As Exception
            Return "MD5Error"
        End Try
    End Function

    Private Function ByteArrayToHexString2(ByVal arrInput() As Byte) As String
        Dim strOutput As New System.Text.StringBuilder(arrInput.Length)
        For i As Integer = 0 To arrInput.Length - 1
            strOutput.Append(arrInput(i).ToString("X2"))
        Next
        Return strOutput.ToString().ToLower
    End Function

End Module

Public Class MyWebClient2
    Inherits System.Net.WebClient
    Private timeout As Long = 12000
    Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
        Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
        w.Timeout = timeout
        Return (w)
    End Function
End Class