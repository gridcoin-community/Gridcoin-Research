
Imports Microsoft.VisualBasic
Imports System.IO

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
    Public msThreadedCPID As String = ""
    Public bNeedsDgvRefreshed As Boolean = False

    Public mlQueue As Long = 0
    Public bDatabaseInUse As Boolean = False
    'Minimum RAC percentage required for RAC to be counted in magnitude:
    Public mdMinimumRacPercentage As Double = 0.06
    Public bMagsDoneLoading As Boolean = True
    Public mdLastSync As Date = DateAdd(DateInterval.Day, -10, Now)
    Public mlPercentComplete As Double = 0
    Public msContractDataForQuorum As String
    Public NeuralNetworkMultiplier As Double = 115000
    Private mclsQHA As New clsQuorumHashingAlgorithm
    Private Const MINIMUM_WITNESSES_REQUIRED_TESTNET As Long = 8
    Private Const MINIMUM_WITNESSES_REQUIRED_PROD As Long = 10
    Public mdLastNeuralNetworkSync As DateTime
    Private lUseCount As Long = 0
    Private MAX_NEURAL_NETWORK_THREADS = 255
    Public msNeuralDetail As String = ""
    Public SYNC_THRESHOLD As Double = 60 * 24 '24 hours
    Public TEAM_SYNC_THRESHOLD As Double = 60 * 24 * 7 '1 WEEK
    Public PROJECT_SYNC_THRESHOLD As Double = 60 * 24 '24 HOURS

    Public Structure NeuralStructure
        Public PK As String
        Public NeuralValue As Double
        Public Witnesses As Double
        Public Updated As DateTime
    End Structure
    Public mdictNeuralNetworkQuorumData As Dictionary(Of String, GRCSec.GridcoinData.NeuralStructure)
    Public mdictNeuralNetworkAdditionalQuorumData As Dictionary(Of String, GRCSec.GridcoinData.NeuralStructure)
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
        Public AvgRAC As String
        Public Found As Boolean
        Public Witnesses As Long
    End Structure
    Public Structure CPID
        Public cpid As String
        Public RAC As Double
        Public Magnitude As Double
        Public Expiration As Date
    End Structure
    Public msCurrentNeuralHash = ""
    Public Function Num(sData As String) As String
        'Ensures culture is neutral
        Dim sOut As String
        sOut = Trim(Math.Round(RoundedMag(Val("0" + Trim(sData))), 2))
        sOut = Replace(sOut, ",", ".")
        Return sOut
    End Function
    Public Sub TestPDS1()
    End Sub
    Public Sub ReconnectToNeuralNetwork()
        Try
            mGRCData = New GRCSec.GridcoinData
        Catch ex As Exception
            Log("Unable to connect to neural network.")
        End Try
    End Sub
    Public Function ConstructTargetFileName(sEtag As String) As String
        Dim sFilename = sEtag + ".gz"
        Dim sEtagTarget As String = GetGridFolder() + "NeuralNetwork\" + sFilename
        Return sEtagTarget
    End Function
    Public Function GetFileSize(sPath As String) As Double
        If File.Exists(sPath) = False Then Return 0
        Dim fi As New FileInfo(sPath)
        Return fi.Length
    End Function
    Public Function DeleteOlderThan(sDirectory As String, lDaysOld As Long, sExtension As String) As Boolean
        Try
            Dim Dir1 As New IO.DirectoryInfo(sDirectory)
            For Each file As IO.FileInfo In Dir1.GetFiles
                If LCase(file.Extension) = LCase(sExtension) Then
                    If Not LCase(file.Name).Contains("whitelist") Then
                        If (Now - file.CreationTime).Days > lDaysOld Then file.Delete()
                    End If
                End If
            Next
        Catch ex As Exception
            Log("Unable to delete old file: " + ex.Message)
            Return False
        End Try
        Return True
    End Function


    Public Function UnixTimestampToDate(ByVal timestamp As Double) As DateTime
        Try
            Dim dt1 As New DateTime(1970, 1, 1, 0, 0, 0)
            Dim dt2 As DateTime = TimeZoneInfo.ConvertTimeToUtc(dt1)
            dt2 = dt2.AddSeconds(timestamp)
            Return dt2
        Catch ex As Exception
            Log("Unable to convert timestamp " + Trim(timestamp) + " to UTC datetime.")
            Dim dt3 As New DateTime(1970, 1, 1, 0, 0, 0)
            dt3 = dt3.AddSeconds(timestamp)
            Return dt3
        End Try
    End Function
    Public Function GetMagnitudeContractDetails() As String
        Dim surrogateRow As New Row
        surrogateRow.Database = "CPID"
        surrogateRow.Table = "CPIDS"
        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        'FAILED TO COMPARE ELEMENT IN ARRAY
        lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim sOut As String = ""
        For Each cpid As Row In lstCPIDs
            Dim sRow As String = cpid.PrimaryKey + "," + Num(cpid.Magnitude) _
                                 + "," + Num(cpid.Magnitude) + "," + Num(cpid.RAC) _
                                 + "," + Trim(cpid.Synced) + "," + Trim(cpid.DataColumn4) _
                                 + "," + Trim(cpid.DataColumn5) + "," + Trim(cpid.Witnesses) + ";"
            sOut += sRow
        Next
        Return sOut
    End Function
    Private Function SumOfNetworkMagnitude(ByRef dAvg As Double) As Double
        Dim surrogateRow As New Row
        surrogateRow.Database = "CPID"
        surrogateRow.Table = "CPIDS"
        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        Dim sOut As String = ""
        Dim total As Double
        Dim entries As Double = 0
        For Each cpid As Row In lstCPIDs
            total += Val(cpid.Magnitude)
            entries += 1
        Next
        dAvg = total / (entries + 0.01)
        Return total
    End Function
    Public Function GetMagnitudeContract() As String

        Dim dLegacyMagnitudeBoost As Double = 1.35
        Dim dtCutoverDate As DateTime = TimeZoneInfo.ConvertTimeToUtc(New DateTime(2017, 9, 7))
        If Now > dtCutoverDate Then dLegacyMagnitudeBoost = 1.0

        Try
            If GetWindowsFileAge(GetGridPath("NeuralNetwork") + "\contract.dat") < 240 Then
                Dim sData As String
                sData = FileToString(GetGridPath("NeuralNetwork") + "\contract.dat")
                Return sData
            End If

            Dim lAgeOfMaster As Long = GetWindowsFileAge(GetGridPath("NeuralNetwork") + "\db.dat")
            If lAgeOfMaster > PROJECT_SYNC_THRESHOLD Then Return ""

            Dim surrogateRow As New Row
            surrogateRow.Database = "CPID"
            surrogateRow.Table = "CPIDS"
            Dim lTotal As Long
            Dim lRows As Long
            If mlPercentComplete > 0 Then Return ""
            'If Last synced older than 1 day, delete the contract - 10-2-2015
            Dim sOut As String = ""
            sOut = "<MAGNITUDES>"
            Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
            lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
            Dim dMagAge As Long = 0
            Dim lMaxZeroShaveAmount = lstCPIDs.Count * 0.06 'Our superblocks must be within 10% tolerance (as compared to beacon count) to be accepted
            Dim lShavedZeroCount As Long = 0

            For Each cpid As Row In lstCPIDs
                If cpid.DataColumn5 = "True" Then
                    Dim dLocalMagnitude As Double = Val("0" + Num(cpid.Magnitude)) * dLegacyMagnitudeBoost
                    If dLocalMagnitude > 32766 Then dLocalMagnitude = 32766

                    Dim sRow As String = cpid.PrimaryKey + "," + Num(dLocalMagnitude) + ";"
                    'Zero magnitude rule (We need a placeholder because of the beacon count rule)
                    If Val(dLocalMagnitude) = 0 Then
                        sRow = "0,15;"
                        If lShavedZeroCount < lMaxZeroShaveAmount Then
                            lShavedZeroCount += 1
                            sRow = "" 'Remove the row
                        End If
                    End If

                    lTotal = lTotal + Val("0" + Trim(dLocalMagnitude))
                    lRows = lRows + 1
                    If Len(sRow) > 0 Then sOut += sRow
                    dMagAge = 0

                Else
                    Dim sRow As String = cpid.PrimaryKey + ",00;"
                    lTotal = lTotal + 0
                    lRows = lRows + 1
                    sOut += sRow
                End If
            Next
            'sOut += "00000000000,275000;" 'This is a placeholder to be removed in Neural Network 2.0
            'This is a placeholder to be removed in Neural Network 2.0.
            'It is needed to bump the average magnitude above 70 to avoid having the superblock rejected.
            'The CPID cannot be all 0 since it will be filtered out and the hashes of the ASCII and the
            'binary superblock will diff. When the 70 average mag requirement has been lifted from the
            'C++ code this placeholder can be removed.
            '            sOut += "00000000000000000000000000000001,32767;"
            sOut += "</MAGNITUDES><QUOTES>"

            surrogateRow.Database = "Prices"
            surrogateRow.Table = "Quotes"
            lstCPIDs = GetList(surrogateRow, "*")

            For Each cpid As Row In lstCPIDs
                Dim dNeuralMagnitude As Double = 0
                Dim sRow As String = cpid.PrimaryKey + "," + Num(cpid.Magnitude) + ";"
                lTotal = lTotal + Val("0" + Trim(cpid.Magnitude))
                lRows = lRows + 1
                sOut += sRow
            Next

            sOut += "</QUOTES><AVERAGES>"
            Dim avg As Double
            avg = lTotal / (lRows + 0.01)


            If avg < 10 Or avg > 170000 Then
                Return "<ERROR>Superblock Average " + Trim(avg) + " out of bounds</ERROR>"
            End If
            'APPEND the Averages:

            Dim lProjectsInContract As Long = 0
            Dim surrogateWhitelistRow As New Row
            Dim lstWhitelist As List(Of Row)
            surrogateWhitelistRow.Database = "Whitelist"
            surrogateWhitelistRow.Table = "Whitelist"
            lstWhitelist = GetList(surrogateWhitelistRow, "*")
            Dim rRow As New Row
            Dim lTotalProjRac As Double = 0
            rRow.Database = "Project"
            rRow.Table = "Projects"
            Dim lstP As List(Of Row) = GetList(rRow, "*")
            lstP.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
            For Each r As Row In lstP
                Dim bIsThisWhitelisted = IsInList(r.PrimaryKey, lstWhitelist, False)
                If (bIsThisWhitelisted) Then
                    If Val("0" + Trim(r.AvgRAC)) > 0 Then
                        Dim sRow As String = r.PrimaryKey + "," + Num(r.AvgRAC) _
                                             + "," + Num(r.RAC) + ";"
                        lRows = lRows + 1
                        lProjectsInContract += 1
                        lTotalProjRac += Val(Num(r.RAC))
                        sOut += sRow
                    End If
                End If

            Next
            Dim lAvgProjRac As Double = lTotalProjRac / (lProjectsInContract + 0.01)
            If lAvgProjRac < 50000 Then
                Return "<ERROR>Superblock Project Average of " + Trim(lAvgProjRac) + " out of bounds</ERROR>"
            End If
            Log("Contracts in Project : " + Trim(lProjectsInContract) + ", Whitelisted Count: " + lstP.Count.ToString())
            'If less than 80% of the projects exist in the superblock, don't emit the contract
            If (lProjectsInContract < lstP.Count * 0.7) Then
                Log("Not enough projects in contract.")
                Return ""
            End If
            sOut += "NeuralNetwork,2000000,20000000;"
            sOut += "</AVERAGES>"
            Return sOut

        Catch ex As Exception
            Log("GetMagnitudeContract" + ex.Message)
            Return ""
        End Try

    End Function
    Public Function RoundedMag(num As Double)
        'Rounds magnitude to nearest Dither Factor
        Return Math.Round(Math.Round(num * mclsQHA.GetDitherMag(num), 0) / mclsQHA.GetDitherMag(num), 2)
    End Function
    Public Function RoundedRac(num As Double)
        'Rounds magnitude to nearest Dither Factor
        Return Math.Round(Math.Round(num * mclsQHA.GetDitherMag(num), 0) / mclsQHA.GetDitherMag(num), 2)
    End Function
    Public Function WithinBounds(n1 As Double, n2 As Double, percent As Double) As Boolean
        If n2 < (n1 + (n1 * percent)) And n2 > (n1 - (n1 * percent)) Then
            Return True
        End If
        Return False
    End Function
    Public Sub ThreadResolveDiscrepanciesInNeuralNetwork(sContract As String)
        msContractDataForQuorum = sContract
        Dim t1 As New System.Threading.Thread(AddressOf threadResolveDiscrepancies)
        t1.Start()
    End Sub
    Public Sub threadResolveDiscrepancies()
        If Len(msContractDataForQuorum) > 1 Then
            Log("Resolution process started.")
            ResolveDiscrepanciesInNeuralNetwork(msContractDataForQuorum)
        End If
    End Sub
    Public Function ResolveDiscrepanciesInNeuralNetwork(sContract As String) As String
        Dim sContractLocal As String = ""
        sContractLocal = GetMagnitudeContract()
        If Len(sContractLocal) = 0 Then Exit Function
        Dim dr As New Row
        Log("Starting neural network resolution process ...")
        Dim sResponse As String = ""
        Try
            Dim iUpdated As Long = 0
            Dim iMagnitudeDrift As Long = 0
            Dim iTimeStart As Double = Timer
            Dim sMags As String = ExtractXML(sContract, "<MAGNITUDES>")
            Dim vMags As String() = Split(sMags, ";")
            For x As Integer = 0 To UBound(vMags)
                If vMags(x).Contains(",") Then
                    Dim vRow As String() = Split(vMags(x), ",")
                    If UBound(vRow) >= 1 Then
                        Dim sCPID As String = vRow(0)
                        Dim sMag As String = vRow(1)
                        Dim dForeignMag As Double = Val("0" + Trim(sMag))
                        dr.Database = "CPID"
                        dr.Table = "CPIDS"
                        dr.PrimaryKey = sCPID
                        dr = Read(dr)
                        Dim dLocalMag As Double = Val("0" + Trim(dr.Magnitude))
                        '7-13-2015 Intelligently resolve disputes between neural network nodes
                        If dLocalMag > 0 And dForeignMag > 0 And RoundedMag(dForeignMag) <> RoundedMag(dLocalMag) Then
                            If WithinBounds(dLocalMag, dForeignMag, 0.15) And dForeignMag < dLocalMag Then
                                dr.Magnitude = RoundedMag(dForeignMag)
                                Store(dr)
                                iUpdated += 1
                                iMagnitudeDrift += Math.Abs(dForeignMag - dLocalMag)
                                Log("Neural Network Quorum: Updating magnitude for CPID " + sCPID + " from " + Trim(dLocalMag) + " to " + Trim(dForeignMag))
                            End If

                        End If
                    End If
                End If
            Next
            Dim iTimeEnd As Double = Timer
            Dim iElapsed As Double = iTimeEnd - iTimeStart
            sResponse = "Resolved (" + Trim(iUpdated) + ") discrepancies with magnitude drift of (" + Trim(iMagnitudeDrift) + ") in " + Trim(iElapsed) + " seconds."

        Catch ex As Exception
            Log("Error occurred while resolving discrepancies: " + ex.Message)
            Return "Resolving Discrepancies " + ex.Message
        End Try

        Log(sResponse)
        Return sResponse

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
                Dim prjTotalRAC As Double = 0
                units = 0
                For Each prjCPID As Row In lstProjectCPIDs
                    If Val(prjCPID.RAC) > 1 Then
                        prjTotalRAC += Val(prjCPID.RAC)
                        units += 1
                    End If
                Next
                'Persist this total
                prj.Database = "Project"
                prj.Table = "Projects"
                prj.AvgRAC = Trim(RoundedRac(prjTotalRAC / (units + 0.01)))
                prj.RAC = Trim(RoundedRac(prjTotalRAC))

                Store(prj)
            Next
            Return True
        Catch ex As Exception
            Log("Update network averages: " + ex.Message)
            Return False

        End Try
    End Function
    Public Sub SyncDPOR2(sData As String)
        If bMagsDoneLoading = False Then
            Log("Blocked call.")
            Exit Sub
        End If
        If KeyValue("disableneuralnetwork") = "true" Then
            Log("Neural network is disabled.")
            Exit Sub
        End If

        msSyncData = sData
        bMagsDoneLoading = False

        Dim t As New Threading.Thread(AddressOf CompleteSync)
        t.Priority = Threading.ThreadPriority.BelowNormal
        t.Start()
    End Sub
    Public Function NeedsSynced(dr As Row) As Boolean
        If Now > dr.Synced Then Return True Else Return False
    End Function

    Public Sub EnsureTeamIsSynchronized()
        Dim sCPIDData As String = ExtractXML(msSyncData, "<CPIDDATA>")
        Dim sQuorumData As String = ExtractXML(msSyncData, "<QUORUMDATA>")
        Dim dAge As Double = Val(ExtractXML(sQuorumData, "<AGE>"))
        Log("EnsureTeamIsSynchronized: " + Trim(dAge))

        If KeyValue("NEURAL_07252017") = "" Then
            UpdateKey("NEURAL_07252017", "CLEARING") 'Start Fresh on July 25 2017, then once every 6 hours we clear.  Take this out when we move to NN2.
            ClearProjectData()
        End If

        Dim dWindow As Double = 60 * 60 '1 hour before and 1 hour after superblock expires:
        Dim lAgeOfMaster = GetUnixFileAge(GetGridFolder() + "NeuralNetwork\db.dat")
        If lAgeOfMaster > (SYNC_THRESHOLD / 4) Then
            Log("Clearing project data once every 6 hours.")
            ClearProjectData()
        End If
        If dAge > (86400 - dWindow) And dAge < (86400 + dWindow) Then
            If lAgeOfMaster > (SYNC_THRESHOLD) Then
                'Clear out this nodes project data, so the node can sync with the team at the same exact time:
                Log("Clearing project data so we can synchronize as a team.")
                ClearProjectData()
            End If
        End If

    End Sub
    Private Sub SoftKill(sFile As String)
        Try
            Kill(sFile)
        Catch ex As Exception

        End Try
    End Sub
    Private Sub ClearProjectData()
        Dim sPath As String = GetGridFolder() + "NeuralNetwork\"
        SoftKill(sPath + "db.dat")
        'Erase the projects
        SoftKill(sPath + "*master.dat")
        SoftKill(sPath + "*.xml")
        SoftKill(sPath + "*.gz")
        SoftKill(sPath + "*.dat")
    End Sub
    Private Sub ClearWhitelistData()
        Dim sPath As String = GetGridFolder() + "NeuralNetwork\Whitelist\"
        SoftKill(sPath + "whitelist*.dat")
    End Sub
    Public Function EnsureNNDirExists()
        'Ensure NN dir exists:
        Try
            Dim sNNPath As String = GetGridFolder() + "NeuralNetwork\"
            If System.IO.Directory.Exists(sNNPath) = False Then
                MkDir(sNNPath)
            End If
        Catch ex As Exception
            Log("Unable to create Neural Network Directory " + ex.Message)
        End Try
    End Function
    Public Sub CompleteSync()
        mbForcefullySyncAllRac = True
        Log("Starting complete Neural Network Sync.")

        Try
            EnsureNNDirExists()
            msCurrentNeuralHash = ""

            Try
                mlPercentComplete = 1
                EnsureTeamIsSynchronized()
                UpdateMagnitudesPhase1()
            Catch ex As Exception
                Log("Err in completesync" + ex.Message)
            End Try
            Log("Complete Sync: Updating mags")
            Try
                UpdateMagnitudes()
                mlPercentComplete = 0
            Catch ex As Exception
                Log("Err in UpdateMagnitudes in completesync" + ex.Message)
            End Try
            mbForcefullySyncAllRac = False
            If LCase(KeyValue("exportmagnitude")) = "true" Then
                ExportToCSV2()
            End If
        Catch ex As Exception
            Log("Completesync:" + ex.Message)
        End Try

        mdLastSync = Now
        mlPercentComplete = 0
        '7-21-2015: Store historical magnitude so it can be charted
        StoreHistoricalMagnitude()
        bNeedsDgvRefreshed = True
        bMagsDoneLoading = True

    End Sub
    Private Function GetMagByCPID(sCPID As String) As Row
        Dim dr As New Row
        dr.Database = "CPID"
        dr.Table = "CPIDS"
        dr.PrimaryKey = sCPID
        dr = Read(dr)
        Return dr
    End Function
    Private Function GRCDay(dDt As DateTime) As String
        'Return a culture agnostic date
        Dim sNeutralDate As String = Trim(Year(dDt)) + "-" + Trim(Month(dDt)) + "-" + Trim(Day(dDt))
        Return sNeutralDate
    End Function
    Private Function GRCDayToDate(sDate As String) As DateTime
        Dim vDate() As String = Split(sDate, "-")
        If UBound(vDate) < 2 Then Return Now
        Dim dt As DateTime = DateSerial(vDate(0), vDate(1), vDate(2))
        Return dt
    End Function
    Private Sub StoreHistoricalMagnitude()
        Dim sCPID As String = KeyValue("PrimaryCPID")
        If Len(sCPID) = 0 Then Exit Sub 'If we dont know the primary cpid
        Dim MyCPID As Row = GetMagByCPID(sCPID)
        'Store the Historical Magnitude:
        Dim d As New Row
        d.Expiration = DateAdd(DateInterval.Day, 30, Now)
        d.Added = Now
        d.Database = "Historical"
        d.Table = "Magnitude"
        d.PrimaryKey = sCPID + GRCDay(Now)
        d.Magnitude = MyCPID.Magnitude
        d.RAC = MyCPID.RAC
        d.AvgRAC = MyCPID.AvgRAC
        Store(d)
        d = New Row
        d.Database = "Historical"
        d.Table = "Magnitude"
        d.PrimaryKey = "Network" + GRCDay(Now)
        Dim dAvg As Double
        d.Magnitude = SumOfNetworkMagnitude(dAvg)
        d.AvgRAC = Trim(Math.Round(dAvg, 3))
        Store(d)
        'Update Last Time Synced for SeP
        d = New Row
        d.Database = "Historical"
        d.Table = "Magnitude"
        d.PrimaryKey = "LastTimeSynced"
        d.Expiration = DateAdd(DateInterval.Day, 30, Now)
        d.Synced = Now
        Store(d)
        Try
            'Let the neural network know we are updated
            If sCPID = "" Then sCPID = "INVESTOR"
        Catch ex As Exception
            Log(ex.Message)
        End Try
    End Sub
    Public Function UpdateSuperblockAgeAndQuorumHash(sAge As String, sQuorumHash As String, sTimestamp As String, sBlock As String, sCPID As String)
        Dim d As New Row
        d.Database = "Historical"
        d.Table = "Magnitude"
        d.PrimaryKey = "QuorumHash"
        d.DataColumn1 = Trim(sAge)
        d.DataColumn2 = Trim(sQuorumHash)
        d.DataColumn3 = Trim(sTimestamp)
        d.DataColumn4 = Trim(sBlock)
        d.Expiration = DateAdd(DateInterval.Day, 30, Now)
        Store(d)
        If Len(sCPID) > 5 Then
            UpdateKey("PrimaryCPID", sCPID)
        End If
    End Function
    Public Sub StoreValue(sDatabase As String, sTable As String, sPK As String, sValue As String)
        sPK = Replace(sPK, "/", "[fslash]")
        sPK = Replace(sPK, ".", "[period]")
        sPK = Replace(sPK, ":", "[colon]")
        Dim d As New Row
        d.Expiration = DateAdd(DateInterval.Day, 30, Now)
        d.Added = Now
        d.Database = sDatabase
        d.Table = sTable
        d.PrimaryKey = sPK
        d.DataColumn1 = sValue
        Store(d)
    End Sub
    Public Function GetDataValue(sDB As String, sTable As String, sPK As String) As Row
        sPK = Replace(sPK, "/", "[fslash]")
        sPK = Replace(sPK, ".", "[period]")
        sPK = Replace(sPK, ":", "[colon]")
        Dim dr As New Row
        dr.Database = sDB
        dr.Table = sTable
        dr.PrimaryKey = sPK
        dr = Read(dr)
        Return dr
    End Function
    Public Function GetHistoricalMagnitude(dt As DateTime, sCPID As String, ByRef dAvg As Double) As Double
        Dim dr As New Row
        dr.Database = "Historical"
        dr.Table = "Magnitude"
        dr.PrimaryKey = sCPID + GRCDay(dt)
        dr = Read(dr)
        dAvg = Val(dr.AvgRAC)
        Return Val(dr.Magnitude)
    End Function
    Private Sub EraseNeuralNetwork(sDatabase As String)
        Try
            Dim sNN As String = GetGridFolder() + "NeuralNetwork\"
            If Directory.Exists(sNN) = False Then
                Try
                    MkDir(sNN)
                Catch ex As Exception
                    Log("Unable to create neural network directory.")
                End Try
            End If
            Kill(sNN + sDatabase + "\*.dat")
        Catch ex As Exception
            Log("EraseNeuralNetwork:" + ex.Message)
        End Try
    End Sub

    Public Function UpdateMagnitudesPhase1()
        Try
            Dim surrogateRow1 As New Row
            Log("Deleting Projects")
            surrogateRow1.Database = "Project"
            surrogateRow1.Table = "Projects"
            EraseNeuralNetwork("project")
            EraseNeuralNetwork("projects")
            surrogateRow1.Database = "Whitelist"
            surrogateRow1.Table = "Whitelist"
            EraseNeuralNetwork("Whitelist")
            Dim sWhitelist As String
            sWhitelist = ExtractXML(msSyncData, "<WHITELIST>")
            Dim sCPIDData As String = ExtractXML(msSyncData, "<CPIDDATA>")
            Dim sQuorumData As String = ExtractXML(msSyncData, "<QUORUMDATA>")
            Dim sAge As String = ExtractXML(sQuorumData, "<AGE>")
            Dim sQuorumHash As String = ExtractXML(sQuorumData, "<HASH>")
            Dim TS As String = ExtractXML(sQuorumData, "<TIMESTAMP>")
            Dim sBlock As String = ExtractXML(sQuorumData, "<BLOCKNUMBER>")
            Dim sPrimaryCPID As String = ExtractXML(sQuorumData, "<PRIMARYCPID>")
            Call UpdateSuperblockAgeAndQuorumHash(sAge, sQuorumHash, TS, sBlock, sPrimaryCPID)
            Try
                mlPercentComplete = 2
                Dim vWhitelist() As String = Split(sWhitelist, "<ROW>")
                For x As Integer = 0 To UBound(vWhitelist)
                    If Len(vWhitelist(x)) > 1 Then
                        Dim vRow() As String
                        vRow = Split(vWhitelist(x), "<COL>")
                        If UBound(vRow) > 0 Then
                            Dim sProject As String = vRow(0)
                            Dim sURL As String = Trim(vRow(1))
                            If Right(sURL, 1) = "@" Then
                                Dim dr As New Row
                                dr.Database = "Whitelist"
                                dr.Table = "Whitelist"
                                dr.DataColumn1 = sURL
                                dr.PrimaryKey = sProject
                                dr = Read(dr)
                                dr.Expiration = DateAdd(DateInterval.Day, 14, Now)
                                dr.Synced = DateAdd(DateInterval.Day, -1, Now)
                                Store(dr)
                            End If
                        End If
                    End If
                Next x
            Catch ex As Exception
                Log("UM Phase 1: While loading projects " + ex.Message)
            End Try

            Dim vCPIDs() As String = Split(sCPIDData, "<ROW>")
            Dim vTestNet() As String
            vTestNet = Split(vCPIDs(0), "<COL>")
            Log("Updating magnitude in testnet=" + Trim(mbTestNet) + " for " + Trim(UBound(vCPIDs)) + " cpids.")
            'Delete any CPIDs that are in the neural network that no longer have beacons:
            surrogateRow1.Database = "CPID"
            surrogateRow1.Table = "CPIDS"
            EraseNeuralNetwork("cpid")
            EraseNeuralNetwork("cpids")

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

            mlPercentComplete = 1
            Dim c As New clsBoincProjectDownload
            c.DownloadGZipFiles()
            mlPercentComplete = 2
            Log("Starting Phase II")

        Catch ex As Exception
            Log("UpdateMagnitudesPhase1: " + ex.Message)
        End Try

        msSyncData = ""

    End Function
    Public Function GetWhitelistedCount(lstProjects As List(Of Row), lstWhitelist As List(Of Row))
        Dim WhitelistedProjects As Double = 0
        For Each prj As Row In lstProjects
            Dim bIsThisWhitelisted = IsInList(prj.PrimaryKey, lstWhitelist, False)
            If bIsThisWhitelisted Then
                WhitelistedProjects += 1
            End If
        Next
        Return WhitelistedProjects
    End Function
    Public Function GetWPC(ByRef WhitelistedProjects As Double, ByRef CountOfAllProjects As Double) As List(Of Row)

        Dim surrogateWhitelistRow As New Row
        Dim lstWhitelist As List(Of Row)
        surrogateWhitelistRow.Database = "Whitelist"
        surrogateWhitelistRow.Table = "Whitelist"
        lstWhitelist = GetList(surrogateWhitelistRow, "*")
        '
        Dim rPRJ As New Row
        rPRJ.Database = "Project"
        rPRJ.Table = "Projects"
        Dim lstProjects1 As List(Of Row) = GetList(rPRJ, "*")
        WhitelistedProjects = GetWhitelistedCount(lstProjects1, lstWhitelist)
        CountOfAllProjects = lstProjects1.Count
        Return lstWhitelist
    End Function
    Private Function GetConsensusData()
        For x As Integer = 1 To 3
            Try
                ReconnectToNeuralNetwork()
                mdictNeuralNetworkQuorumData = mGRCData.GetNeuralNetworkQuorumData2("quorumdata", mbTestNet, IIf(mbTestNet, MINIMUM_WITNESSES_REQUIRED_TESTNET, MINIMUM_WITNESSES_REQUIRED_PROD))
                mdictNeuralNetworkAdditionalQuorumData = mGRCData.GetNeuralNetworkQuorumData3("quorumconsensusdata", mbTestNet, IIf(mbTestNet, MINIMUM_WITNESSES_REQUIRED_TESTNET, MINIMUM_WITNESSES_REQUIRED_PROD))
                If mdictNeuralNetworkQuorumData.Count > IIf(mbTestNet, MINIMUM_WITNESSES_REQUIRED_TESTNET, MINIMUM_WITNESSES_REQUIRED_PROD) Then
                    Return True
                End If
            Catch ex As Exception
                Dim sErr As String = ex.Message
            End Try
        Next x
        Return False
    End Function
    Public Function GuiDoEvents()
        Try
            If Not mfrmMining Is Nothing Then
                mfrmMining.DoEvents()
            End If
        Catch

        End Try

    End Function
    Public Function CPIDCountWithNoWitnesses()
        'Loop through the researchers
        Dim r As Row
        Dim lstCPIDs As List(Of Row)
        r.Database = "CPID"
        r.Table = "CPIDS"
        lstCPIDs = GetList(r, "*")
        Dim lNoWitnesses As Long = 0

        For Each cpid As Row In lstCPIDs
            If cpid.Witnesses = 0 Then
                lNoWitnesses += 1
            End If
        Next
        Return lNoWitnesses

    End Function
    Public Sub ResetCPIDsForManualSync()
        Dim r As Row
        Dim lstCPIDs As List(Of Row)
        r.Database = "CPID"
        r.Table = "CPIDS"
        lstCPIDs = GetList(r, "*")
        Dim lNoWitnesses As Long = 0
        For Each cpid As Row In lstCPIDs
            cpid.Witnesses = 0
            Store(cpid)
        Next
    End Sub

    Public Function UpdateMagnitudes() As Boolean
        Dim lstCPIDs As List(Of Row)
        Dim surrogateRow As New Row
        Dim WhitelistedProjects As Double = 0
        Dim ProjCount As Double = 0
        Dim lstWhitelist As List(Of Row)
        Dim bConsensus As Boolean = False
        bConsensus = GetConsensusData()
        Log("Updating Magnitudes " + IIf(bConsensus, "With consensus data", "Without consensus data"))
        Dim lStartingWitnesses As Long = CPIDCountWithNoWitnesses()
        Log(Trim(lStartingWitnesses) + " CPIDs starting out with clean slate.")

        For z As Integer = 1 To 5
            Dim iRow As Long = 0

            Try
                'Loop through the researchers
                surrogateRow.Database = "CPID"
                surrogateRow.Table = "CPIDS"
                lstCPIDs = GetList(surrogateRow, "*")
                lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
                mlQueue = 0
                For Each cpid As Row In lstCPIDs
                    Try

                        If cpid.Witnesses = 0 Then
                            Dim bResult As Boolean = GetRacFromNeuralNetwork(cpid.PrimaryKey, 0)
                            GuiDoEvents()
                        End If

                        iRow += 1
                        Dim p As Double = (iRow / (lstCPIDs.Count + 0.01)) * 100
                        mlPercentComplete = p + 5
                        If mlPercentComplete > 90 Then mlPercentComplete = 90
                        'Log(Trim(mlPercentComplete) + "%: #" + Trim(iRow) + ", Gathering Magnitude for CPID " + cpid.PrimaryKey + ", RAC: " + Trim(cpid.RAC))
                    Catch ex As Exception
                        Log("Error in UpdateMagnitudes: " + ex.Message + " while processing CPID " + Trim(cpid.PrimaryKey))
                    End Try

                Next
            Catch ex As Exception
                Log("UpdateMagnitudes:GatherRAC: " + ex.Message)
            End Try

            'Thread.Join
            For x As Integer = 1 To 120
                If mlQueue = 0 Then Exit For
                GuiDoEvents()
                Threading.Thread.Sleep(200)
                mlPercentComplete -= 1
                If mlPercentComplete < 10 Then mlPercentComplete = 10
            Next
            Dim lNoWitnesses As Long = CPIDCountWithNoWitnesses()
            If mlQueue = 0 And lNoWitnesses = 0 Then Exit For Else Log(Trim(lNoWitnesses) + " CPIDs remaining with no witnesses.  Cleaning up problem.")
        Next z

        Try
            Log("UpdNetworkAvgs Start Time ")

            UpdateNetworkAverages()
            Log("UpdNetworkAvgs End Time")

        Catch ex As Exception
            Log("UpdateMagnitudes:UpdateNetworkAverages: " + ex.Message)
        End Try

        lstWhitelist = GetWPC(WhitelistedProjects, ProjCount)
        lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim sMemoryName = IIf(mbTestNet, "magnitudes_testnet", "magnitudes")
        'Get CryptoCurrency Quotes:
        Dim dBTC As Double = GetCryptoPrice("BTC").Price * 100
        Dim dGRC As Double = GetCryptoPrice("GRC").Price * 10000000000
        '8-16-2015
        Dim q As New Row
        q.Database = "Prices"
        q.Table = "Quotes"
        q.PrimaryKey = "BTC"
        q.Expiration = DateAdd(DateInterval.Day, 1, Now)
        q.Synced = q.Expiration

        q.Magnitude = Trim(Math.Round(dBTC, 2))
        Log("Storing Bitcoin price quote")
        Store(q)
        q = New Row
        q.Database = "Prices"
        q.Table = "Quotes"
        q.Expiration = DateAdd(DateInterval.Day, 1, Now)
        q.PrimaryKey = "GRC"
        q.Magnitude = Trim(Math.Round(dGRC, 2))
        q.Synced = q.Expiration
        Log("Storing Gridcoin Price Quote")
        Store(q)

        'Update all researchers magnitudes (Final Calculation Phase):
        Dim surrogatePrj As New Row
        surrogatePrj.Database = "Project"
        surrogatePrj.Table = "Projects"
        Dim lstProjects As List(Of Row) = GetList(surrogatePrj, "*")
        'Remove blacklisted projects first
        For x As Integer = 0 To lstProjects.Count - 1
            If Not IsInList(lstProjects(x).PrimaryKey, lstWhitelist, False) Then
                Dim dr As Row = lstProjects(x)
                dr.PrimaryKey = ""
                lstProjects(x) = dr
            End If
        Next
        'Update individual CPIDs with calculated magnitude:
        Try
            Dim iRow2 As Long = 0
            lstCPIDs = GetList(surrogateRow, "*")
            For Each cpid As Row In lstCPIDs
                Dim dResearcherMagnitude As Double = 0
                Dim TotalRAC As Double = 0
                Dim TotalNetworkRAC As Double = 0
                Dim TotalMagnitude As Double = 0
                'Dim TotalRAC As Double = 0
                For Each prj As Row In lstProjects
                    Dim surrogatePrjCPID As New Row
                    If prj.PrimaryKey <> "" Then
                        surrogatePrjCPID.Database = "Project"
                        surrogatePrjCPID.Table = prj.PrimaryKey + "CPID"
                        surrogatePrjCPID.PrimaryKey = prj.PrimaryKey + "_" + cpid.PrimaryKey
                        Dim rowRAC = Read(surrogatePrjCPID)
                        Dim CPIDRAC As Double = Val(Trim("0" + rowRAC.RAC))
                        Dim PrjTotalRAC As Double = Val(Trim("0" + prj.RAC))
                        If CPIDRAC > 0 Then
                            TotalRAC += CPIDRAC
                            TotalNetworkRAC += PrjTotalRAC
                            Dim IndMag As Double = Math.Round(((CPIDRAC / (PrjTotalRAC + 0.01)) / (WhitelistedProjects + 0.01)) * NeuralNetworkMultiplier, 2)
                            TotalMagnitude += IndMag
                        End If
                    End If

                Next
                'Now we can store the magnitude - Formula for Network Magnitude per CPID:
                Dim dCalculatedResearcherMagnitude As Double = Math.Round(TotalMagnitude, 2)
                If TotalMagnitude < 1 And TotalMagnitude > 0.25 Then dCalculatedResearcherMagnitude = 1
                PersistMagnitude(cpid, dCalculatedResearcherMagnitude, True)
                iRow2 += 1
                Dim p As Double = (iRow2 / (lstCPIDs.Count + 0.01)) * 9.9
                mlPercentComplete = p + 90
                If mlPercentComplete > 99 Then mlPercentComplete = 99
            Next

            mGRCData.FinishSync(mbTestNet)

            mlPercentComplete = 0
            Return True
        Catch ex As Exception
            Log("UpdateMagnitudes:StoreMagnitudes: " + ex.Message)
        End Try

    End Function
    Public Function StringStandardize(sData As String) As String
        Dim sCompare1 As String = sData
        sCompare1 = Replace(sCompare1, "_", " ")
        sCompare1 = Replace(sCompare1, " ", "")
        sCompare1 = UCase(sCompare1)
        Return sCompare1
    End Function
    Public Function IsInListExact(sData As String, lstRows As List(Of Row)) As Boolean
        For Each oRowIterator As Row In lstRows
            If Trim(UCase(oRowIterator.PrimaryKey)) = Trim(UCase(sData)) Then
                Return True
            End If
        Next
        Return False
    End Function

    Public Function IsInList(sData As String, lstRows As List(Of Row), bRequireExactMatch As Boolean) As Boolean
        For Each oRowIterator As Row In lstRows
            If Trim(UCase(oRowIterator.PrimaryKey)) = Trim(UCase(sData)) Then
                Return True
            End If
            'One-off exceptions due to differences between project sites & Ntsoft (Will adjust via voting shortly then we can remove the code)
            If sData Like "*the lattice proj*" Then Return False
            If sData Like "*lhc@home classic" Then Return True
            If sData Like "*moowrap*" Then Return True

            If Not bRequireExactMatch Then
                Dim sCompare1 As String = StringStandardize(oRowIterator.PrimaryKey)
                Dim sCompare2 As String = StringStandardize(sData)
                If Left(sCompare1, 12) = Left(sCompare2, 12) Then Return True
            End If
        Next
        Return False
    End Function
    Public Function xGetList(DataRow As Row, sWildcard As String) As List(Of Row)
        Dim xx As New List(Of Row)
        Dim sErr As String = ""

        For x As Integer = 1 To 10
            Try
                Return xx
            Catch ex As Exception
                sErr = ex.Message
                System.Threading.Thread.Sleep(100)
            End Try
        Next

        Return xx

        Log("While asking for list in getlist of " + DataRow.PrimaryKey + ", " + sErr)

    End Function

    Public Function GetList(DataRow As Row, sWildcard As String) As List(Of Row)
        '   Dim sPath As String = GetPath(DataRow)
        Dim sTemp As String = ""
        Dim d As String = "<COL>"
        Dim x As New List(Of Row)
        Dim oLock As New Object

        Dim sNNFolder As String = GetGridFolder() + "NeuralNetwork\" + DataRow.Table + "\"
        If Not System.IO.Directory.Exists(sNNFolder) Then
            Try
                MkDir(sNNFolder)
            Catch ex As Exception
                Log("Unable to create Neural Network Subfolder : " + sNNFolder)
            End Try
        End If
        Dim bBlacklist As Boolean

        SyncLock oLock
            Dim di As New DirectoryInfo(sNNFolder)
            Dim fiArr As FileInfo() = di.GetFiles()
            Dim fi As FileInfo
            Dim sPrefix = GetEntryPrefix(DataRow)
            For Each fi In fiArr
                If Left(fi.Name, Len(sPrefix)) <> sPrefix Then Continue For

                Try
                    Using Stream As New System.IO.FileStream(fi.FullName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)
                        Dim objReader As New System.IO.StreamReader(Stream)
                        While objReader.EndOfStream = False
                            sTemp = objReader.ReadLine
                            Dim r As Row = DataToRow(sTemp)
                            r.Database = DataRow.Database
                            r.Table = DataRow.Table
                            If LCase(r.PrimaryKey) Like LCase(sWildcard) Then
                                'Differences between Project Sites and Ntsoft:
                                bBlacklist = False
                                If LCase(r.Table) = "project" Or LCase(r.Table) = "whitelist" Or LCase(r.Table) = "projects" Then
                                    r.PrimaryKey = Replace(r.PrimaryKey, "_", " ")
                                    If IsInListExact(r.PrimaryKey, x) Then bBlacklist = True
                                End If
                                If Not bBlacklist Then
                                    If Not r.PrimaryKey Is Nothing Then
                                        x.Add(r)
                                    End If
                                End If
                            End If
                        End While
                        objReader.Close()
                    End Using
                Catch ex As IO.FileNotFoundException
                    Log("GetList: Error reading " + fi.FullName)
                End Try

            Next fi
        End SyncLock
        Return x
    End Function

    Public Function GetRacFromNeuralNetwork(sCPID As String, lMsWait As Long) As Boolean
        Dim lCatastrophicFailures As Long = 0
