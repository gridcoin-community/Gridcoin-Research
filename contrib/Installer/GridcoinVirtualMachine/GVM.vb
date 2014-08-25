


Public Class GVM

    Private lCt As Long = 0

    Public CPUMiner As CPUMiner

    Sub New()
        CPUMiner = New CPUMiner
        Initialize()
    End Sub

    Public ReadOnly Property BoincCredits As Double
        Get
            Return modBoincCredits.BoincCredits
        End Get
    End Property
    Public ReadOnly Property BoincDeltaOverTime As String
        Get
            Return modUtilization.BoincAvgOverTime
        End Get
    End Property
    Public ReadOnly Property MinedHash As String
        Get
            Return CPUMiner.MinedHash
        End Get
    End Property
    Public ReadOnly Property mbunarr1 As Double
        Get
            Return mdProcNarrComponent1
        End Get
    End Property

    Public ReadOnly Property mbunarr2 As Double
        Get
            Return mdProcNarrComponent2
        End Get
    End Property
    Public ReadOnly Property SourceBlock As String
        Get
            Return CPUMiner.SourceBlock
        End Get
    End Property

    Public ReadOnly Property BoincProjects As Double
        Get
            Return modBoincCredits.BoincProjects
        End Get
    End Property
    Public Function BoincCreditsByProject(ByVal projectid As Long, ByVal dUserId As Double) As Double
        Return modBoincCredits.BoincCreditsByProject(projectid, dUserId)
    End Function
    Public Function BoincCreditsByProject2(ByVal projectid As Long, ByVal dUserId As Double, ByRef sStruct As String, ByRef sTeam As String) As Double
        Dim dCredits As Double

        Try
            dCredits = ExtractCreditsByProject(projectid, dUserId, PublicWalletAddress, sStruct)
            Dim vStruct() As String
            vStruct = Split(sStruct, ",")
            If UBound(vStruct) >= 2 Then
                sTeam = vStruct(2)
            End If
            Return dCredits

        Catch ex As Exception

        End Try
    End Function
    Public Function Des3Encrypt(ByVal s As String) As String
        Return modCryptography.Des3EncryptData(s)
    End Function
    Public Function Des3Decrypt(ByVal sData As String) As String
        Return modCryptography.Des3DecryptData(sData)
    End Function
    Public ReadOnly Property BoincCreditsAvg As Double
        Get
            Return modBoincCredits.BoincCreditsAvg
        End Get
    End Property
    Public Function CPUPoW(ByVal sHash As String) As Double
        Dim vHash() As String
        vHash = Split(sHash, ":")
        If UBound(vHash) <> 2 Then Return -13
        Dim iProjectId As Integer
        Dim lUserId As Long
        Dim sGRCAddress As String
        iProjectId = vHash(0) : lUserId = vHash(1) : sGRCAddress = vHash(2)
        Dim dCredits As Double
        Dim sErr As String
        dCredits = CPUPoW(iProjectId, lUserId, sGRCAddress)
        Return dCredits
    End Function
    Public Function CPUPoW(ByVal iProjectId As Integer, ByVal lUserId As Long, ByVal sGRCAddress As String) As Long
        Dim dCredits As Double
        Dim sErr As String
        dCredits = ExtractCreditsByProject(iProjectId, lUserId, sGRCAddress, sErr)
        Return dCredits
    End Function



    Public Property PublicWalletAddress As String
        Set(ByVal value As String)
            modUtilization.PublicWalletAddress = value
        End Set
        Get
            Return modUtilization.PublicWalletAddress
        End Get
    End Property
    Public Property BestBlock As Long
        Set(ByVal nBlock As Long)
            modUtilization.mnBestBlock = nBlock
            lCt = lCt + 1
            If lCt = 1 Then Log("Setting GVM best block " + Trim(nBlock))

        End Set
        Get
            Return mnBestBlock
        End Get
    End Property


    Public Property LastBlockHash As String
        Set(ByVal value As String)
            modUtilization.LastBlockHash = value
        End Set
        Get
            Return modUtilization.LastBlockHash
        End Get
    End Property
    Public ReadOnly Property BoincUtilization As Double
        Get
            Return Val(mBoincProcessorUtilization)

        End Get
    End Property

    Public ReadOnly Property BoincThreads As Double
        Get
            Return Val(mBoincThreads)

        End Get
    End Property
    Public ReadOnly Property BoincTotalHostAvg As Double
        Get
            Return modBoincCredits.BoincTotalHostAvg

        End Get
    End Property


    Public ReadOnly Property CalcApiUrl(lProj As Long, sUserId As String) As String
        Get
            Return Trim(modBoincCredits.CalculateApiURL(lProj, sUserId))
        End Get
    End Property


    Public ReadOnly Property CalcFriendlyUrl(lProj As Long, sUserId As String) As String
        Get
            Return Trim(modBoincCredits.CalculateFriendlyURL(lProj, sUserId))
        End Get
    End Property

    Public ReadOnly Property OldBA As String
        Get
        End Get
    End Property
    Public ReadOnly Property BoincCreditsAtPointInTime As Double
        Get
            Return modBoincCredits.BoincCreditsAtPointInTime
        End Get
    End Property
    Public ReadOnly Property BoincCreditsAvgAtPointInTime As Double
        Get
            Return modBoincCredits.BoincCreditsAvgAtPointInTime
        End Get
    End Property
    Public Function ReturnBoincCreditsAtPointInTime(ByVal dLookback As Double) As Double
        Return modBoincCredits.ReturnBoincCreditsAtPointInTime(dLookback)
    End Function
    Public ReadOnly Property BoincMD5 As String
        Get
            Return Trim(modBoincMD5)
        End Get
    End Property
    Public Function MineBlock()
        Dim c As New CPUMiner
        c.MineNewBlock()
    End Function
End Class
