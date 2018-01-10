Imports Microsoft.VisualBasic
Imports System.Timers
Imports System.IO
Imports System.Windows.Forms

Public Class Utilization
    Implements IGridcoinMining

    Private _nBestBlock As Long
    Private _lLeaderboard As Long
    Private _lLeaderUpdates As Long
    Public _boincmagnitude As Double
    Private msSentence As String = ""
    Private mlSpeakMagnitude As Double
    Public ReadOnly Property Version As Double
        Get
            Return 425
        End Get
    End Property

    Private lfrmMiningCounter As Long = 0
    Public Function cGetCryptoPrice(sSymbol As String) As Double
        Return GetCryptoPrice(sSymbol).Price
    End Function
    Public Function cqGetCryptoPrice(sSymbol As String) As Quote
        Return GetCryptoPrice(sSymbol)
    End Function
    Public Function SetQuorumData(sData As String) As String
        Dim sQuorumData As String = ExtractXML(sData, "<QUORUMDATA>")
        Dim sAge As String = ExtractXML(sQuorumData, "<AGE>")
        Dim sQuorumHash As String = ExtractXML(sQuorumData, "<HASH>")
        Dim TS As String = ExtractXML(sQuorumData, "<TIMESTAMP>")
        Dim sBlock As String = ExtractXML(sQuorumData, "<BLOCKNUMBER>")
        Dim sPrimaryCPID As String = ExtractXML(sQuorumData, "<PRIMARYCPID>")
        Log(sData)
        Call UpdateSuperblockAgeAndQuorumHash(sAge, sQuorumHash, TS, sBlock, sPrimaryCPID)
        Return ""
    End Function
    Public Sub TestGZIPBoincDownload()
        Dim c As New clsBoincProjectDownload
        c.DownloadGZipFiles()
    End Sub
    Public Function WriteKey(sData As String) As String
        Try
            Dim sKey As String = ExtractXML(sData, "<KEY>")
            Dim sValue As String = ExtractXML(sData, "<VALUE>")
            UpdateKey(sKey, sValue)
            Return "True"
        Catch ex As Exception
            Return "False"
        End Try
    End Function
    Public Function GetDotNetMessages(sDataType As String) As String
        Dim sReply As String = msRPCCommand
        msRPCCommand = ""
        Return sReply
    End Function
    Public Function SetRPCResponse(sResponse) As Double
        SetRPCReply(sResponse)
        Return 1
    End Function
    Public Function FromBase64String(sData As String) As String
        Dim ba() As Byte = Convert.FromBase64String(sData)
        Dim sOut As String = ByteToString(ba)
        Return sOut
    End Function
    Public Function ByteToString(b() As Byte)
        Dim sReq As String
        sReq = System.Text.Encoding.UTF8.GetString(b)
        Return sReq
    End Function
    Public Function StringToByteArray(sData As String) As Byte()
        Return modGRC.StringToByte(sData)
    End Function
    Public Function BoincMagnitude(value As String) As String
        _boincmagnitude = Val(value)
        Return ""
    End Function
    Public Function cAES512Encrypt(sData As String) As String
        Return AES512EncryptData(sData)
    End Function
    Public Function cAES512Decrypt(sData As String) As String
        Return AES512DecryptData(sData)
    End Function
    Public Sub StartWireFrameRenderer()
        Dim thWireFrame As New Threading.Thread(AddressOf ThreadWireFrame)
        thWireFrame.Priority = Threading.ThreadPriority.Lowest
        thWireFrame.Start()
    End Sub
    Public Sub StartGalaza()
        Dim p As New Process
        p = Process.Start(GetGRCAppDir() + "\Galaza\GridcoinGalaza.exe")
    End Sub
    Public Sub StopWireFrameRenderer()
        If Not mfrmWireFrame Is Nothing Then
            mfrmWireFrame.EndWireFrame()
        End If
    End Sub
    Public Sub TestnetSetGenericRPCValue(sData As String)
        SetRPCReply(sData)
    End Sub
    Public Function TestnetGetGenericRPCValue() As String
        Return ""
    End Function
    Public ReadOnly Property ClientNeedsUpgrade As Double
        Get
            Dim bNeedsUp As Boolean = NeedsUpgrade()
            If bNeedsUp Then
                Log("Client outdated; needs upgraded[2].")
                Dim sLastUpgraded As String = KeyValue("AutoUpgrade")
                If Len(sLastUpgraded) > 0 Then
                    Log("Found key AutoUpgrade " + Trim(sLastUpgraded))

                    Dim dDiff As Long
                    dDiff = DateDiff(DateInterval.Day, Now, CDate(sLastUpgraded))
                    If Math.Abs(dDiff) < 1 Then
                        Log("Upgraded too recently. Aborting. " + Trim(dDiff))
                        Return 0
                    End If
                End If
                Log("Ready for upgrade")
                If KeyValue("suppressupgrade") = "true" Then
                    Log("Client needs upgraded; Not upgrading due to key.")
                    Return 0
                End If
                'Set a key to prevent multiple upgrades
                UpdateKey("AutoUpgrade", Trim(Now))
                Return 1
            End If
            Log("Client up to date")
            Return 0
        End Get
    End Property
    Public Function NeuralNetwork() As Double
        Return 1999
    End Function

    Sub New()
        mclsUtilization = Me
        Try
            Try
                UpdateKey("UpdatingLeaderboard", "false")
            Catch ex As Exception
                Log(ex.Message)
            End Try
            Try
                PurgeLog()
            Catch ex As Exception
            End Try
            Log("Loading...")
            Try
            Catch ex As Exception
                Log("New:" + ex.Message)
            End Try
        Catch ex As Exception
            Log("While loading clsUtilization : " + ex.Message)
        End Try
        Log("Loaded")
    End Sub
    Sub New(bLoadMiningConsole As Boolean)
        If bLoadMiningConsole Then ShowMiningConsole()
    End Sub
    Public Sub RestartWallet()
        Call RestartWallet1("")
    End Sub
    Public Sub UpgradeWallet()
        Call RestartWallet1("upgrade")
    End Sub
    Public Sub UpgradeWalletTestnet()
        Call RestartWallet1("testnetupgrade")
    End Sub
    Public Sub ReindexWallet()
        Call RestartWallet1("reindex")
    End Sub
    Public Sub RebootClient()
        Call RestartWallet1("reboot")
    End Sub
    Public Sub DownloadBlocks()
        If mbTestNet Then
            Call RestartWallet1("downloadblocks testnet")
        Else
            Call RestartWallet1("downloadblocks")
        End If
    End Sub
    Public Sub ReindexWalletTestNet()
        Call RestartWallet1("reindextestnet")
    End Sub
    Public Sub RestoreSnapshot()
        Call RestartWallet1("restoresnapshot")
    End Sub
    Public Sub CreateRestorePoint()
        Call RestartWallet1("createrestorepoint")
    End Sub
    Public Sub CreateRestorePointTestNet()
        Call RestartWallet1("createrestorepointtestnet")
    End Sub
    Public Function cGetMd5(sData As String) As String
        Return GetMd5String(sData)
    End Function
    Public Function GetMd5FromBytes(b() As Byte) As String
        Return GetMd5String(b)
    End Function
    Public Function StrToMd5Hash(s As String) As String
        Return CalcMd5(s)
    End Function
    Public Function GetNeuralHash() As String
        If Len(msCurrentNeuralHash) > 1 Then Return msCurrentNeuralHash 'This is invalidated when it changes
        Dim sContract As String = GetMagnitudeContract()
        '7-25-2015 - Use Quorum Hashing algorithm to get the quorum hash 
        Dim clsQHA As New clsQuorumHashingAlgorithm
        Dim sHash As String = clsQHA.QuorumHashingAlgorithm(sContract)
        ' Dim contractsDir As String = Path.Combine(GetGridFolder(), "contracts")

        '  If Directory.Exists(contractsDir) = False Then
        ' Directory.CreateDirectory(contractsDir)
        ' End If
        ' Dim contractPath As String = Path.Combine(contractsDir, sHash, ".txt")
        ' If File.Exists(contractPath) = False Then
        ' File.WriteAllText(contractPath, sContract)
        ' End If

        Return sHash
    End Function
    Public Sub ExportToCSVFile()
        ExportToCSV2()
    End Sub
    Public Function GetNeuralContract() As String
        Dim sContract As String = GetMagnitudeContract()
        Return sContract
    End Function
    Public Function ShowVotingConsole() As Double
        Dim fmVoting As New frmVoting
        fmVoting.Show()
        Return 1
    End Function
    Public Function ShowForm(sFormName As String) As String
        Try
            Dim vFormName() As String
            vFormName = Split(sFormName, ",")
            Log("Showing " + sFormName)

            If UBound(vFormName) > 0 Then
                sFormName = vFormName(0)
                msPayload = vFormName(1)
                Log("Showing form with payload " + msPayload)

            Else
                msPayload = ""
            End If

            Dim sMyName As String = System.Reflection.Assembly.GetExecutingAssembly.GetName.Name
            Dim obj As Object = Activator.CreateInstance(Type.GetType(sMyName + "." + sFormName))
            obj.Show()
            Return "1"
        Catch ex As Exception
            Log("Unable to show " + sFormName + " because of " + ex.Message)
            Return "0"
        End Try

    End Function
    Public Function ShowNewUserWizard() As Double
        Dim fNUW As New frmNewUserWizard
        fNUW.Show()
        Return 1
    End Function
    Public Function ShowConfig() As Double
        Try
            mfrmConfig = New frmConfiguration
            mfrmConfig.Show()
        Catch ex As Exception
            Log("Error while transitioning to frmConfig" + ex.Message)
        End Try
        Return 1
    End Function
    Public Function ShowFAQ() As Double
        Try
            mfrmFAQ = New frmFAQ
            mfrmFAQ.Show()

        Catch ex As Exception
            Log("Error:FAQ")
        End Try
        Return 1
    End Function
    Public Function ShowTicketAdd() As Double
        Try
            mfrmTicketAdd = New frmTicketAdd
            mfrmTicketAdd.Show()
        Catch ex As Exception
            Log("Error while transitioning to frmTicketAdd" + ex.Message)
        End Try
        Return 1
    End Function
    Public Function ShowDiagnostics() As Double
        Try
            mFrmDiagnostics = New frmDiagnostics
            mFrmDiagnostics.Show()
        Catch ex As Exception
            Log("Error while showing Diagnostics" + ex.Message)
        End Try
        Return 1
    End Function
    Public Function muFileToBytes(SourceFile As String) As Byte()
        Return FileToBytes(SourceFile)
    End Function
    Public Function ShowFoundation() As Double
        Try
            mfrmFoundation = New frmFoundation
            mfrmFoundation.Show()
        Catch ex As Exception
            Log("Error while showing frmFoundation " + ex.Message)
        End Try
        Return 1
    End Function
    Public Function ShowTicketList() As Double
        Try
            mGRCData = New GRCSec.GridcoinData
            mfrmLogin = New frmLogin
            mfrmTicketList = New frmTicketList
            mfrmTicketList.Show()
        Catch ex As Exception
            Log("Error while transitioning to frmTicketList" + ex.Message)
        End Try
        Return 1
    End Function
    Public Function ShowTicker() As Double
        Try
            mfrmTicker = New frmLiveTicker
            mfrmTicker.Show()
        Catch ex As Exception
            Log("Error while booting ticker " + ex.Message)
        End Try
        Return 1
    End Function
    Public Function ShowLeaderboard() As Double
        mfrmLeaderboard = New frmLeaderboard
        mfrmLeaderboard.Show()
        Return 1
    End Function
    Public Function ShowMiningConsole() As Double
        Try
            lfrmMiningCounter = lfrmMiningCounter + 1
            If mfrmMining Is Nothing Then
                mfrmMining = New frmMining
            End If
            mfrmMining.Show()
        Catch ex As Exception
        End Try
        Return 1
    End Function
    Public Function TestOutdated(ByVal sdata As String, ByVal mins As Long) As Boolean
        Return Outdated(sdata, mins)
    End Function
    Public Function TestKeyValue(ByVal sKey As String) As String
        Return KeyValue(sKey)
    End Function
    Public Function TestUpdateKey(ByVal sKey As String, ByVal sValue As String) As Double
        Call UpdateKey(sKey, sValue)
        Return 1
    End Function
    Public Sub ExecuteCode(ByVal sCode As String)
        Log("Executing smart contract " + sCode)
        Dim classSmartContract As New GridcoinSmartContract
        classSmartContract.ExecuteContract(sCode)
    End Sub
    Public Sub SpeakSentence(sSentence As String)
        msSentence = sSentence
        Dim t As New Threading.Thread(AddressOf SpeakOnBackgroundThread)
        t.Start()
    End Sub
    Public Sub SpeakOnBackgroundThread()
        Dim S As New SpeechSynthesis
        S.Speak(msSentence)
    End Sub
    Public Function UpdateConfirm(sTxId As String) As String
        msTXID = sTxId
        Dim thUpdate As New System.Threading.Thread(AddressOf mUpdateConfirmAsync)
        thUpdate.Start()
        Return "1"
    End Function
    Public Function GRCCodeExecutionSubsystem(sCommand As String) As String
        'Generic interface to execute approved signed safe code at runtime
        Dim sResult As String = "FAIL"
        Select Case sCommand
            Case "DISABLE_WINDOWS_ERROR_REPORTING"
                sResult = AllowWindowsAppsToCrashWithoutErrorReportingDialog()
            Case "RESERVED"
                sResult = "NOT IMPLEMENTED YET"
            Case Else
                sResult = "NOT IMPLEMENTED"
        End Select

        If sResult = "" Then sResult = "SUCCESS"
        Return sResult

    End Function
    Public Function TrackConfirm(sTXID As String) As String
        Log("Tracking " + Trim(sTXID))
        Try
            Dim lOut As Double = mTrackConfirm(sTXID)
            Log("Returning " + Trim(lOut))
            Return Trim(lOut)
        Catch ex As Exception
            Log("ERROR" + ex.Message)
            Return "0"
        End Try
        'Return a 0 or 1
    End Function
    Public Function InsertConfirm(sConfirm As String) As Double
        Log(sConfirm)
        Dim vConfirm() As String
        vConfirm = Split(sConfirm, "<COL>")
        Dim dAmt As Double = vConfirm(0)
        Dim sFrom As String = vConfirm(1)
        Dim sTo As String = vConfirm(2)
        Dim sTXID As String = vConfirm(3)
        Dim sOut As String = mInsertConfirm(dAmt, sFrom, sTo, sTXID)
        Log("Inserted " + Trim(sOut))
        Return 1
    End Function
    Public Function SetDebugMode(bMode) As Boolean
        mbDebugging = bMode
    End Function
    Public Function SetSessionInfo(sInfo As String) As Double
        Dim vSession() As String
        vSession = Split(sInfo, "<COL>")
        Try
            msDefaultGRCAddress = vSession(0)
            msCPID = vSession(1)
            mlMagnitude = Val(vSession(2))
        Catch ex As Exception
            Log("SetSessionInfo " + ex.Message)
        End Try
        Return 1
    End Function
    Public Function ExplainMag(sCPID As String) As String
        If bMagsDoneLoading = False Then
            Log("This node is still syncing.")
            Return ""
        End If
        Dim sOut As String = ""
        sOut = ExplainNeuralNetworkMagnitudeByCPID(sCPID)
        '  Log("Responding to neural request for " + sCPID + " " + sOut)
        Return sOut
    End Function
    Public Function ResolveDiscrepancies(sContract As String) As String
        '7-25-2015 - Moving to QuorumHashingAlgorithm for this - disable this for the time being
        'Call ThreadResolveDiscrepanciesIn(sContract)
        'Return "Started Async Resolution."
        Return ""
    End Function
    Public Function ResolveCurrentDiscrepancies(sContract As String) As String
        Try
            Dim sPath As String = GetGridFolder() + "NeuralNetwork\contract.dat"
            Dim swContract As New StreamWriter(sPath)
            swContract.Write(sContract)
            swContract.Close()
        Catch ex As Exception
            Return ex.Message
        End Try
        Return "SUCCESS"
    End Function
    Public Function SetGenericVotingData(sValue As String) As Double
        Return SetGenericData("POLLS", sValue)
    End Function
    Public Function SetGenericData(sKey As String, sValue As String) As Double
        Try
            If msGenericDictionary.ContainsKey(sKey) Then
                msGenericDictionary(sKey) = sValue
            Else
                msGenericDictionary.Add(sKey, sValue)
            End If
            Return 1
        Catch ex As Exception
            Return 0
        End Try
    End Function
    Public Function SetTestNetFlag(sData As String) As Double
        Try
            If sData = "TESTNET" Then
                mbTestNet = True
            Else
                mbTestNet = False
            End If
            Log("Testnet : " + Trim(mbTestNet))

        Catch ex As Exception
            Return 0
        End Try
        Return 1
    End Function
    Public Function SyncCPIDsWithDPORNodes(sData As String) As Double
        'Write the Gridcoin CPIDs to the Persisted Data System
        Try
            Call SyncDPOR2(sData)
        Catch ex As Exception
            Log("Exception during SyncDpor2 : " + ex.Message)
            Return -2
        End Try
        Log("Finished syncing DPOR cpids.")
        Return 0
    End Function
    Public Function PushGridcoinDiagnosticData(sData As String) As Double
        Try
            msSyncData = sData
        Catch ex As Exception
            Log("Exception during PushGridcoinDiagnosticData : " + ex.Message)
            Return -2
        End Try
        Return 1
    End Function
    Public Sub UpdateMagnitudesOnly()
        EnsureNNDirExists()
        mbForcefullySyncAllRac = True
        ResetCPIDsForManualSync()
        Call UpdateMagnitudes()
    End Sub
    Public Sub SetSqlBlock(ByVal data As String)
        Exit Sub
    End Sub
    Public Sub UpdateLeaderBoard()
        Exit Sub
        If KeyValue("disablesql") = "true" Then Exit Sub
        Try
        Catch ex As Exception
        End Try
    End Sub
    Public Sub SetBestBlock(ByVal nBlock As Integer)

    End Sub
    Public Function GetLanIP() As String
        Return GetLocalLanIP1()
    End Function
    Public Sub clsLogOff()
        If mGRCData Is Nothing Then mGRCData = New GRCSec.GridcoinData
        mGRCData.LogOff(GetSessionGuid)
    End Sub
    Public Sub ShowEmailModule()
        Dim e As New frmMail
        e.Show()
        e.RetrievePop3Emails()
    End Sub
    Protected Overrides Sub Finalize()
        Try
            MyBase.Finalize()
        Catch ex As Exception
        End Try
    End Sub

End Class

Public Interface IGridcoinMining

   
End Interface
