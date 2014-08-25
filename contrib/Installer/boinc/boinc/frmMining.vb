Imports System.IO
Imports System.Runtime.InteropServices
Imports System.Windows.Forms
Imports System.Drawing
Imports System.Diagnostics
Imports System.Timers
Imports System.Windows.Forms.DataVisualization.Charting
Imports System.Threading
Imports BoincStake

Public Class frmMining
    Private MaxHR As Double = 1
    Private LastMHRate As String = ""
    Private lMHRateCounter As Long = 0
    Private mIDelay As Long = 0

    Private clsUtilization As Utilization
    Private RefreshCount As Long
    Private bUICharted As Boolean = False
    Public bDisposing As Boolean
    Public bSuccessfullyLoaded As Boolean
    Private bCharting As Boolean
    Private mEnabled(10) As Boolean
    Private msReaperOut(10) As String
    Private miInitCounter As Long
    Private msLastBlockHash As String
    Private mlElapsedTime As Long
    Private msLastSleepStatus As String

    Public Sub SetClsUtilization(c As Utilization)
        clsUtilization = c
    End Sub
  
    Private Sub UpdateCharts()
        Try
            ChartBoinc()
            UpdateChartHashRate()
            ChartBoincUtilization()

            Me.Update()
        Catch ex As Exception
        End Try
    End Sub

    Private Sub OneMinuteUpdate()
        Try

             ChartBoincUtilization()

        Catch exx As Exception
            Log("One minute update:" + exx.Message)
        End Try
    End Sub
    Public Function GetBoincProgFolder() As String
        Dim sAppDir As String
        sAppDir = KeyValue("boincappfolder")
        If Len(sAppDir) > 0 Then Return sAppDir
        Dim bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9 As String
        bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles)
        bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9 = Trim(Replace(bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9, "(x86)", ""))
        bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9 = bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9 + "\Boinc\"
        If Not System.IO.Directory.Exists(bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9) Then
            bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86) + mclsUtilization.Des3Decrypt("sEl7B/roaQaNGPo+ckyQBA==")
        End If
        Return bigtime3f7o6l0daedrf4597acff2affbb5ed209f439aFroBearden0edd44ae1167a1e9be6eeb5cc2acd9c9
    End Function

    Public Sub ChartBoinc()
        Exit Sub

        Dim last_total As Double
        Dim seriesAvgCredits As Series : Dim seriesTotalCredits As Series : Dim seriesProjects As Series

        Try
            If bCharting Then Exit Sub
            bCharting = True
            If Chart1.Titles.Count < 1 Then
                Chart1.Series.Clear()

                Chart1.Titles.Clear()
                Chart1.Titles.Add("Boinc Utilization")
                Chart1.BackColor = Color.Transparent : Chart1.ForeColor = Color.Lime
                Chart1.ChartAreas(0).AxisX.IntervalType = DateTimeIntervalType.Weeks : Chart1.ChartAreas(0).AxisX.TitleForeColor = Color.White
                Chart1.ChartAreas(0).BackSecondaryColor = Color.Transparent : Chart1.ChartAreas(0).AxisX.LabelStyle.ForeColor = Color.Lime
                Chart1.ChartAreas(0).AxisY.LabelStyle.ForeColor = Color.Lime : Chart1.ChartAreas(0).ShadowColor = Color.Chocolate
                Chart1.ChartAreas(0).BackSecondaryColor = Color.Gray : Chart1.ChartAreas(0).BorderColor = Color.Gray
                Chart1.Legends(0).ForeColor = Color.Lime
                Chart1.ChartAreas(0).AxisX.LabelStyle.Format = "MM-dd-yyyy"
                seriesAvgCredits = New Series

                seriesAvgCredits.Name = "Avg Daily Credits" : seriesAvgCredits.ChartType = SeriesChartType.Line
                seriesAvgCredits.LabelForeColor = Color.Lime
                seriesTotalCredits = New Series
                seriesTotalCredits.ChartType = SeriesChartType.FastLine
                seriesTotalCredits.Name = "Total Daily Credits"
                seriesProjects = New Series
                seriesProjects.Name = "Projects" : Chart1.ChartAreas(0).AxisX.Interval = 1 : seriesProjects.ChartType = SeriesChartType.StepLine
                Chart1.Series.Add(seriesTotalCredits)
                Chart1.Series.Add(seriesAvgCredits)
                Chart1.Series.Add(seriesProjects)

            End If

            seriesAvgCredits.Points.Clear()
            seriesTotalCredits.Points.Clear()
            seriesProjects.Points.Clear()


            Dim dProj As Double
            Dim lookback As Double
            For x = 30 To 0.5 Step -6
                lookback = x * 3600 * 24
                'clsGVM.ReturnBoincCreditsAtPointInTime(lookback)
                Dim l1 As Double
                Dim l2 As Double
                Dim l3 As Double
                ' l1 = clsGVM.BoincCreditsAvgAtPointInTime
                ' clsGVM.ReturnBoincCreditsAtPointInTime(lookback - (3600 * 24))
                'l2 = clsGVM.BoincCreditsAtPointInTime
                l3 = Math.Abs(l2 - last_total)
                last_total = l2
                If l3 > (l1 * 5) Then l3 = l1
                Application.DoEvents()
                Dim pCreditsAvg As New DataPoint
                ' dProj = clsGVM.BoincProjects
                Dim d1 As Date = DateAdd(DateInterval.Day, -x, Now)
                pCreditsAvg.SetValueXY(d1, l1)
                seriesAvgCredits.Points.Add(pCreditsAvg)
                Dim dpProj As New DataPoint()
                dpProj.SetValueXY(d1, dProj * (l1 / 10))
                seriesProjects.Points.Add(dpProj)
                Dim pCreditsTotal As New DataPoint()
                pCreditsTotal.SetValueXY(d1, l3)
                seriesTotalCredits.Points.Add(pCreditsTotal)
            Next
            'Chart1.Refresh()
        Catch ex As Exception
        End Try
        bCharting = False
    End Sub
    Public Sub ChartBoincUtilization()
        Exit Sub

           End Sub

    Private Sub Timer1_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles tOneMinute.Tick

       
    End Sub

    Private Sub btnRestartMiner_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnRestartMiner.Click
        Try
       
        Catch ex As Exception
        End Try
    End Sub
    Private Sub RefreshRestartMinutes()
    End Sub

    Private Sub frmMining_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
      
    End Sub
    Private Sub frmMining_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing

        If bDisposing Then
       
            Me.Close()
            Me.Dispose()
            Exit Sub
        End If
        Me.Hide()
        e.Cancel = True
    End Sub
   
   
    Private Sub btnHide_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnHide.Click
        Me.Hide()
    End Sub
    Private Sub InitializeFormMining()

    End Sub
   
    Private Sub HideToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles HideToolStripMenuItem.Click
        Me.Hide()
    End Sub

   
    Public Sub UpdateChartHashRate()
        Exit Sub

        Try
            ChartHashRate.Series.Clear()
            ChartHashRate.Titles.Clear()
            ChartHashRate.BackColor = Color.Transparent
            ChartHashRate.ForeColor = Color.Red
            ChartHashRate.Titles.Add("GPU Hash Rate")
            ChartHashRate.Titles(0).ForeColor = Color.Green

            ChartHashRate.ChartAreas(0).BackColor = Color.Transparent
            ChartHashRate.ChartAreas(0).BackSecondaryColor = Color.PaleVioletRed
            Dim sHR As New Series
            sHR.Name = "HR"
            sHR.ChartType = SeriesChartType.Pie
            sHR.LabelBackColor = Color.Lime
            sHR.IsValueShownAsLabel = False
            sHR.LabelForeColor = Color.Honeydew
            ChartHashRate.Series.Add(sHR)
            Dim dHR As Double
            dHR = Val(lblGPUMhs.Text)
            If dHR > MaxHR Then MaxHR = dHR
            '''''''''''''''''''''''''''''''Add Max Hash Rate

            ChartHashRate.Series("HR").Points.AddY(MaxHR)
            ChartHashRate.Series("HR").LabelBackColor = Color.Transparent
            ChartHashRate.Series("HR").Points(0).Label = Trim(dHR)
            ChartHashRate.Series("HR").Points(0).Color = Color.Blue
            ChartHashRate.Series("HR").Points(0).LegendToolTip = Trim(dHR) + " Hash Rate"
            ''''''''''''''''''''''''''''''''''Add Actual Hash Rate
            ChartHashRate.Series("HR").Points.AddY(MaxHR - dHR)
            ChartHashRate.Series("HR")("PointWidth") = "0.5"
            ChartHashRate.Series("HR").IsValueShownAsLabel = False
            ChartHashRate.Series("HR")("BarLabelStyle") = "Center"
            ChartHashRate.ChartAreas(0).Area3DStyle.Enable3D = True
            ChartHashRate.Series("HR")("DrawingStyle") = "Cylinder"
            ChartHashRate.Update()

        Catch ex As Exception
        End Try

    End Sub
    Private Sub frmMining_Load(sender As Object, e As System.EventArgs) Handles Me.Load
    
        Try

        If KeyValue("suppressminingconsole") = "true" Then Exit Sub
            Call OneMinuteUpdate()
        BoincWebBrowser.ScriptErrorsSuppressed = True

        BoincWebBrowser.Navigate("http://boincstats.com/en/stats/-1/team/detail/118094994/overview")
        Catch ex As Exception

        End Try


    End Sub
    Private Sub PersistCheckboxes()
        If Not bSuccessfullyLoaded Then Exit Sub

        Try

        'Set the defaults for the checkboxes
        UpdateKey("chkFullSpeed", chkFullSpeed.Checked.ToString)
        UpdateKey("chkMiningEnabled", chkMiningEnabled.Checked.ToString)
        UpdateKey("chkCGMonitor", chkCGMonitor.Checked.ToString)
        Catch ex As Exception

        End Try

    End Sub
   
   
    Public Sub New()

        ' This call is required by the designer.
        InitializeComponent()

  

    End Sub

 
    Private Sub PoolsToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles PoolsToolStripMenuItem.Click
 
    End Sub

End Class