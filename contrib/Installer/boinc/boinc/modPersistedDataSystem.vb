
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
    Public msSyncData As String = ""
    Public mbForcefullySyncAllRac = False
    Public mbTestNet As Boolean = False
    'Minimum RAC percentage required for RAC to be counted in magnitude:
    Public mdMinimumRacPercentage As Double = 0.06
    Public bMagsDoneLoading As Boolean = True

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
        Dim lTotal As Long
        Dim lRows As Long
        If bMagsDoneLoading = False Then Return ""
        Dim sOut As String = ""
        sOut = "<MAGNITUDES>"
        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        For Each cpid As Row In lstCPIDs
            If cpid.DataColumn5 = "True" Then
                ' If CDate(cpid.Added) < DateAdd(DateInterval.Day, -1, Now) Then
                Dim sRow As String = cpid.PrimaryKey + "," + Trim(RoundedMag(Val(cpid.Magnitude))) + ";"
                lTotal = lTotal + Val(cpid.Magnitude)
                lRows = lRows + 1
                sOut += sRow
            End If
        Next
        sOut += "</MAGNITUDES><AVERAGES>"
        Dim avg As Double
        avg = lTotal / (lRows + 0.01)
        If avg < 25 Then Return ""
        'APPEND the Averages:
        Dim rRow As New Row
        rRow.Database = "Project"
        rRow.Table = "Projects"
        Dim lstP As List(Of Row) = GetList(rRow, "*")
        lstP.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        For Each r As Row In lstP
            If r.RAC > 0 Then
                Dim sRow As String = r.PrimaryKey + "," + Trim(RoundedMag(Val(r.RAC))) + ";"
                lRows = lRows + 1
                sOut += sRow
            End If
        Next
        sOut += "</AVERAGES>"
        Return sOut
    End Function
    Public Function xReservedForFuture(data As String)
        Dim sMags As String
        sMags = ExtractXML(data, "<MAGNITUDES>")
        Dim vMags() As String
        vMags = Split(sMags, ";")
        For x As Integer = 0 To UBound(vMags)
            If Len(vMags(x)) > 10 Then
                Dim vRow() As String = Split(vMags(x), ",")
                If Len(vRow(0)) > 5 Then
                    Dim sCPID As String = vRow(0)
                    Dim dMag As Double = Val(vRow(1))
                End If
            End If
        Next x
    End Function

    Public Function RoundedMag(num As Double)
        'Rounds magnitude to nearest Dither Factor
        Return Math.Round(Math.Round(num * GetDitherMag(num), 0) / GetDitherMag(num), 2)
    End Function
    Public Function RoundedRac(num As Double)
        'Rounds magnitude to nearest Dither Factor
        Return Math.Round(Math.Round(num * GetDitherMag(num), 0) / GetDitherMag(num), 2)
    End Function
    Public Function GetDitherMag(Data As Double) As Double
        Dim Dither As Double = 0.1
        If Data > 0 And Data < 25 Then Dither = 0.8 '1.25
        If Data >= 25 And Data < 500 Then Dither = 0.2 '5
        If Data >= 500 And Data <= 1000 Then Dither = 0.1 '10
        If Data >= 1000 And Data <= 10000 Then Dither = 0.025 '40
        If Data >= 10000 And Data <= 50000 Then Dither = 0.006 '160
        If Data >= 50000 And Data <= 100000 Then Dither = 0.003 ' 320
        If Data >= 100000 And Data <= 999999 Then Dither = 0.0015 ' 640
        If Data >= 1000000 Then Dither = 0.0007 '1428
        Return Dither
    End Function
    Public Function UpdateNetworkAverages() As Boolean
        'loop through all projects on file, persist network averages
        'Get a collection of Projects
        Try

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
                    If Val(prjCPID.RAC) > 10 Then
                        prjTotalRAC += Val(prjCPID.RAC)
                        units += 1
                    End If
                Next
                'Persist this total
                prj.Database = "Project"
                prj.Table = "Projects"
                prj.RAC = Trim(RoundedRac(prjTotalRAC / (units + 0.01)))
                Store(prj)
            Next
            Return True
        Catch ex As Exception
            Log("Update network averages: " + ex.Message)
            Return False

        End Try

    End Function
    Public Sub SyncDPOR2()
        Dim t As New Threading.Thread(AddressOf CompleteSync)
        t.Start()

    End Sub
    Public Function NeedsSynced(dr As Row) As Boolean

        If Now > dr.Synced Then Return True Else Return False
    End Function
    Public Sub CompleteSync()
        If bMagsDoneLoading = False Then Exit Sub

        mbForcefullySyncAllRac = True

        Try

        bMagsDoneLoading = False
        UpdateMagnitudesPhase1()
        UpdateMagnitudes()
        mbForcefullySyncAllRac = False

        If LCase(KeyValue("exportmagnitude")) = "true" Then
            ExportToCSV2()
            End If
        Catch ex As Exception
            Log("Completesync:" + ex.Message)
        End Try

        bMagsDoneLoading = True

    End Sub
    Private Function BlowAwayTable(dr As Row)
        Dim sPath As String = GetPath(dr)
        Try
            Kill(sPath)
        Catch ex As Exception
            Log("Neural updatemagnitudesphase1:" + ex.Message)
        End Try

    End Function
    Public Function UpdateMagnitudesPhase1()
        Try
            'Blow away the projects
            Dim surrogateRow1 As New Row

            surrogateRow1.Database = "Project"
            surrogateRow1.Table = "Projects"
            BlowAwayTable(surrogateRow1)

            surrogateRow1.Database = "Whitelist"
            surrogateRow1.Table = "Whitelist"
            BlowAwayTable(surrogateRow1)

            Dim sWhitelist As String
            sWhitelist = ExtractXML(msSyncData, "<WHITELIST>")
            Dim sCPIDData As String = ExtractXML(msSyncData, "<CPIDDATA>")
            Log(msSyncData)
            Log("")

            Log(sWhitelist)
            Log("")
            Log(sCPIDData)

            Try

            Dim vWhitelist() As String = Split(sWhitelist, "<ROW>")
            For x As Integer = 0 To UBound(vWhitelist)
                If Len(vWhitelist(x)) > 1 Then
                    Dim vRow() As String
                    vRow = Split(vWhitelist(x), "<COL>")
                    Dim sProject As String = vRow(0)
                    Dim sURL As String = vRow(1)
                    Dim dr As New Row
                    dr.Database = "Whitelist"
                    dr.Table = "Whitelist"
                    dr.PrimaryKey = sProject
                    dr = Read(dr)
                        dr.Expiration = DateAdd(DateInterval.Day, 14, Now)
                        dr.Synced = DateAdd(DateInterval.Day, -1, Now)
                        Store(dr)
                End If
            Next x
            Catch ex As Exception
                Log("while loading projects " + ex.Message)
            End Try

            Dim vCPIDs() As String = Split(sCPIDData, "<ROW>")
            Dim vTestNet() As String
            vTestNet = Split(vCPIDs(0), "<COL>")
            Log("Updating magnitude in testnet=" + Trim(mbTestNet) + " for " + Trim(UBound(vCPIDs)) + " cpids.")
            'Delete any CPIDs that are in the neural network that no longer have beacons:
            surrogateRow1.Database = "CPID"
            surrogateRow1.Table = "CPIDS"
            BlowAwayTable(surrogateRow1)

            For x As Integer = 0 To UBound(vCPIDs)
                If Len(vCPIDs(x)) > 20 Then
                    Dim vRow() As String
                    vRow = Split(vCPIDs(x), "<COL>")
                    Dim sCPID As String = vRow(0)
                    Dim sBase As String = vRow(1)
                    Dim unBase As String = DecodeBase64(sBase)
                    'contract = GlobalCPUMiningCPID.cpidv2 + ";" + hashRand.GetHex() + ";" + GRCAddress;
                    Dim vCPIDRow() As String = Split(unBase, ";")
                    Dim cpidv2 As String = vCPIDRow(0)
                    Dim BlockHash As String = vCPIDRow(1)
                    Dim Address As String = vCPIDRow(2)
                    Dim dr As New Row
                    dr.Database = "CPID"
                    dr.Table = "CPIDS"
                    dr.PrimaryKey = sCPID
                    dr = Read(dr)
                    If NeedsSynced(dr) Or mbForcefullySyncAllRac Then
                        dr.Expiration = DateAdd(DateInterval.Day, 14, Now)
                        dr.Synced = DateAdd(DateInterval.Day, -1, Now)
                        dr.DataColumn1 = cpidv2
                        dr.DataColumn3 = BlockHash
                        dr.DataColumn4 = Address
                        Dim bValid As Boolean = False
                        Dim clsMD5 As New MD5

                        bValid = clsMD5.CompareCPID(sCPID, cpidv2, BlockHash)
                        dr.DataColumn5 = Trim(bValid)
                        Store(dr)
                    End If
                End If
            Next
        Catch ex As Exception
            Log("UpdateMagnitudesPhase1: " + ex.Message)
        End Try

        msSyncData = ""

    End Function

    Public Function UpdateMagnitudes() As Boolean
        Dim lstCPIDs As List(Of Row)
        Dim surrogateRow As New Row
        Dim lstWhitelist As List(Of Row)
        Dim surrogateWhitelistRow As New Row
        surrogateWhitelistRow.Database = "Whitelist"
        surrogateWhitelistRow.Table = "Whitelist"
        lstWhitelist = GetList(surrogateWhitelistRow, "*")

        Dim WhitelistedProjects As Double = 0
        Dim WhitelistedWithRAC As Double = 0

        Try

            'Loop through the researchers
            surrogateRow.Database = "CPID"
            surrogateRow.Table = "CPIDS"
            lstCPIDs = GetList(surrogateRow, "*")
            lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
            Dim lstProjectsCt As List(Of Row) = GetList(surrogateRow, "*")
            WhitelistedProjects = lstProjectsCt.Count
            WhitelistedWithRAC = lstWhitelist.Count
            For Each cpid As Row In lstCPIDs
                If NeedsSynced(cpid) Or mbForcefullySyncAllRac Then
                    Dim bResult As Boolean = GetRacViaNetsoft(cpid.PrimaryKey)
                End If
            Next
        Catch ex As Exception
            Log("UpdateMagnitudes:GatherRAC: " + ex.Message)
        End Try
        Try
            UpdateNetworkAverages()
        Catch ex As Exception
            Log("UpdateMagnitudes:UpdateNetworkAverages: " + ex.Message)
        End Try

        Try

            lstCPIDs = GetList(surrogateRow, "*")
            For Each cpid As Row In lstCPIDs
                Dim surrogatePrj As New Row
                surrogatePrj.Database = "Project"
                surrogatePrj.Table = "Projects"
                Dim lstProjects As List(Of Row) = GetList(surrogatePrj, "*")
                Dim TotalAvgRAC As Double = 0
                'Dim TotalRAC As Double = 0
                For Each prj As Row In lstProjects
                    Dim surrogatePrjCPID As New Row

                    If IsInList(prj.PrimaryKey, lstWhitelist, False) Then
                        surrogatePrjCPID.Database = "Project"
                        surrogatePrjCPID.Table = prj.PrimaryKey + "CPID"
                        surrogatePrjCPID.PrimaryKey = prj.PrimaryKey + "_" + cpid.PrimaryKey
                        Dim rowRAC = Read(surrogatePrjCPID)
                        Dim CPIDRAC As Double = Val(rowRAC.RAC)
                        Dim PrjAvgRAC As Double = Val(prj.RAC)
                        Dim avgRac As Double = CPIDRAC / (PrjAvgRAC + 0.01) * 100
                        Dim MinRAC As Double = PrjAvgRAC * mdMinimumRacPercentage

                        If CPIDRAC > MinRAC Then
                            TotalAvgRAC += avgRac
                        Else
                            'Optional: Erase researchers RAC here (magnitude is already adjusted)
                        End If
                    End If
                Next
                'Now we can store the magnitude
                Dim Magg As Double = (TotalAvgRAC / WhitelistedProjects) * WhitelistedWithRAC
                cpid.Database = "CPID"
                cpid.Table = "CPIDS"
                cpid.Magnitude = Trim(Math.Round(Magg, 2))
                If Magg < 1 And Magg > 0.25 Then cpid.Magnitude = 1
                Store(cpid)
            Next
            Return True
        Catch ex As Exception
            Log("UpdateMagnitudes:StoreMagnitudes: " + ex.Message)
        End Try

    End Function
    Public Function IsInList(sData As String, lstRows As List(Of Row), bRequireExactMatch As Boolean) As Boolean
        For Each blah As Row In lstRows
            If Trim(UCase(blah.PrimaryKey)) = Trim(UCase(sData)) Then
                Return True
            End If
            If Not bRequireExactMatch Then
                Dim sCompare1 As String = Replace(Replace(Trim(UCase(blah.PrimaryKey)), " ", ""), "_", "")
                Dim sCompare2 As String = Replace(Replace(Trim(UCase(sData)), " ", ""), "_", "")
                If Left(sCompare1, 8) = Left(sCompare2, 8) Then Return True
            End If
        Next
        Return False
    End Function
    Public Function GetList(DataRow As Row, sWildcard As String) As List(Of Row)
        Dim xx As New List(Of Row)
        Dim sErr As String = ""

        For x As Integer = 1 To 10
            Try
                xx = GetList_Safe(DataRow, sWildcard)
                Return xx
            Catch ex As Exception
                sErr = ex.Message
                System.Threading.Thread.Sleep(2000)
            End Try
        Next

        Return xx

        Log("While asking for list in getlist of " + DataRow.PrimaryKey + ", " + sErr)

    End Function

    Public Function GetList_Safe(DataRow As Row, sWildcard As String) As List(Of Row)
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
    Public Function GetRacViaNetsoft(sCPID As String) As Boolean
        For x = 1 To 5
            Dim bResult As Boolean = GetRACViaNetsoft_Resilient(sCPID)
            If bResult Then Return bResult
        Next
        Return False
    End Function
    Public Function GetXMLOnly(sCPID As String)

        If sCPID = "" Then Return False

        Try

            Dim sURL As String = "http://boinc.netsoft-online.com/get_user.php?cpid=" + sCPID
            Dim w As New MyWebClient2
            Dim sData As String = w.DownloadString(sURL)
            Return sData

        Catch

        End Try

    End Function
    Public Function GetRACViaNetsoft_Resilient(sCPID As String) As Boolean

        If sCPID = "" Then Return False
        msCurrentNeuralHash = ""

        Try

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
            Dim TotalRAC As Double = 0
            For y As Integer = 0 To UBound(vData)
                'For each project
                sName = ExtractXML(vData(y), "<name>", "</name>")
                Rac = RoundedRac(Val(ExtractXML(vData(y), "<expavg_credit>", "</expavg_credit>")))
                Team = LCase(Trim(ExtractXML(vData(y), "<team_name>", "</team_name>")))
                'Store the :  PROJECT_CPID, RAC
                If Rac > 10 And Team = "gridcoin" Then
                    PersistProjectRAC(sCPID, Rac, sName)
                    TotalRAC += Rac
                End If
            Next y


            Dim d As Row = New Row
            d.Expiration = Tomorrow()
            d.Added = Now
            d.Database = "CPID"
            d.Table = "CPIDS"
            d.PrimaryKey = sCPID
            d = Read(d)
            d.Expiration = DateAdd(DateInterval.Day, 14, Now)
            d.Synced = Tomorrow()
           
            d.RAC = TotalRac
            Store(d)
            Return True

        Catch ex As Exception
            Return False
        End Try


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
        d.RAC = Trim(RoundedRac(rac))
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
        If s = "" Then Return CDate("1-1-1900")

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

        If UBound(vData) >= 4 Then r.DataColumn1 = "" & vData(4)
        If UBound(vData) >= 5 Then r.DataColumn2 = "" & vData(5)
        If UBound(vData) >= 6 Then r.DataColumn3 = "" & vData(6)
        If UBound(vData) >= 7 Then r.DataColumn4 = "" & vData(7)
        If UBound(vData) >= 8 Then r.DataColumn5 = "" & vData(8)
        If UBound(vData) >= 9 Then r.Magnitude = "" & vData(9)
        If UBound(vData) >= 10 Then r.RAC = "" & vData(10)

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
        Dim oRow As Row
        Dim sErr As String
        For x As Integer = 1 To 10
            Try
                oRow = Read_Surrogate(dataRow)
                Return oRow
            Catch ex As Exception
                sErr = ex.Message
                System.Threading.Thread.Sleep(1000)
            End Try
        Next
        Log("Read: " + sErr)
        Return dataRow

    End Function

    Public Function Read_Surrogate(dataRow As Row) As Row
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
    Public Function SerStr(sData As String) As String
        sData = "" & sData
        Return sData
    End Function
    Public Function SerializeRow(dataRow As Row) As String
        Dim d As String = "<COL>"
       
        Dim sSerialized As String = SerializeDate(dataRow.Added) + d + SerializeDate(dataRow.Expiration) _
            + d + SerializeDate(dataRow.Synced) + d + LCase(dataRow.PrimaryKey) + d _
            + SerStr(dataRow.DataColumn1) + d + SerStr(dataRow.DataColumn2) _
            + d + SerStr(dataRow.DataColumn3) + d + SerStr(dataRow.DataColumn4) + d + SerStr(dataRow.DataColumn5) _
            + d + SerStr(dataRow.Magnitude) + d + SerStr(dataRow.RAC)

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
        Dim sErr As String = ""

        For x As Integer = 1 To 10
            Try
                Insert(dataRow, False)
                Return True

            Catch ex As Exception
                sErr = ex.Message
                System.Threading.Thread.Sleep(2000)
            End Try
        Next x

        Log("While storing data row " + dataRow.Table + "," + dataRow.Database + ", PK: " + dataRow.PrimaryKey + " : " + sErr)


        Return False
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



    Public Sub ExportToCSV2()
        Try

        Dim sReport As String = ""
        Dim sReportRow As String = ""
            Dim sHeader As String = "CPID,Magnitude,TotalRAC,Expiration,Synced,Address,CPID_Valid"
        sReport += sHeader + vbCrLf
        Dim grr As New GridcoinReader.GridcoinRow
            Dim sHeading As String = "CPID;Magnitude;TotalRAC;Expiration;Synced;Address;CPID_Valid"
        Dim vHeading() As String = Split(sHeading, ";")
        Dim sData As String = modPersistedDataSystem.GetMagnitudeContractDetails()
        Dim vData() As String = Split(sData, ";")
        Dim iRow As Long = 0
        Dim sValue As String
        For y = 0 To UBound(vData) - 1
            sReportRow = ""
            For x = 0 To UBound(vHeading)
                Dim vRow() As String = Split(vData(y), ",")
                sValue = vRow(x)
                sReportRow += sValue + ","
            Next x
            sReport += sReportRow + vbCrLf
            iRow = iRow + 1
        Next
        'Get the Neural Hash
        Dim sMyNeuralHash As String
        Dim sContract = GetMagnitudeContract()
        sMyNeuralHash = GetMd5String(sContract)
        sReport += "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"
        Dim sWritePath As String = GetGridFolder() + "reports\DailyNeuralMagnitudeReport.csv"
        If Not System.IO.Directory.Exists(GetGridFolder() + "reports") Then MkDir(GetGridFolder() + "reports")
        Using objWriter As New System.IO.StreamWriter(sWritePath)
            objWriter.WriteLine(sReport)
            objWriter.Close()
        End Using

        Catch ex As Exception
            Log("Error while exportingToCSV2: " + ex.Message)
        End Try

    End Sub


End Module

Public Class MyWebClient2
    Inherits System.Net.WebClient
    Private timeout As Long = 10000
    Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
        Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
        w.Timeout = timeout
        Return (w)
    End Function

End Class