TryAgain:
        Try

            Dim oNeuralType As New NeuralStructure
            oNeuralType.PK = sCPID
            Threading.ThreadPool.QueueUserWorkItem(AddressOf ThreadGetRac, oNeuralType)

ThreadStarted:
            mlQueue += 1
            If lCatastrophicFailures > 0 Then
                Log("Successfully recovered from catastrophic failures.")
            End If
        Catch ex As Exception
            Log("Panic: Threading error: " + ex.Message) 'Handle out of memory errors on small nodes.

            If lCatastrophicFailures > 10 Then
                'This node may not have enough RAM to sync the NN... Fail
                Log("Node does not have enough RAM to sync with Neural Network.  Failing.")
                Return False
            End If
            Threading.Thread.Sleep(333)
            If mlQueue > (MAX_NEURAL_NETWORK_THREADS * 0.9) Then
                Dim iPointInTime = mlQueue
                For x As Integer = 1 To 100
                    Threading.Thread.Sleep(333)
                    If mlQueue < iPointInTime - 2 Then Exit For 'Allow two to fall off the stack
                Next x
                lCatastrophicFailures += 1
                Log("Trying to sync this CPID again (" + sCPID + ").  Attempt # " + Trim(lCatastrophicFailures) + ".")
                GoTo TryAgain
            End If
        End Try
        If mlQueue > MAX_NEURAL_NETWORK_THREADS Then
            For x As Integer = 1 To 100
                Threading.Thread.Sleep(333)
                If mlQueue < MAX_NEURAL_NETWORK_THREADS Then Exit For
            Next x
        End If
    End Function
    Public Sub ThreadGetRac(oNeuralType As NeuralStructure)
        'Dim sLocalCPID As String = msThreadedCPID
        Dim bResult As Boolean = False
        For x = 1 To 12
            Try
                bResult = GetRAC_Resilient(oNeuralType.PK)
                If bResult Then Exit For
            Catch ex As Exception
                Log("Attempt # " + Trim(x) + ": Error in ThreadGetRAC : " + ex.Message + ", Trying again.")
            End Try
        Next x
        mlQueue -= 1
    End Sub
    Public Function GetSupermajorityVoteStatus(sCPID As String, lMinimumWitnessesRequired As Long) As Boolean
        If mdictNeuralNetworkQuorumData Is Nothing Then Return False
        Try
            For Each NS In mdictNeuralNetworkQuorumData
                If NS.Value.CPID = sCPID Then
                    If NS.Value.Witnesses > (NS.Value.Participants * 0.51) And NS.Value.Witnesses > lMinimumWitnessesRequired Then
                        Return True
                    End If
                End If
            Next
            Return False
        Catch ex As Exception
            Return False
        End Try
        Return False
    End Function
    Public Function GetSupermajorityVoteStatusForResearcher(sCPID As String, lMinimumWitnessesRequired As Long, ByRef ResearcherMagnitude As Double) As Boolean
        Return False
        If mdictNeuralNetworkAdditionalQuorumData Is Nothing Then Return False
        Try
            If mdictNeuralNetworkAdditionalQuorumData.ContainsKey(sCPID) Then
                Dim NS As GRCSec.GridcoinData.NeuralStructure = mdictNeuralNetworkAdditionalQuorumData(sCPID)
                If NS.Witnesses > (NS.Participants * 0.51) And NS.Witnesses > lMinimumWitnessesRequired Then
                    ResearcherMagnitude = NS.Magnitude
                    Return True
                End If
                Return False
            Else
                Return False
            End If
        Catch ex As Exception
            Return False
        End Try
        Return False
    End Function
    Public Function GetRAC(sCPID As String) As String
        'First try the project database:
        Dim sData As String
        Dim sNNFolder As String = GetGridFolder() + "NeuralNetwork\"
        Dim sDB As String = sNNFolder + "db.dat"
        If File.Exists(sDB) Then
            Using oStream As New System.IO.FileStream(sDB, FileMode.Open, FileAccess.Read, FileShare.Read)
                Using objReader As New System.IO.StreamReader(oStream)
                    While objReader.EndOfStream = False
                        Dim sTemp As String = objReader.ReadLine
                        Dim sLocalCPID As String = ExtractXML(sTemp, "<cpid>", "</cpid>")
                        If sLocalCPID = sCPID Then
                            sData += sTemp + vbCrLf
                        End If
                    End While
                    objReader.Close()
                End Using
            End Using
            Return sData
        End If

        'Then as a last resort, fall back to ntsoft:
        sData = ""
        Dim sURL As String = "http://boinc.netsoft-online.com/get_user.php?cpid=" + sCPID
        Dim w As New MyWebClient2
        Try
            sData = w.DownloadString(sURL)
        Catch ex As Exception

        End Try
        Return sData
    End Function
    Public Function GetRAC_Resilient(sCPID As String) As Boolean
        If sCPID = "" Then Return False
