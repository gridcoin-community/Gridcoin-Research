
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
    Public mdLastSync As Date = DateAdd(DateInterval.Day, -10, Now)
    Public mlPercentComplete As Double = 0
    Public msContractDataForQuorum As String
    Public NeuralNetworkMultiplier As Double = 115000
    Private mclsQHA As New clsQuorumHashingAlgorithm

    Private lUseCount As Long = 0

    Public Structure NeuralStructure
        Public PK As String
        Public NeuralValue As Double
        Public Witnesses As Double
        Public Updated As DateTime
    End Structure
    'Public mdictNeuralNetworkMemories As Dictionary(Of String, GRCSec.GridcoinData.NeuralStructure)

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

    End Structure
    Public Structure CPID
        Public cpid As String
        Public RAC As Double
        Public Magnitude As Double
        Public Expiration As Date
    End Structure
    Public msCurrentNeuralHash = ""
    Private Function Num(sData As String) As String
        'Ensures culture is neutral
        Dim sOut As String
        sOut = Trim(Math.Round(RoundedMag(Val("0" + Trim(sData))), 2))
        sOut = Replace(sOut, ",", ".")
        Return sOut
    End Function

    Public Sub ReconnectToNeuralNetwork()
        Try
            mGRCData = New GRCSec.GridcoinData

        Catch ex As Exception
            Log("Unable to connect to neural network.")
        End Try
    End Sub
    Public Function GetMagnitudeContractDetails() As String
        '8-8-2015: Retrieve true magnitude average from all nodes

        ' If mdictNeuralNetworkMemories Is Nothing Then
        ' Try
        ' ReconnectToNeuralNetwork()
        ' Dim sMemoryName = IIf(mbTestNet, "magnitudes_testnet", "magnitudes")
        ' mdictNeuralNetworkMemories = mGRCData.GetNeuralNetworkQuorumData(sMemoryName)
        ' Catch ex As Exception
        ' Log("Unable to connect to neural network for memories.")
        ' End Try
        ' End If

        Dim surrogateRow As New Row
        surrogateRow.Database = "CPID"
        surrogateRow.Table = "CPIDS"
        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim sOut As String = ""
        For Each cpid As Row In lstCPIDs
            Dim dNeuralMagnitude As Double = 0
            '   Try
            'dNeuralMagnitude = mdictNeuralNetworkMemories(cpid.PrimaryKey).NeuralValue
            'Catch ex As Exception
            'End Try
            Dim sRow As String = cpid.PrimaryKey + "," + Num(cpid.Magnitude) _
                                 + "," + Num(dNeuralMagnitude) + "," + Num(cpid.RAC) _
                                 + "," + Trim(cpid.Synced) + "," + Trim(cpid.DataColumn4) _
                                 + "," + Trim(cpid.DataColumn5) + ";"
            sOut += sRow
        Next
        surrogateRow.Database = "Prices"
        surrogateRow.Table = "Quotes"
        lstCPIDs = GetList(surrogateRow, "*")

        For Each cpid As Row In lstCPIDs
            Dim dNeuralMagnitude As Double = 0
            Dim sRow As String = cpid.PrimaryKey + "," + Num(cpid.Magnitude) _
                                 + "," + Num(dNeuralMagnitude) + "," + Num(cpid.RAC) _
                                 + "," + Trim(cpid.Synced) + "," + Trim(cpid.DataColumn4) _
                                 + "," + Trim(cpid.DataColumn5) + ";"
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
        Try

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
        For Each cpid As Row In lstCPIDs
            If cpid.DataColumn5 = "True" Then
                    Dim sRow As String = cpid.PrimaryKey + "," + Num(cpid.Magnitude) + ";"
                    lTotal = lTotal + Val("0" + Trim(cpid.Magnitude))
                    lRows = lRows + 1
                    sOut += sRow
                    dMagAge = 0
                    Try
                        dMagAge = Math.Abs(DateDiff(DateInterval.Minute, Now, cpid.Added))

                    Catch ex As Exception
                    End Try
                    If dMagAge > (60 * 12) Then Return ""

                Else
                    Dim sRow As String = cpid.PrimaryKey + ",00;"
                    lTotal = lTotal + 0
                    lRows = lRows + 1
                    sOut += sRow
                End If
        Next
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
        If avg < 25 Then Return ""
            'APPEND the Averages:

            Dim surrogateWhitelistRow As New Row
            Dim lstWhitelist As List(Of Row)
            surrogateWhitelistRow.Database = "Whitelist"
            surrogateWhitelistRow.Table = "Whitelist"
            lstWhitelist = GetList(surrogateWhitelistRow, "*")


        Dim rRow As New Row
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
                        sOut += sRow
                    End If
                End If

            Next
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
                                'Dim bResult As Boolean = GetRacViaNetsoft(dr.PrimaryKey)
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
                Dim prjTotalRAC = 0
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
    Public Sub SyncDPOR2()
        Dim t As New Threading.Thread(AddressOf CompleteSync)
        t.Start()

    End Sub
    Public Function NeedsSynced(dr As Row) As Boolean

        If Now > dr.Synced Then Return True Else Return False
    End Function
    Public Sub CompleteSync()
        If bMagsDoneLoading = False Then
            If Math.Abs(DateDiff(DateInterval.Second, Now, mdLastSync)) > 60 * 10 Then
                bMagsDoneLoading = True
            Else
                Exit Sub
            End If
        End If

        mbForcefullySyncAllRac = True

        Try
            msCurrentNeuralHash = ""

            bMagsDoneLoading = False

            Try
                mGRCData = New GRCSec.GridcoinData

            Catch ex As Exception

            End Try
           
            Try
                mlPercentComplete = 1
                UpdateMagnitudesPhase1()

            Catch ex As Exception
                Log("Err in completesync" + ex.Message)

            End Try
            Log("Updating mags")
            Try
                mlPercentComplete = 10

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

        bMagsDoneLoading = True
        mdLastSync = Now
        mlPercentComplete = 0
        '7-21-2015: Store historical magnitude so it can be charted
        StoreHistoricalMagnitude()

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
            mGRCData.CheckInWithNeuralNetwork(sCPID, "")
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
    Public Sub StoreTestMagnitude()
        For x = 1 To 25
            UpdateKey("PrimaryCPID", "784afb35d92503160125feb183157378")

            Dim dtTest As DateTime = DateAdd(DateInterval.Day, -x, Now)

            Dim sCPID As String = KeyValue("PrimaryCPID")
            If Len(sCPID) = 0 Then Exit Sub 'If we dont know the primary cpid
            Dim MyCPID As Row = GetMagByCPID(sCPID)
            'Store the Historical Magnitude:
            Dim d As New Row
            d.Expiration = DateAdd(DateInterval.Day, 30, Now)
            d.Added = Now
            d.Database = "Historical"
            d.Table = "Magnitude"
            d.PrimaryKey = sCPID + GRCDay(dtTest)
            d.Magnitude = x + 1100
            d.RAC = MyCPID.RAC
            d.AvgRAC = MyCPID.AvgRAC
            Store(d)
            d.PrimaryKey = "Network" + GRCDay(dtTest)
            d.AvgRAC = 400 + x
            d.Magnitude = 500 + x
            Store(d)
        Next

    End Sub
    Public Function GetDataValue(sDB As String, sTable As String, sPK As String) As Row
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
    Private Function BlowAwayTable(dr As Row)
        Dim sPath As String = GetPath(dr)
        Try
            Kill(sPath)
        Catch ex As Exception
            Log("Neural UpdateMagnitudesPhase1:" + ex.Message)
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
                Log("UM Phase 1: While loading projects " + ex.Message)
            End Try
            mlPercentComplete = 5
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
            mlPercentComplete = 6

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

    Public Function UpdateMagnitudes() As Boolean
        Dim lstCPIDs As List(Of Row)
        Dim surrogateRow As New Row
        Dim WhitelistedProjects As Double = 0
        Dim ProjCount As Double = 0
        Dim lstWhitelist As List(Of Row)

        Dim iRow As Long = 0
        Try
            'Loop through the researchers
            surrogateRow.Database = "CPID"
            surrogateRow.Table = "CPIDS"
            lstCPIDs = GetList(surrogateRow, "*")
            lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
            Dim lstProjectsCt As List(Of Row) = GetList(surrogateRow, "*")
            For Each cpid As Row In lstCPIDs
                If NeedsSynced(cpid) Or mbForcefullySyncAllRac Then
                    Dim bResult As Boolean = GetRacViaNetsoft(cpid.PrimaryKey)
                End If
                iRow += 1
                Dim p As Double = (iRow / (lstCPIDs.Count + 0.01)) * 100
                mlPercentComplete = p

            Next
        Catch ex As Exception
            Log("UpdateMagnitudes:GatherRAC: " + ex.Message)
        End Try

        Try
            UpdateNetworkAverages()
        Catch ex As Exception
            Log("UpdateMagnitudes:UpdateNetworkAverages: " + ex.Message)
        End Try

        lstWhitelist = GetWPC(WhitelistedProjects, ProjCount)
        lstCPIDs.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Try
            mGRCData = New GRCSec.GridcoinData

        Catch ex As Exception

        End Try
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
        Store(q)
        q = New Row
        q.Database = "Prices"
        q.Table = "Quotes"
        q.Expiration = DateAdd(DateInterval.Day, 1, Now)
        q.PrimaryKey = "GRC"
        q.Magnitude = Trim(Math.Round(dGRC, 2))
        q.Synced = q.Expiration

        Store(q)


        'Update all researchers magnitudes:
        Try
            Dim iRow2 As Long

            lstCPIDs = GetList(surrogateRow, "*")
            For Each cpid As Row In lstCPIDs
                Dim surrogatePrj As New Row
                surrogatePrj.Database = "Project"
                surrogatePrj.Table = "Projects"
                Dim lstProjects As List(Of Row) = GetList(surrogatePrj, "*")
                Dim TotalRAC As Double = 0
                Dim TotalNetworkRAC As Double = 0
                Dim TotalMagnitude As Double = 0
                'Dim TotalRAC As Double = 0
                For Each prj As Row In lstProjects
                    Dim surrogatePrjCPID As New Row
                    If IsInList(prj.PrimaryKey, lstWhitelist, False) Then
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
                cpid.Database = "CPID"
                cpid.Table = "CPIDS"
                cpid.Magnitude = Trim(Math.Round(TotalMagnitude, 2))
                If TotalMagnitude < 1 And TotalMagnitude > 0.25 Then cpid.Magnitude = Trim(1)

                'Try
                'mGRCData.BroadcastNeuralNetworkMemoryValue(sMemoryName, cpid.PrimaryKey, Val(cpid.Magnitude), False)
                'Catch ex As Exception
                'End Try
                Store(cpid)
            Next
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
    Public Function IsInList(sData As String, lstRows As List(Of Row), bRequireExactMatch As Boolean) As Boolean
        For Each oRowIterator As Row In lstRows
            If Trim(UCase(oRowIterator.PrimaryKey)) = Trim(UCase(sData)) Then
                Return True
            End If
            If Not bRequireExactMatch Then
                Dim sCompare1 As String = StringStandardize(oRowIterator.PrimaryKey)
                Dim sCompare2 As String = StringStandardize(sData)
                If Left(sCompare1, 12) = Left(sCompare2, 12) Then Return True
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
            d.RAC = TotalRAC
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
        Read(d)
        ' (Infinity Error)
        If Not d.Found Then
            Store(d)
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
        r.Magnitude = "0"
        r.RAC = "0"
        r.AvgRAC = "0"

        If UBound(vData) >= 4 Then r.DataColumn1 = "" & vData(4)
        If UBound(vData) >= 5 Then r.DataColumn2 = "" & vData(5)
        If UBound(vData) >= 6 Then r.DataColumn3 = "" & vData(6)
        If UBound(vData) >= 7 Then r.DataColumn4 = "" & vData(7)
        If UBound(vData) >= 8 Then r.DataColumn5 = "" & vData(8)
        If UBound(vData) >= 9 Then r.Magnitude = "" & vData(9)
        If UBound(vData) >= 10 Then r.RAC = "" & vData(10)
        If UBound(vData) >= 11 Then r.AvgRAC = "" & vData(11)

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
            + d + SerStr(dataRow.Magnitude) + d + SerStr(dataRow.RAC) + d + SerStr(dataRow.AvgRAC)

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
        sRow += "<ROW>Your Neural Magnitude: " + Trim(Math.Round(CumulativeMag, 2))
        'Dim sXML As String = GetXMLOnly(sCPID)
        Return sOut

    End Function


    Public Sub ExportToCSV2()
        Try

            Dim sReport As String = ""
            Dim sReportRow As String = ""
            Dim sHeader As String = "CPID,LocalMagnitude,NeuralMagnitude,TotalRAC,Synced Til,Address,CPID_Valid"
            sReport += sHeader + vbCrLf
            Dim grr As New GridcoinReader.GridcoinRow
            Dim sHeading As String = "CPID;LocalMagnitude;NeuralMagnitude;TotalRAC;Synced Til;Address;CPID_Valid"
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
            Dim sJSON As String = w.DownloadString(sURL)
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


    Public Function GetQuorumHash(data As String)
        Return mclsQHA.QuorumHashingAlgorithm(data)
    End Function

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