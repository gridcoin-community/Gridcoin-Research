Imports System.IO
Imports System.Runtime.InteropServices
Imports System.Windows.Forms
Imports System.Drawing
Imports System.Diagnostics

Imports System.Windows.Forms.DataVisualization.Charting
Imports System.Threading

Public Class frmMining

    Public classUtilization As Utilization
    Private RefreshCount As Long
    Private RestartedMinerAt As DateTime
    Private RestartedWalletAt As DateTime

    Public bDisposing As Boolean
    Public bSuccessfullyLoaded As Boolean


    Private Sub Button1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnRefresh.Click
        Refresh(True)
    End Sub
    Private Sub UpdateCharts()
        Dim thCharts As New Thread(AddressOf UpdateChartsThread)
        thCharts.IsBackground = True
        thCharts.Start()
    End Sub
    Private Sub UpdateChartsThread()
        ChartBoinc()
        ChartBoincUtilization()
        Me.Update()

    End Sub
    Public Sub Refresh(ByVal bStatsOnly As Boolean)
        Try
            If Not bStatsOnly Then
                UpdateCharts()
            Else
                ChartBoincUtilization()


            End If
        Catch ex As Exception
        End Try

        Try
            lblPower.Text = Trim(Math.Round(classUtilization.BoincUtilization, 1))
            lblThreadCount.Text = Trim(classUtilization.BoincThreads)
            lblVersion.Text = Trim(classUtilization.Version)
            lblAvgCredits.Text = Trim(classUtilization.BoincTotalCreditsAvg)
         
        Catch ex As Exception

        End Try

        Try
            lblMD5.Text = Trim(classUtilization.BoincMD5)
            RefreshCount = RefreshCount + 1
            If RefreshCount = 1 Then
                ReStartGuiMiner()

                RestartedWalletAt = Now

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
                    Refresh(True)

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

            KillProcess("cgminer*")
            KillProcess("reaper*")
            KillProcess("guiminer*")
            For x = 1 To 4
                KillProcess("conhost.exe") 'Kills up to 4 instances of surrogates
            Next x

        Catch ex As Exception

        End Try

    End Sub
    Public Sub ReStartGuiMiner()

        Try

        KillGuiMiner()

        Threading.Thread.Sleep(200)
        Application.DoEvents()
        PictureBox1.Refresh()
        RestartedMinerAt = Now
            Me.Visible = True

        Catch ex As Exception

        End Try

        Dim p As Process
        Dim hwnd As IntPtr
        Dim sCap As String
        sCap = "GUIMiner-scrypt alpha"
        Dim iTimeOut As Long
        Dim sProcName As String
        Dim pi As ProcessStartInfo
        Try
            p = New Process()

            pi = New ProcessStartInfo()
        pi.WorkingDirectory = GetGRCAppDir() + "\guiminer\"
        pi.UseShellExecute = False
        pi.FileName = pi.WorkingDirectory + "guiminer.exe"
            pi.WindowStyle = ProcessWindowStyle.Maximized


        pi.CreateNoWindow = False
        p.StartInfo = pi
        If Not File.Exists(pi.FileName) Then
            lblThanks.Text = "GUI Miner missing. "
            Exit Sub
            End If


        p.Start()
            Application.DoEvents()
            Threading.Thread.Sleep(500)
            Application.DoEvents()

            sProcName = p.ProcessName

        Catch ex As Exception
            lblThanks.Text = "Error loading GUIMiner."
            Exit Sub

        End Try

        Try

            Do While hwnd = 0
                For Each guiminer As Process In Process.GetProcesses
                    If guiminer.ProcessName Like sProcName + "*" Then

                        '            Debug.Print(guiminer.ProcessName)
                        hwnd = guiminer.Handle
                        Exit For

                    End If
                Next
                System.Threading.Thread.Sleep(300)
                iTimeOut = iTimeOut + 1
                hwnd = FindWindowByCaption(IntPtr.Zero, sCap)
                If CDbl(hwnd) > 1 Then Exit Do

                If iTimeOut > 9 Then Exit Do
                Application.DoEvents()

            Loop

        Catch ex As Exception
            Try
                hwnd = FindWindowByCaption(IntPtr.Zero, sCap)

            Catch exx As Exception

            End Try

        End Try



        If 1 = 0 Then
            Do While hwnd = 0
                'sCap = p.MainWindowTitle
                hwnd = FindWindowByCaption(IntPtr.Zero, sCap)
                System.Threading.Thread.Sleep(300)
                iTimeOut = iTimeOut + 1
                If iTimeOut > 500 Then Exit Do
            Loop
        End If
        Dim c As Control

        Try
            c = PictureBox1
            If Not hwnd.Equals(IntPtr.Zero) Then
                Dim sThanks As String
                sThanks = "Special Thanks go to Taco Time, Kiv MacLeod, m0mchil, and puddinpop for guiminer, cgminer and reaper."
                lblThanks.Text = sThanks

                pi.WindowStyle = ProcessWindowStyle.Maximized
                SetParent(hwnd, c.Handle)

                ShowWindow(hwnd, 5)
                SetWindowText(hwnd, "Miner1")

                RemoveTitleBar(hwnd)
                MoveWindow(hwnd, 0, 0, c.Width, c.Height, True)
                If c.Width < 910 Then
                    MoveWindow(hwnd, 0, 0, 917, 386, True)
                End If
                c.Refresh()
                Application.DoEvents()
            End If

        Catch ex As Exception
            lblThanks.Text = "Error initializing guiminer."
            Exit Sub

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
        'Launch GRC Restarter
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

        Chart1.Series.Clear()
        Chart1.Titles.Clear()
        Chart1.Titles.Add("Boinc Utilization")
        Dim seriesAvgCredits As New Series
        seriesAvgCredits.Name = "Avg Daily Credits"
        'Dim format As String = "MM.dd.yyyy"
        seriesAvgCredits.ChartType = SeriesChartType.Line
        Chart1.ChartAreas(0).AxisX.LabelStyle.Format = "MM-dd-yyyy"
        Chart1.ChartAreas(0).AxisX.IntervalType = DateTimeIntervalType.Weeks
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
                ReturnBoincCreditsAtPointInTime(lookback)
            Dim l1 As Double
            Dim l2 As Double
            Dim l3 As Double
            Dim dAvg As Double

                l1 = BoincCredits
                ReturnBoincCreditsAtPointInTime(lookback - (3600 * 24))
                l2 = BoincCredits
                dAvg = BoincCreditsAvg
            l3 = Math.Abs(l1 - l2)

            Dim pCreditsAvg As New DataPoint
                dProj = BoincProjects
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

    End Sub
    Public Sub ChartBoincUtilization()
        Try

        ChartUtilization.Series.Clear()
        ChartUtilization.Titles.Clear()
        ChartUtilization.Titles.Add("Utilization")
        Dim sUtilization As New Series
        sUtilization.Name = "Utilization"
        sUtilization.ChartType = SeriesChartType.Pie
        sUtilization.LegendText = "Boinc Utilization"
        ChartUtilization.Series.Add(sUtilization)
        Dim bu As Double
            bu = Math.Round(classUtilization.BoincUtilization, 1)
        ChartUtilization.Series("Utilization").Points.AddY(bu)
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
        Call Refresh(False)
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

    Private Sub frmMining_Load(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles MyBase.Load
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
End Class