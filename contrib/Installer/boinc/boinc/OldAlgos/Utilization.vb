Imports Microsoft.VisualBasic
Imports System.Timers



Public Class Utilization
    'Public Ticker As Long
    Private lfrmMiningCounter As Long = 0

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
    Sub New()
        Initialize()
        ShowMiningConsole()

    End Sub
    Public ReadOnly Property Version As Double
        Get
            Return 19
        End Get
    End Property
    Public ReadOnly Property BoincMD5 As String
        Get
            Return modBoincMD5()
        End Get
    End Property
    Public ReadOnly Property BoincAuthenticityString As String
        Get
            Return Trim(VerifyBoincAuthenticity.ToString())
        End Get
    End Property
    Public ReadOnly Property BoincTotalCreditsAvg As Double
        Get
            Return modUtilization.BoincCreditsAvg
        End Get
    End Property
    Public ReadOnly Property BoincAuthenticity As Double
        Get
            Return VerifyBoincAuthenticity()
        End Get

    End Property
    Public ReadOnly Property BoincTotalCredits As Double
        Get
            Return modUtilization.BoincCredits
        End Get
    End Property
    Public Function Des3Encrypt(ByVal s As String) As String
        Return Des3EncryptData(s)
    End Function
    Public Function Des3Decrypt(ByVal sData As String) As String
        Return Des3DecryptData(sData)
    End Function
    Public Function ShowMiningConsole()
        Try

            ' If mfrmMining Is Nothing Or lfrmMiningCounter = 0 Then mfrmMining = New frmMining
            lfrmMiningCounter = lfrmMiningCounter + 1
            If mfrmMining Is Nothing Or lfrmMiningCounter = 1 Then
                Try
                    mfrmMining = New frmMining
                    mfrmMining.classUtilization = Me
                    mfrmMining.Show()
                    mfrmMining.Refresh(False)
                    'Call Elapsed()
                Catch ex As Exception

                End Try
            Else
                mfrmMining.Show()
                mfrmMining.BringToFront()
                mfrmMining.Focus()
            End If


        Catch ex As Exception
            mfrmMining = New frmMining
            lfrmMiningCounter = lfrmMiningCounter + 1
            mfrmMining.classUtilization = Me
            mfrmMining.Show()
            mfrmMining.Refresh(False)
        End Try
    End Function
    Public ReadOnly Property BoincDeltaOverTime() As String
        Get
            Return modUtilization.BoincAvgOverTime
        End Get
    End Property
    Public ReadOnly Property BoincProjectCount As Double
        Get
            Return modUtilization.BoincProjects
        End Get
    End Property
    Public ReadOnly Property BoincTotalHostAverageCredits As Double
        Get
            Return modUtilization.BoincTotalHostAvg
        End Get
    End Property

    Public Sub ShowEmailModule()
        Dim e As New frmMail
        e.Show()
        e.RetrievePop3Emails()
    End Sub

    Protected Overrides Sub Finalize()
        If Not mfrmMining Is Nothing Then
            mfrmMining.bDisposing = True
            mfrmMining.Close()
            mfrmMining.Dispose()
            mfrmMining = Nothing
        End If
        MyBase.Finalize()
    End Sub
End Class
