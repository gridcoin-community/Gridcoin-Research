﻿Imports Microsoft.VisualBasic
Imports System.Timers
Imports System.IO

Public Class Utilization
    Implements IGridCoinMining

    Private _nBestBlock As Long
    Private _lLeaderboard As Long
    Private _lLeaderUpdates As Long
    Public _boincmagnitude As Double
    Private msSentence As String = ""
    Private mlSpeakMagnitude As Double


    Public ReadOnly Property Version As Double
        Get
            Return 384

        End Get
    End Property

    Private lfrmMiningCounter As Long = 0
    Public ReadOnly Property BoincUtilization As Double
        Get
            ' Return Val(clsGVM.BoincUtilization)
        End Get
    End Property
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
        p = Process.Start(GetGRCAppDir() + "\GridcoinGalaza.exe")
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
    Public ReadOnly Property BoincThreads As Double
        Get

        End Get
    End Property
    Sub New()
        UpdateKey("UpdatingLeaderboard", "false")
        Try
            If Not DatabaseExists("gridcoin_leaderboard") Then ReplicateDatabase("gridcoin_leaderboard")
        Catch ex As Exception
            Log("New:" + ex.Message)
        End Try
        mclsUtilization = Me
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
        Call RestartWallet1("downloadblocks")
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
    Public ReadOnly Property BoincMD5 As String
        Get
            '   Return clsGVM.BoincMD5()
        End Get
    End Property
    Public Function cGetMd5(sData As String) As String
        Return GetMd5String(sData)
    End Function
    Public Function StrToMd5Hash(s As String) As String
        Return CalcMd5(s)
    End Function
    Public Function GetNeuralHash() As String
        If Len(msCurrentNeuralHash) > 1 Then Return msCurrentNeuralHash 'This is invalidated when it changes
        Dim sContract As String = GetMagnitudeContract()

        Dim sHash As String = GetMd5String(sContract)
        Return sHash
    End Function
    Public Function GetNeuralContract() As String
        Dim sContract As String = GetMagnitudeContract()
        Return sContract
    End Function

    Public ReadOnly Property RetrieveWin32BoincHash() As String
        Get

        End Get
    End Property
    Public ReadOnly Property RetrieveSqlHighBlock As Double

        Get
        
        End Get
    End Property
    Public ReadOnly Property BoincDeltaOverTime As String
        Get

        End Get
    End Property
    Public ReadOnly Property BoincTotalCreditsAvg As Double
        Get

        End Get
    End Property
    Public ReadOnly Property BoincTotalCredits As Double
        Get

        End Get
    End Property
    Public Function Des3Encrypt(ByVal s As String) As String

    End Function
    Public Function Des3Decrypt(ByVal sData As String) As String

    End Function
    Public Function ShowProjects()
    End Function
    Public Function ShowVotingConsole()
        Dim fmVoting As New frmVoting
        fmVoting.Show()
    End Function

    Public Function ShowNewUserWizard()
        Dim fNUW As New frmNewUserWizard
        fNUW.Show()
    End Function
    Public Function ShowSql()
        Try
            mfrmSql = New frmSQL
            mfrmSql.Show()
        Catch ex As Exception
            Log("Error while transitioning to frmSQL" + ex.Message)
        End Try
    End Function
    Public Function ShowTicketAdd()
        Try
            mfrmTicketAdd = New frmTicketAdd
            mfrmTicketAdd.Show()
        Catch ex As Exception
            Log("Error while transitioning to frmTicketAdd" + ex.Message)
        End Try
    End Function
    Public Function ShowTicketList()
        Try
            mfrmLogin = New frmLogin
            mfrmTicketList = New frmTicketList
            mfrmTicketList.Show()
        Catch ex As Exception
            Log("Error while transitioning to frmTicketList" + ex.Message)
        End Try
    End Function
    Public Function ShowTicker()
        Try
            mfrmTicker = New frmLiveTicker
            mfrmTicker.Show()
        Catch ex As Exception
            Log("Error while booting ticker " + ex.Message)
        End Try
    End Function

    Public Function ShowLeaderboard()
        mfrmLeaderboard = New frmLeaderboard
        mfrmLeaderboard.Show()
    End Function

    Public Function ShowMiningConsole()
        Try
            lfrmMiningCounter = lfrmMiningCounter + 1
            
            If mfrmMining Is Nothing Then
                mfrmMining = New frmMining
            End If

            mfrmMining.Show()

        Catch ex As Exception
        End Try
    End Function
   
    Public ReadOnly Property SourceBlock As String
        Get

        End Get
    End Property

    Public Function TestOutdated(ByVal sdata As String, ByVal mins As Long) As Boolean
        Return Outdated(sdata, mins)
    End Function
    Public Function TestKeyValue(ByVal sKey As String) As String
        Return KeyValue(sKey)
    End Function
    Public Function TestUpdateKey(ByVal sKey As String, ByVal sValue As String)
        Call UpdateKey(sKey, sValue)
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
        msTxId = sTxId
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
            Log("HEINOUS ERROR" + ex.Message)
            Return "0"
        End Try
        'return a 0 or 1
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
            msSyncData = sData


            Call SyncDPOR2()
        Catch ex As Exception
            Log("Exception during SyncDpor2 : " + ex.Message)
            Return -2
        End Try
        Log("Finished syncing DPOR cpids.")

        Return 0

    End Function
    Public Sub AddressUserThread()

        Dim s As New SpeechSynthesis
        s.AddressUserBySurname(mlSpeakMagnitude)

    End Sub
    Public Function AddressUser(sMagnitude As String) As Double
        Dim t As New Threading.Thread(AddressOf AddressUserThread)
        mlSpeakMagnitude = Val(sMagnitude)
        t.Start()

        Return 1
    End Function
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
    Public Sub SetLastBlockHash(ByVal data As String)
        ' clsGVM.LastBlockHash = Trim(data)
    End Sub
    Public Sub SetPublicWalletAddress(ByVal data As String)

    End Sub
    Public Sub SetBestBlock(ByVal nBlock As Integer)


    End Sub
   
  
    Public Function GetLanIP() As String
        Return GetLocalLanIP1()
    End Function
    Public ReadOnly Property BoincProjectCount As Double
        Get
        End Get
    End Property
    Public ReadOnly Property BoincTotalHostAverageCredits As Double
        Get
        End Get
    End Property

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

Public Interface IGridCoinMining

   
End Interface