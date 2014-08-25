Imports System.Security.Cryptography
Imports System.Timers

Public Class CPUMiner

    Public KHPS As Double
    Public Elapsed As TimeSpan
    Public Status As Boolean
    Public MinedHash As String
    Public SourceBlock As String
    Public LastSolvedHash As String = "?"
    Private _timerCPUMiner As System.Timers.Timer
    Private MinerSlowCounter As Long
    Private sGVMGuidHash As String

    Sub New()
        If _timerCPUMiner Is Nothing Then
            _timerCPUMiner = New System.Timers.Timer(1000)
            AddHandler _timerCPUMiner.Elapsed, New ElapsedEventHandler(AddressOf CPUMinerTimerElapsed)
            _timerCPUMiner.Enabled = True
        End If
    End Sub
    Private Sub CPUMinerTimerElapsed()
        If modUtilization.mBoincThreads > 0 Then
            MinerSlowCounter = MinerSlowCounter + 1
            If MinerSlowCounter > 15000 Then MinerSlowCounter = 0 : Status = False
            If Status = False Then
                If LastSolvedHash <> modUtilization.LastBlockHash Then
                    Status = True
                    MineNewBlock()
                End If
            End If
        End If

    End Sub
    Private Structure Block
        Public PREVIOUS_GRC_HASH As String
        Public BLOCK_DATA As String
        Public CPU_UTILIZATION As Integer
        Public BOINC_AVG_CREDITS As Long
        Public BOINC_THREAD_COUNT As Integer
        Public BOINC_PROJECTS_COUNT As Integer
        Public BOINC_PROJECTS_DATA As String
        Public UNIX_TIME As Long
        Public NONCE As Double
        Public DIFFICULTY As Double
        Public GVM_BOINC_GUID As String
    End Structure
    Private Function GetBlockTemplate(ByVal nonce As Double, ByVal Difficulty As Double) As Block
        Dim b As New Block
        b.PREVIOUS_GRC_HASH = modUtilization.LastBlockHash
        b.BLOCK_DATA = modUtilization.BlockData
        b.CPU_UTILIZATION = modUtilization.mBoincProcessorUtilization
        b.BOINC_AVG_CREDITS = Left(Trim(modBoincCredits.BoincCreditsAvg), 4)
        b.BOINC_THREAD_COUNT = modUtilization.mBoincThreads
        b.BOINC_PROJECTS_COUNT = modBoincCredits.BoincProjects
        b.BOINC_PROJECTS_DATA = modBoincCredits.BoincProjectData
        b.UNIX_TIME = UnixTimestamp()
        b.NONCE = nonce
        b.DIFFICULTY = Difficulty
        Return b
    End Function
    Private Function BlockToGRCString(ByVal MiningBlock As Block) As String
        Try

        Dim s As String
        Dim d As String = "\"
            s = MiningBlock.PREVIOUS_GRC_HASH + d + MiningBlock.BLOCK_DATA + d + Trim(MiningBlock.CPU_UTILIZATION) + d _
                + Left(Trim(MiningBlock.BOINC_AVG_CREDITS), 4) + d + Trim(MiningBlock.BOINC_THREAD_COUNT) _
                + d + Trim(MiningBlock.BOINC_PROJECTS_COUNT) + d _
                + Trim(MiningBlock.BOINC_PROJECTS_DATA) + d + Trim(MiningBlock.UNIX_TIME) + d + Trim(MiningBlock.DIFFICULTY) + d + Trim(MiningBlock.NONCE)
            Return s
        Catch ex As Exception

        End Try

    End Function
    Public Sub MineNewBlock()
        Dim thrMine As New System.Threading.Thread(AddressOf Mine)
        thrMine.IsBackground = True
        thrMine.Start()
    End Sub
    Private Sub Mine()

        Try

        KHPS = 0
        Status = True
        Dim objSHA1 As New SHA1CryptoServiceProvider()
        Call modBoincCredits.LogBoincCredits()
            modUtilization.BlockData = "" + ":" + Trim(modBoincMD5())
        Dim bHash() As Byte
        Dim cHash As String
        Dim startime = Now
        Dim stoptime = Now
        Dim diff As String
        Dim targetms As Long = 10000 'This will change as soon as we implement the Moore's Law equation
        diff = Trim(Math.Round(targetms / 5000, 0))
        Dim nonce As Double = 0
        Dim MiningBlock As Block
        Dim sSourceBlock As String
        Dim sStartingBlockHash As String = LastBlockHash
        MiningBlock = GetBlockTemplate(nonce, diff)

        While True
                nonce = nonce + 1
                System.Threading.Thread.Sleep(100)

            If LastBlockHash <> sStartingBlockHash Then
                sGVMGuidHash = modCryptography.ReturnGVMGuid(Des3EncryptData(LastBlockHash + ",CPUMiner," + UnixTimestamp().ToString))
                MiningBlock.GVM_BOINC_GUID = sGVMGuidHash
                Exit While 'New block detected on network
            End If
            MiningBlock = GetBlockTemplate(nonce, diff)
            sSourceBlock = BlockToGRCString(MiningBlock)
            bHash = System.Text.Encoding.ASCII.GetBytes(sSourceBlock)
            cHash = Replace(BitConverter.ToString(objSHA1.ComputeHash(bHash)), "-", "")
            If (nonce Mod 1000) = 0 Then
                Elapsed = Now - startime
                KHPS = nonce / Elapsed.Seconds
                If Elapsed.Seconds > 600 Then Exit Sub
            End If
                If cHash.Contains(Trim(diff)) And cHash.Contains(String.Format("{0:000}", MiningBlock.CPU_UTILIZATION)) _
                    And cHash.Contains(Trim(Val(MiningBlock.BOINC_AVG_CREDITS))) _
                    And cHash.Contains(Trim(Val(MiningBlock.BOINC_THREAD_COUNT))) Then
                    Elapsed = Now - startime
                    MinedHash = cHash
                    SourceBlock = sSourceBlock
                    LastSolvedHash = MiningBlock.PREVIOUS_GRC_HASH
                    Exit While
                End If
        End While
            Status = False


        Catch ex As Exception
            Status = False

        End Try



    End Sub

End Class