Retry:
        msCurrentNeuralHash = ""
        Dim TotalRAC As Double = 0
        Dim lFailCount As Long = 0
        If 1 = 0 Then
            Try
                'If more than 51% of the network voted on this CPIDs projects today, use that value
                If GetSupermajorityVoteStatus(sCPID, IIf(mbTestNet, MINIMUM_WITNESSES_REQUIRED_TESTNET, MINIMUM_WITNESSES_REQUIRED_PROD)) Then
                    'Use the Neural Network Quorum Data since we have over 51% witnesses for this CPID on file:
                    Dim lWitnesses As Long = 0
                    For Each nNeuralStructure In mdictNeuralNetworkQuorumData
                        If nNeuralStructure.Value.CPID = sCPID Then
                            If nNeuralStructure.Value.RAC > 10 Then PersistProjectRAC(sCPID, nNeuralStructure.Value.RAC, nNeuralStructure.Value.Project, False)
                            TotalRAC += nNeuralStructure.Value.RAC
                            If nNeuralStructure.Value.Witnesses > lWitnesses Then lWitnesses = nNeuralStructure.Value.Witnesses
                        End If
                    Next
                    UpdateCPIDStatus(sCPID, TotalRAC, lWitnesses)
                    Return True
                End If
            Catch ex As Exception
                lFailCount += 1
                If lFailCount < 12 Then
                    Threading.Thread.Sleep(400)
                    GoTo Retry
                End If
                Log("Error while updating CPID  (Attempts: 12) - Resorting to gather online rac for " + sCPID)
            End Try
        End If

        Try
            Dim sData As String = GetRAC(sCPID)

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
                Rac = RoundedRac(Val(ExtractXML(vData(y), "<expavg_credit>", "</expavg_credit>")))
                Team = LCase(Trim(ExtractXML(vData(y), "<team_name>", "</team_name>")))
                'Store the :  PROJECT_CPID, RAC
                If Rac > 10 And Team = "gridcoin" Then
                    PersistProjectRAC(sCPID, Val(Num(Trim(Rac))), sName, True)
                    TotalRAC += Rac
                End If
            Next y
            If TotalRAC = 0 Then
                PersistProjectRAC(sCPID, 0, "NeuralNetwork", True)
            End If
            UpdateCPIDStatus(sCPID, TotalRAC, 1)
            Return True

        Catch ex As Exception
            Return False
        End Try

    End Function
    Private Function UpdateCPIDStatus(sCPID As String, TotalRAC As Double, lWitnesses As Long) As Boolean
        Dim oLock As New Object
        SyncLock oLock
            Dim d As Row = New Row
            d.Expiration = Tomorrow()
            d.Added = Now
            d.Database = "CPID"
            d.Table = "CPIDS"
            d.PrimaryKey = sCPID
            d = Read(d)
            d.Expiration = DateAdd(DateInterval.Day, 14, Now)
            d.Synced = Tomorrow()
            d.RAC = TotalRAC
            d.Witnesses = lWitnesses
            Store(d)
            Return True
        End SyncLock
    End Function
    Public Function Tomorrow() As Date
        Dim dt As Date = DateAdd(DateInterval.Day, 1, Now)
        Dim dtTomorrow As Date = New Date(Year(dt), Month(dt), Day(dt), 6, 0, 0)
        Return dtTomorrow
    End Function
    Private Function PersistMagnitude(CPID As Row, Magnitude As Double, bGenData As Boolean) As Boolean
        CPID.Database = "CPID"
        CPID.Table = "CPIDS"
        CPID.Magnitude = Trim(Math.Round(Magnitude, 2))
        Store(CPID)
    End Function
    Private Function PersistProjectRAC(sCPID As String, rac As Double, Project As String, bGenData As Boolean) As Boolean
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
        Read(d)
        ' (Infinity Error)
        If Not d.Found Then
            Store(d)
            'Vote on the RAC for the project for the CPID (Once we verify > 51% of the NN agrees (by distinct IP-CPID per vote), the nodes can use this project RAC for the day to increase performance)
            Try
                If bGenData Then mGRCData.VoteOnProjectRAC(sCPID, rac, Project, mbTestNet)
            Catch ex As Exception
            End Try
        End If
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
        If Trim(s) = "" Then Return CDate("1-1-1900")
        Dim vDate() As String
        vDate = Split(s, "/")
        Dim dtOut As Date = CDate("1-1-1900")
        If UBound(vDate) >= 5 Then
            Try
                dtOut = New Date(Val(vDate(0)), Val(vDate(1)), Val(vDate(2)), Val(vDate(3)), Val(vDate(4)), Val(vDate(5)))

            Catch ex As Exception
                Return CDate("1-1-1900")
            End Try
        End If
        Return dtOut
    End Function
    Public Function DataToRow(sData As String) As Row
        Dim vData() As String
        Dim d As String = "<COL>"
        vData = Split(sData, d)
        Dim r As New Row
        r.Magnitude = "0"
        r.RAC = "0"
        r.AvgRAC = "0"

        Try

            If UBound(vData) >= 0 Then r.Added = DeserializeDate(vData(0))
            If UBound(vData) >= 1 Then r.Expiration = DeserializeDate(vData(1))
            If UBound(vData) >= 2 Then r.Synced = DeserializeDate(vData(2))
            If UBound(vData) >= 3 Then r.PrimaryKey = vData(3)
            If UBound(vData) >= 4 Then r.DataColumn1 = "" & vData(4)
            If UBound(vData) >= 5 Then r.DataColumn2 = "" & vData(5)
            If UBound(vData) >= 6 Then r.DataColumn3 = "" & vData(6)
            If UBound(vData) >= 7 Then r.DataColumn4 = "" & vData(7)
            If UBound(vData) >= 8 Then r.DataColumn5 = "" & vData(8)
            If UBound(vData) >= 9 Then r.Magnitude = "" & vData(9)
            If UBound(vData) >= 10 Then r.RAC = "" & vData(10)
            If UBound(vData) >= 11 Then r.AvgRAC = "" & vData(11)
            Try
                If UBound(vData) >= 12 Then r.Witnesses = Val("" & vData(12))
            Catch ex As Exception

            End Try
            Return r
        Catch ex As Exception
            Dim sMsg As String = ex.Message
        End Try

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
        Dim sPath As String = GetEntryName(dataRow, dataRow.PrimaryKey)
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
                        r.Found = True
                        Return r
                    End If
                End While
                objReader.Close()
            End Using

        End If
        dataRow.Found = False

        Return dataRow

    End Function
    Private Function OverwriteOriginal(sSource As String, sTarget As String)
        Try

            Dim sTargetProxy As String = sTarget + ".backup_" + Guid.NewGuid().ToString() + ".txt"
            If File.Exists(sTarget) Then
                Rename(sTarget, sTargetProxy)
            End If
            If File.Exists(sSource) Then
                FileCopy(sSource, sTarget)
            End If
            'Clean up
            Try
                Kill(sTargetProxy)
            Catch ex As Exception
            End Try
        Catch ex As Exception
            Dim sMsg As String = ex.Message
        End Try
    End Function
    Public Function Insert(dataRow As Row, bOnlyWriteIfNotFound As Boolean)
        Dim sPath As String = GetEntryName(dataRow, dataRow.PrimaryKey)
        Dim oLock As New Object
        Dim sNewFileName As String
        Dim sTemp As String = ""
        Dim d As String = "<COL>"
        Dim bFound As Boolean
        If dataRow.Added = New Date Then dataRow.Added = Now
        SyncLock oLock
            'FileMode.Create
            Dim oFileOutStream As New System.IO.FileStream(sPath, FileMode.Create, FileAccess.Write, FileShare.ReadWrite)
            Using objWriter As New System.IO.StreamWriter(oFileOutStream)
                objWriter.WriteLine(SerializeRow(dataRow))
                objWriter.Close()
            End Using
        End SyncLock
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
            + d + SerStr(dataRow.Magnitude) + d + SerStr(dataRow.RAC) + d + SerStr(dataRow.AvgRAC) + d + SerStr(dataRow.Witnesses)
        Return sSerialized
    End Function

    Public Function GetPath(dataRow As Row) As String
        Dim sFilename As String = LCase(dataRow.Database) + "_" + LCase(dataRow.Table) + ".dat"
        Dim sPath As String = GetGridFolder() + "NeuralNetwork\" + dataRow.Table + "\"
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
    Public Function GetEntryName(dataRow As Row, sPrimaryKey As String) As String
        Dim sFilename As String = LCase(dataRow.Database) + "_" + LCase(dataRow.Table) + "_" + sPrimaryKey + ".dat"
        Dim sPath As String = GetGridFolder() + "NeuralNetwork\" + dataRow.Table + "\"
        Try
            If Not System.IO.Directory.Exists(sPath) Then MkDir(sPath)
        Catch ex As Exception
            Log("Unable to create folder " + sPath)
        End Try

        Return sPath + sFilename
    End Function

    Public Function GetEntryPrefix(dataRow As Row) As String
        Dim sFilename As String = LCase(dataRow.Database) + "_" + LCase(dataRow.Table) + "_"
        Return sFilename
    End Function

    Public Function Store(dataRow As Row) As Boolean
        Dim sErr As String = ""
        Dim lFailCount As Long = 0
