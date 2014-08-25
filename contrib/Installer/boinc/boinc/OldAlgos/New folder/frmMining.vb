Imports System.IO
Imports System.Runtime.InteropServices
Imports System.Windows.Forms
Imports System.Drawing
Imports System.Diagnostics

Imports System.Timers
Imports System.Windows.Forms.DataVisualization.Charting
Imports System.Threading

Public Class frmMining

    Public clsUtilization As Utilization
    Private RefreshCount As Long
    Private RestartedMinerAt As DateTime
    Private RestartedWalletAt As DateTime

    Public bDisposing As Boolean
    Public bSuccessfullyLoaded As Boolean
    Private bCharting As Boolean
    Private mMh(10) As Double
    Private mShares(10) As Double
    Private mStales(10) As Double
    Private mInvalids(10) As Double

    Private mProcess(10) As Process
    Private mProcessInfo(10) As ProcessStartInfo
    Private mThreadTimer(10) As System.Timers.Timer
    Private mRTB(10) As RichTextBox

    Private mLineCount(10) As Integer
    Private mEnabled(10) As Boolean
    Private msReaperOut(10) As String

    Private Sub Button1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnRefresh.Click
        Refresh2(True)

    End Sub
    Private Sub UpdateCharts()

        Try

        Dim thCharts As New Thread(AddressOf UpdateChartsThread)
        thCharts.IsBackground = True
            thCharts.Start()
        Catch ex As Exception

        End Try

    End Sub
    Private Sub UpdateChartsThread()
        Try
            ChartBoinc()
            ChartBoincUtilization()
            Me.Update()

        Catch ex As Exception

        End Try
       
    End Sub
    Public Sub Refresh2(ByVal bStatsOnly As Boolean)

        bCharting = False

        Try
            If Not bStatsOnly Then
                UpdateCharts()
            Else
                ChartBoincUtilization()
            End If
        Catch ex As Exception
        End Try

        Try
            lblPower.Text = Trim(Math.Round(clsUtilization.BoincUtilization, 1))
            lblThreadCount.Text = Trim(clsUtilization.BoincThreads)
            lblVersion.Text = Trim(clsUtilization.Version)
            lblAvgCredits.Text = Trim(clsUtilization.BoincTotalCreditsAvg)

        Catch ex As Exception

        End Try

        Try
            lblMD5.Text = Trim(clsUtilization.BoincMD5)
            RefreshCount = RefreshCount + 1
            If RefreshCount = 1 Then
                For x = 1 To 1 'Soft start while client starts
                    Application.DoEvents()
                    System.Threading.Thread.Sleep(700)
                Next
                ReStartGuiMiner()
                RestartedWalletAt = Now
                System.Threading.Thread.Sleep(100)
                UpdateCharts()
            End If

        Catch ex As Exception
        End Try
        Try
            Dim lMinSetting = Val(KeyValue("RestartMiner"))
            Dim lRunning = Math.Abs(DateDiff(DateInterval.Minute, RestartedMinerAt, Now))
            If lMinSetting = 0 Then
                lblRestartMiner.Text = "Never"
            Else
                Dim dCountdown As Double
                dCountdown = lMinSetting - lRunning
                lblRestartMiner.Text = Trim(dCountdown)
                If dCountdown <= 0 Then
                    ReStartGuiMiner()
                    Refresh2(True)
                End If
            End If

        Catch ex As Exception

        End Try
        Try
            Dim lMinSetting = Val(KeyValue("RestartWallet"))
            Dim lRunning = Math.Abs(DateDiff(DateInterval.Minute, RestartedWalletAt, Now))
            If lMinSetting = 0 Then
                lblRestartWallet.Text = "Never"
            Else
                Dim dCountdown As Double
                dCountdown = lMinSetting - lRunning
                lblRestartWallet.Text = Trim(dCountdown)
                If dCountdown <= 0 Then
                    RestartedWalletAt = Now
                    RestartWallet()
                End If
            End If

        Catch ex As Exception

        End Try

        bSuccessfullyLoaded = True



    End Sub
    Public Sub KillGuiMiner()

        Try

            For x = 1 To 6
                KillProcess("cgminer*")
                KillProcess("reaper*")

                KillProcess("conhost.exe") 'Kills up to 4 instances of surrogates
            Next x
        Catch ex As Exception
        End Try
    End Sub
    Public Function GetGRCAppDir() As String
        Try
            Dim fi As New System.IO.FileInfo(Application.ExecutablePath)
            Return fi.DirectoryName
        Catch ex As Exception
        End Try
    End Function
    Public Sub RestartWallet()
        Try
            'First kill CGMiner, Reaper and GuiMiner
            For x = 1 To 4
                KillGuiMiner()
                Threading.Thread.Sleep(500) 'Let CGMiner & Reaper close.
                Application.DoEvents()
            Next
            Threading.Thread.Sleep(2000)
            Dim p As Process = New Process()
            Dim pi As ProcessStartInfo = New ProcessStartInfo()
            pi.WorkingDirectory = GetGRCAppDir()
            pi.UseShellExecute = True
            pi.FileName = "GRCRestarter.exe"
            p.StartInfo = pi
            p.Start()
        Catch ex As Exception

        End Try


    End Sub
    Public Sub ChartBoinc()
        Try
            If bCharting Then Exit Sub

            bCharting = True

            Chart1.Series.Clear()
            Chart1.Titles.Clear()
            Chart1.Titles.Add("Boinc Utilization")
            Chart1.BackColor = Color.Black
            Chart1.ForeColor = Color.Green

            Dim seriesAvgCredits As New Series
            seriesAvgCredits.Name = "Avg Daily Credits"
            seriesAvgCredits.ChartType = SeriesChartType.Line
            seriesAvgCredits.LabelBackColor = Color.Black
            seriesAvgCredits.LabelForeColor = Color.Green

            Chart1.ChartAreas(0).AxisX.LabelStyle.Format = "MM-dd-yyyy"
            Chart1.ChartAreas(0).AxisX.IntervalType = DateTimeIntervalType.Weeks
            Chart1.ChartAreas(0).BackColor = Color.Black
            Chart1.ChartAreas(0).ShadowColor = Color.Chocolate
            Chart1.ChartAreas(0).BackSecondaryColor = Color.Gray
            Chart1.ChartAreas(0).BorderColor = Color.Gray
            Chart1.Legends(0).BackColor = Color.Black
            Chart1.Legends(0).ForeColor = Color.Green



            Dim seriesTotalCredits As New Series
            seriesTotalCredits.ChartType = SeriesChartType.FastLine
            seriesTotalCredits.Name = "Total Daily Credits"
            Dim dProj As Double
            Dim seriesProjects As New Series
            seriesProjects.Name = "Projects"
            Chart1.ChartAreas(0).AxisX.Interval = 1
            seriesProjects.ChartType = SeriesChartType.StepLine
            Dim lookback As Double '
            For x = 30 To 0.5 Step -1.5
                lookback = x * 3600 * 24
                clsGVM.ReturnBoincCreditsAtPointInTime(lookback)
                Dim l1 As Double
                Dim l2 As Double
                Dim l3 As Double
                Dim dAvg As Double
                l1 = clsGVM.BoincCredits
                clsGVM.ReturnBoincCreditsAtPointInTime(lookback - (3600 * 24))
                l2 = clsGVM.BoincCredits
                dAvg = clsGVM.BoincCreditsAvg
                l3 = Math.Abs(l1 - l2)
                Application.DoEvents()
                Chart1.Refresh()
                System.Threading.Thread.Sleep(50)
                Dim pCreditsAvg As New DataPoint
                dProj = clsGVM.BoincProjects
                Dim d1 As Date = DateAdd(DateInterval.Day, -x, Now)
                pCreditsAvg.SetValueXY(d1, dAvg)
                seriesAvgCredits.Points.Add(pCreditsAvg)
                Dim dpProj As New DataPoint()
                dpProj.SetValueXY(d1, dProj * (dAvg / 10))
                seriesProjects.Points.Add(dpProj)
                Dim pCreditsTotal As New DataPoint()
                pCreditsTotal.SetValueXY(d1, l3)
                seriesTotalCredits.Points.Add(pCreditsTotal)
            Next
            Chart1.Series.Add(seriesTotalCredits)
            Chart1.Series.Add(seriesAvgCredits)
            Chart1.Series.Add(seriesProjects)
        Catch ex As Exception
        End Try
        bCharting = False

    End Sub
    Public Sub ChartBoincUtilization()
        Try
            ChartUtilization.Series.Clear()
            ChartUtilization.Titles.Clear()
            ChartUtilization.BackColor = Color.Black
            ChartUtilization.ForeColor = Color.Green

            ChartUtilization.Titles.Add("Utilization")
            ChartUtilization.Titles(0).BackColor = Color.Black
            ChartUtilization.Titles(0).ForeColor = Color.Green
            ChartUtilization.ChartAreas(0).BackColor = Color.Black
            ChartUtilization.ChartAreas(0).ShadowColor = Color.Chocolate
            ChartUtilization.ChartAreas(0).BackSecondaryColor = Color.Gray
            ChartUtilization.ChartAreas(0).BorderColor = Color.Gray
            ChartUtilization.Legends(0).BackColor = Color.Black
            ChartUtilization.Legends(0).ForeColor = Color.Green


        Dim sUtilization As New Series
        sUtilization.Name = "Utilization"
        sUtilization.ChartType = SeriesChartType.Pie
            sUtilization.LegendText = "Boinc Utilization"
            sUtilization.LabelBackColor = Color.Black
            sUtilization.LabelForeColor = Color.Green

            ChartUtilization.Series.Add(sUtilization)

        Dim bu As Double
            bu = Math.Round(clsUtilization.BoincUtilization, 1)
            ChartUtilization.Series("Utilization").Points.AddY(bu)
            ChartUtilization.Series("Utilization").LabelBackColor = Color.Black
            ChartUtilization.Series("utilization").LabelForeColor = Color.Green

        ChartUtilization.Series("Utilization").Points(0).Label = Trim(bu)
        ChartUtilization.Series("Utilization").Points(0).LegendToolTip = Trim(bu) + " utilization."
        ChartUtilization.Series("Utilization").Points.AddY(100 - bu)
        ChartUtilization.Series("Utilization").Points(1).LegendText = ""
        ChartUtilization.Series("Utilization").Points(1).IsVisibleInLegend = False
        ChartUtilization.Series("Utilization")("PointWidth") = "0.5"
        ChartUtilization.Series("Utilization").IsValueShownAsLabel = False
        ChartUtilization.Series("Utilization")("BarLabelStyle") = "Center"
        ChartUtilization.ChartAreas(0).Area3DStyle.Enable3D = True
            ChartUtilization.Series("Utilization")("DrawingStyle") = "Cylinder"
        Catch ex As Exception

        End Try

    End Sub

    Private Sub Timer1_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Timer1.Tick
        Call Refresh2(False)
    End Sub

    Private Sub btnRestartMiner_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnRestartMiner.Click
        Try
            ReStartGuiMiner()
        Catch ex As Exception
        End Try
    End Sub
    Private Sub RefreshRestartMinutes()
    End Sub


    Private Sub frmMining_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing
        If bDisposing Then
            Me.Close()
            Me.Dispose()
            KillGuiMiner()
            Exit Sub
        End If
        Me.Hide()
        e.Cancel = True
    End Sub
    Private Sub btnRestart_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnRestart.Click
        RestartWallet()
    End Sub

    Private Sub btnCloseWallet_Click(ByVal sender As System.Object, ByVal e As System.EventArgs)
        Try
            KillGuiMiner()
            KillProcess("gridcoin-qt*")
        Catch ex As Exception
        End Try
    End Sub
    Private Sub btnClose_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnClose.Click
        KillGuiMiner()
    End Sub

    Private Sub btnCloseWallet_Click_1(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnCloseWallet.Click
        Try

            KillGuiMiner()
            KillProcess("gridcoin-qt*")

        Catch ex As Exception

        End Try
    End Sub

    Private Sub btnHide_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnHide.Click
        Me.Hide()

    End Sub

    Private Sub btnmining_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnTestCPUMiner.Click

    
        clsGVM.LastBlockHash = Trim(Now)

        Dim sNarr As String
        sNarr = clsGVM.SourceBlock + "  OUTPUTS : " + clsGVM.MinedHash

        MsgBox(sNarr)



    End Sub

    Private Sub timerBoincBlock_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles timerBoincBlock.Tick

        lblCPUMinerElapsed.Text = Trim(Math.Round(clsGVM.CPUMiner.KHPS, 0))
        lblLastBlockHash.Text = clsGVM.LastBlockHash


        If clsGVM.CPUMiner.Status = False Then
            pbBoincBlock.Visible = False
            lblBoincBlock.Text = clsGVM.CPUMiner.MinedHash
        Else
            pbBoincBlock.Visible = True
            pbBoincBlock.Value = clsGVM.CPUMiner.Elapsed.Seconds
            If clsGVM.CPUMiner.Elapsed.Seconds > pbBoincBlock.Maximum - 5 Then pbBoincBlock.Maximum = pbBoincBlock.Maximum + 15
        End If
    End Sub





    Public Function SetupRTB(ByVal iThread As Integer) As RichTextBox
        Try

        Dim r() As Control

        r = Tc1.TabPages(iThread - 1).Controls.Find("rtb" + Trim(iThread), True)
        If r.Length = 0 Then
                Dim rtb As New RichTextBox
                rtb.BackColor = Color.Black
                rtb.ForeColor = Color.Green

            Dim f As New Font("Verdana", 10, FontStyle.Bold)
            rtb.Font = f
            rtb.Multiline = True

            Tc1.TabPages(iThread - 1).Controls.Add(rtb)
            rtb.Width = Tc1.TabPages(iThread - 1).Width
            rtb.Height = Tc1.TabPages(iThread - 1).Height
            rtb.Name = "rtb" + Trim(iThread - 1)
        End If
        r = Tc1.TabPages(iThread - 1).Controls.Find("rtb" + Trim(iThread - 1), True)
        mRTB(iThread) = r(0)

        Catch ex As Exception

        End Try


    End Function

    Public Sub StartReaperThread(ByVal iThreadId As Long)
        Try

        mProcess(iThreadId) = New Process()
        mProcessInfo(iThreadId) = New ProcessStartInfo()
        mProcessInfo(iThreadId).WorkingDirectory = GetGridFolder() + "\Reaper0" + Trim(iThreadId) + "\"
        mProcessInfo(iThreadId).UseShellExecute = False
        mProcessInfo(iThreadId).FileName = mProcessInfo(iThreadId).WorkingDirectory + "\reaper.exe"
        mProcessInfo(iThreadId).WindowStyle = ProcessWindowStyle.Hidden
        mProcessInfo(iThreadId).CreateNoWindow = True
        mProcess(iThreadId).StartInfo = mProcessInfo(iThreadId)
        mProcess(iThreadId).StartInfo.RedirectStandardOutput = True
        mEnabled(iThreadId) = False
       

        If Not File.Exists(mProcessInfo(iThreadId).FileName) Then
            msReaperOut(iThreadId) = "Reaper is missing. "
            Exit Sub
        End If
        mProcess(iThreadId).Start()
        mEnabled(iThreadId) = True
        Dim sTemp As String


        While True
            sTemp = mProcess(iThreadId).StandardOutput.ReadLine
            msReaperOut(iThreadId) = msReaperOut(iThreadId) + sTemp + vbCrLf
            TallyReaper(sTemp, iThreadId)


            If sTemp Is Nothing Then Exit While
        End While
        Catch ex As Exception

        End Try



    End Sub


    Private Sub TallyReaper(ByVal data As String, ByVal thread As Integer)

        Try
            If Mid(data, 1, 5) = "share" Then data = "0MH/s," + data

            Dim vData() As String
            vData = Split(data, ",")

            'GPU 1.147MH/s, shares: 0|0, stale 0%, GPU errors: 0%, ~1111.28 kH/s, 139s  
            Dim mh As String
            Dim sh As String
            Dim serr As String
            If UBound(vData) < 3 Then Exit Sub
            mh = vData(0)
            Dim multiplier As Double
            multiplier = 100000
            mh = Replace(mh, "GPU ", "")
            If (InStr(1, mh, "KH/s")) > 0 Then multiplier = 1000
            mh = Replace(mh, "MH/s", "")
            mh = Replace(mh, "KH/s", "")
            Dim dMH As Double
            dMH = Val(mh) * multiplier
            sh = vData(1)
            sh = Replace(sh, "shares: ", "")
            Dim vStale() As String
            vStale = Split(sh, "|")
            Dim shares As String
            Dim stales As String
            If UBound(vStale) < 1 Then Exit Sub
            shares = vStale(0)
            stales = vStale(1)
            serr = vData(3)
            serr = Replace(serr, "GPU errors: ", "")
            serr = Replace(serr, "%", "")
            mMh(thread) = dMH
            mShares(thread) = Val(shares)
            mStales(thread) = Val(stales)
            mInvalids(thread) = Val(serr)

        Catch ex As Exception

        End Try

    End Sub
    Public Sub updateGh()

        Try

        Dim x As Long
            mShares(0) = 0 : mStales(0) = 0 : mInvalids(0) = 0 : mMh(0) = 0
            For x = 1 To 5
                mShares(0) = mShares(0) + mShares(x)

                mStales(0) = mStales(0) + mStales(x)
                mInvalids(0) = mInvalids(0) + mInvalids(x)
                mMh(0) = mMh(0) + mMh(x)

            Next


            lblGPUMhs.Text = Math.Round(mMh(0) / 10000, 4)
            lblAccepted.Text = Trim(mShares(0))
            lblInvalid.Text = Trim(mInvalids(0))
            lblStale.Text = Trim(mInvalids(0))
        Catch ex As Exception

        End Try

    End Sub

    Public Sub ReStartGuiMiner()
        Try
            KillGuiMiner()
            RestartedMinerAt = Now
            Me.Visible = True

            For x = 1 To 4
                SetupRTB(x)
                mRTB(x).Clear()
                Dim rt As New Thread(AddressOf StartReaperThread)
                rt.Start(x)
            Next


        Catch ex As Exception
        End Try

    End Sub


    Private Sub timerReaper_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles timerReaper.Tick
        Try

        updateGh()
        For x = 1 To 4
            If mEnabled(x) Then
                mRTB(x).Text = msReaperOut(x)
            End If

                If Len(mRTB(x).Text) > 10000 Then msReaperOut(x) = ""

            mRTB(x).SelectionStart = mRTB(x).Text.Length
            mRTB(x).SelectionLength = 0
            mRTB(x).ScrollToCaret()

            Next
        Catch ex As Exception

        End Try

    End Sub

    Private Sub TabPage2_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TabPage2.Click

    End Sub

    Private Sub Chart1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Chart1.Click

    End Sub

    Private Sub Tc1_SelectedIndexChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Tc1.SelectedIndexChanged

    End Sub

    Private Sub ChartUtilization_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ChartUtilization.Click

    End Sub
End Class