Retry:
        Dim oLock As New Object
        Try
            SyncLock oLock
                Insert(dataRow, False)
            End SyncLock

        Catch ex As Exception
            Log("Attempt #" + Trim(lFailCount) + " While storing data row " + dataRow.Table + "," + dataRow.Database + ", PK: " + dataRow.PrimaryKey + " : " + ex.Message + ", Retrying.")
            lFailCount += 1
            If lFailCount < 100 Then
                GoTo Retry
                Threading.Thread.Sleep(100)
            End If
            Return False
        End Try
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
    Public Function ExplainNeuralNetworkMagnitudeByCPID(sCPID As String) As String
        Dim lstWhitelist As List(Of Row)
        Dim surrogateWhitelistRow As New Row
        surrogateWhitelistRow.Database = "Whitelist"
        surrogateWhitelistRow.Table = "Whitelist"
        lstWhitelist = GetList(surrogateWhitelistRow, "*")
        Dim rPRJ As New Row
        rPRJ.Database = "Project"
        rPRJ.Table = "Projects"
        Dim lstProjects1 As List(Of Row) = GetList(rPRJ, "*")
        lstProjects1.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim WhitelistedProjects As Double = GetWhitelistedCount(lstProjects1, lstWhitelist)
        Dim TotalProjects As Double = lstProjects1.Count
        Dim PrjCount As Double = 0
        lstWhitelist.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim TotalRAC As Double = 0
        If sCPID.Contains("Hash") Then Exit Function
        If Len(sCPID) < 10 Then Exit Function
        Dim sOut As String

        Dim sHeading As String = "CPID,Project,RAC,Project_Total_RAC,Project_Avg_RAC,Project Mag,Cumulative RAC,Cumulative Mag,Errors"
        sOut += sHeading + "<ROW>"

        Dim vHeading() As String = Split(sHeading, ",")
        Dim surrogatePrj As New Row
        surrogatePrj.Database = "Project"
        surrogatePrj.Table = "Projects"
        Dim lstProjects As List(Of Row) = GetList(surrogatePrj, "*")
        Dim iRow As Long = 0
        Dim CumulativeMag As Double = 0
        Dim sRow As String = ""
        Dim sErr As String = ""

        For Each prj As Row In lstProjects
            Dim surrogatePrjCPID As New Row
            surrogatePrjCPID.Database = "Project"
            surrogatePrjCPID.Table = prj.PrimaryKey + "CPID"
            surrogatePrjCPID.PrimaryKey = prj.PrimaryKey + "_" + sCPID
            Dim rowRAC = Read(surrogatePrjCPID)
            Dim CPIDRAC As Double = Val(rowRAC.RAC)
            Dim PrjRAC As Double = Val(prj.RAC)
            If CPIDRAC > 0 Then
                iRow += 1
                '7-26-2015
                sRow = sCPID + "," + prj.PrimaryKey + "," + Trim(CPIDRAC) + "," + Trim(PrjRAC) + "," + Trim(prj.AvgRAC)
                'Cumulative Mag:
                Dim bIsThisWhitelisted As Boolean = False
                bIsThisWhitelisted = IsInList(prj.PrimaryKey, lstWhitelist, False)
                Dim IndMag As Double = 0
                sErr = ""
                If Not bIsThisWhitelisted Then
                    sErr = "Not Whitelisted"
                End If
                If bIsThisWhitelisted Then
                    IndMag = Math.Round(((CPIDRAC / (PrjRAC + 0.01)) / (WhitelistedProjects + 0.01)) * NeuralNetworkMultiplier, 2)
                    CumulativeMag += IndMag
                    TotalRAC += CPIDRAC
                End If
                sRow += "," + Trim(IndMag) + "," + Trim(TotalRAC)
                sRow += "," + Trim(CumulativeMag) + "," + sErr
                sOut += sRow + "<ROW>"

            End If
        Next
        Dim sSignature As String = ""
        Dim sNeuralCPID As String = KeyValue("PrimaryCPID")
        If sNeuralCPID = "" Then sNeuralCPID = "Unknown"
        Dim sNH As String = mclsUtilization.GetNeuralHash
        If sNeuralCPID = "" Then sNeuralCPID = mclsUtilization.GetNeuralHash
        sSignature = "NN Host Version: " + Trim(mclsUtilization.Version) + ", NeuralHash: " + sNH + ", SignatureCPID: " + sNeuralCPID + ", Time: " + Trim(Now)
        sRow = "Total RAC: " + Trim(TotalRAC) + "<ROW>" + "Total Mag: " + Trim(Math.Round(CumulativeMag, 2))
        sOut += sSignature + "<ROW>" + sRow
        sRow += "<ROW>Your Neural Magnitude: " + Trim(RoundedMag(CumulativeMag))
        'Dim sXML As String = GetXMLOnly(sCPID)
        Return sOut

    End Function


    Public Sub ExportToCSV2()
        Try

            Dim sReport As String = ""
            Dim sReportRow As String = ""
            Dim sHeader As String = "CPID,LocalMagnitude,NeuralMagnitude,TotalRAC,Synced Til,Address,CPID_Valid,Witnesses"
            sReport += sHeader + vbCrLf
            Dim grr As New GridcoinReader.GridcoinRow
            Dim sHeading As String = "CPID;LocalMagnitude;NeuralMagnitude;TotalRAC;Synced Til;Address;CPID_Valid;Witnesses"
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
            sMyNeuralHash = GetQuorumHash(sContract)
            sReport += "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"
            Dim sWritePath As String = GetGridFolder() + "reports\DailyNeuralMagnitudeReport.csv"
            If Not System.IO.Directory.Exists(GetGridFolder() + "reports") Then MkDir(GetGridFolder() + "reports")
            Using objWriter As New System.IO.StreamWriter(sWritePath)
                objWriter.WriteLine(sReport)
                objWriter.Close()
            End Using
            'Mass Export for RTM 11-7-2015
            Dim sMassWritePath As String = GetGridFolder() + "reports\MassExport.dat"
            Dim sTemp As String = ""
            Dim oLock As New Object
            Using objWriter As New System.IO.StreamWriter(sMassWritePath)
                Dim sNNFolder As String = GetGridFolder() + "NeuralNetwork\"
                SyncLock oLock
                    Dim di As New DirectoryInfo(sNNFolder)
                    Dim fiArr As FileInfo() = di.GetFiles()
                    Dim fi As FileInfo
                    For Each fi In fiArr
                        If LCase(fi.Name) Like "cpid_*" Or LCase(fi.Name) Like "project_*" Then

                            Using Stream As New System.IO.FileStream(fi.FullName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)
                                Dim objReader As New System.IO.StreamReader(Stream)
                                While objReader.EndOfStream = False
                                    sTemp = objReader.ReadLine
                                    Dim sExport As String = fi.Name + "<COL>" + sTemp
                                    objWriter.WriteLine(sExport)
                                End While
                                objReader.Close()
                            End Using
                        End If

                    Next fi

                End SyncLock
            End Using
        Catch ex As Exception
            Log("Error while exportingToCSV2: " + ex.Message)
        End Try

    End Sub

    Private Function NiceTicker(sSymbol As String)
        sSymbol = Replace(Replace(sSymbol, "$", ""), "^", "")
        Return sSymbol
    End Function

    Public Function GetCryptoPrice(sSymbol As String) As Quote

        Try

            'Sample Ticker Format :  {"ticker":{"high":0.00003796,"low":0.0000365,"avg":0.00003723,"lastbuy":0.0000371,"lastsell":0.00003795,"buy":0.00003794,"sell":0.00003795,"lastprice":0.00003795,"updated":1420369200}}
            Dim sSymbol1 As String
            sSymbol1 = NiceTicker(sSymbol)
            Dim ccxPage As String = ""
            If sSymbol1 = "BTC" Then
                ccxPage = "btc-usd.json"
            Else
                ccxPage = LCase(sSymbol1) + "-btc.json"
            End If
            Dim sURL As String = "https://c-cex.com/t/" + ccxPage
            Dim w As New MyWebClient
            Dim sJSON As String = String.Empty
            Try
                sJSON = w.DownloadString(sURL)

            Catch ex As Exception

            End Try
            Dim sLast As String = ExtractValue(sJSON, "lastprice", "updated")
            sLast = Replace(sLast, ",", ".")

            Dim dprice As Double
            dprice = CDbl(sLast)
            Dim qBitcoin As Quote
            qBitcoin = GetQuote("$BTC")
            If sSymbol1 <> "BTC" And qBitcoin.Price > 0 Then dprice = dprice * qBitcoin.Price
            Dim q As Quote
            q = GetQuote(sSymbol)
            q.Symbol = sSymbol
            q.PreviousClose = q.Price
            Dim Variance As Double
            Variance = Math.Round(dprice, 3) - q.PreviousClose
            q.Variance = Variance
            q.Price = Math.Round(dprice, 11)
            Return q

        Catch ex As Exception
            Log("Unable to get quote data probably due to SSL being blocked: " + ex.Message)
        End Try
    End Function
    Public Function GetWindowsFileAge(sPath As String) As Double
        If File.Exists(sPath) = False Then Return 1000000
        Dim fi As New FileInfo(sPath)
        If fi.Length = 0 Then Return 1000000
        Dim iMins As Long = DateDiff(DateInterval.Minute, fi.LastWriteTime, Now)
        Return iMins
    End Function
    Public Function GetUtcDateTime(sDateTime As String) As DateTime
        'When converting from a date time to a target datetime that falls within the window of a daylight savings time adjustment, an error is thrown since the target is technically no longer a valid date time, so we need to account for this just in case
        'On July2nd 2017, the Western Sahara had a 2AM to 3AM local time change and an error was thrown when we pulled in a RAC timestamp of 2AM JUL 2 2017 and tried to convert it to UTC
        'Step 1, try the natural conversion first
        If sDateTime = "" Then Return CDate("1-1-1970")
        Dim dtTime As DateTime = CDate("1-1-1970")
        Try
            dtTime = CDate(sDateTime)
        Catch ex As Exception
            Log("Error while converting #" + Trim(sDateTime) + " to DateTime")
            Return CDate("1-1-1970")
        End Try
        Try
            Dim dTime As DateTime = TimeZoneInfo.ConvertTimeToUtc(dtTime)
            Return dTime
        Catch ex As Exception
            'Next try 2 hours back
            Log("Setting clock 2 hours back.")
            Try
                Dim dTime2 As DateTime = DateAdd(DateInterval.Hour, -2, dtTime)
                Dim dTime2Return As DateTime = TimeZoneInfo.ConvertTimeToUtc(dTime2)
                Return dTime2Return
            Catch ex2 As Exception
                Log("Still unable to convert to UTC from " + Trim(dtTime))
                Return CDate("1-1-1970")
            End Try
        End Try
    End Function
    Public Function GetRowAgeInMins(sRow As String, dtSyncTime As DateTime) As Double
        Dim sTS As String = ExtractXML(sRow, "<expavg_time>", "</expavg_time>")
        Dim dStamp As Double = Val(sTS)
        Dim dTime As DateTime = UnixTimestampToDate(dStamp)
        Dim iMins As Long = DateDiff(DateInterval.Minute, dTime, dtSyncTime)
        Return iMins
    End Function



    Public Function GetUnixFileAge(sPath As String) As Double
        If File.Exists(sPath) = False Then Return 1000000
        Dim fi As New FileInfo(sPath)
        If fi.Length = 0 Then Return 1000000
        Dim sr As New StreamReader(sPath)
        Dim dMaxStamp As Double = 0
        Dim sTemp As String = ""
        While sr.EndOfStream = False
            sTemp = sr.ReadLine
            Dim sTS As String = ExtractXML(sTemp, "<expavg_time>", "</expavg_time>")
            Dim dStamp As Double = Val(sTS)
            If dStamp > 0 And dStamp > dMaxStamp Then
                dMaxStamp = dStamp
            End If
        End While
        sr.Close()
        Log("GUFA Timestamp: " + Trim(dMaxStamp))
        Dim dTime As DateTime = UnixTimestampToDate(dMaxStamp)
        Dim iMins As Long = DateDiff(DateInterval.Minute, dTime, DateTime.UtcNow)
        Return iMins
    End Function
    Public Function GetQuorumHash(data As String)
        Return mclsQHA.QuorumHashingAlgorithm(data)
    End Function
End Module

Public Class MyWebClient2
    Inherits System.Net.WebClient
    Private timeout As Long = 5000
    Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
        Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
        w.Timeout = timeout
        Return (w)
    End Function
End Class