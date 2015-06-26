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
    Private msNeuralReport As String = ""

    'Private clsUtilization As Utilization
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


    Private Sub UpdateCharts()
        Try
            ChartBoinc()
            UpdateChartHashRate()
           
            Me.Update()
        Catch ex As Exception
        End Try
    End Sub

    Private Sub OneMinuteUpdate()
        Try

            ChartBoinc()

        Catch exx As Exception
            Log("One minute update:" + exx.Message)
        End Try
    End Sub
    

    Public Sub ChartBoinc()
        
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

                Dim l1 As Double
                Dim l2 As Double
                Dim l3 As Double
              
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

        Catch ex As Exception
        End Try
        bCharting = False
    End Sub
    
    
    Private Sub frmMining_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
      
    End Sub
    Private Sub frmMining_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing

        Me.Hide()
        e.Cancel = True
    End Sub
   
   
    Private Sub btnHide_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnHide.Click
        Me.Hide()
    End Sub
   
    Private Sub HideToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles HideToolStripMenuItem.Click
        Me.Hide()
    End Sub

   
    Public Sub UpdateChartHashRate()
    
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
           
            '''''''''''''''''''''''''''''''Add Max Hash Rate

        
        Catch ex As Exception
        End Try

    End Sub
    Private Sub frmMining_Load(sender As Object, e As System.EventArgs) Handles Me.Load
    
        Try

            Call OneMinuteUpdate()
            Me.TabControl1.SelectedIndex = 2
            If mbTestNet Then lblTestnet.Text = "TESTNET"

        Catch ex As Exception

        End Try


    End Sub
   
   
    Public Sub New()
        InitializeComponent()
    End Sub


    Public Sub PopulateNeuralData()

        Dim sReport As String = ""
        Dim sReportRow As String = ""

        Dim sHeader As String = "CPID,Magnitude,RAC,Expiration,Synced,Address,CPID_Valid"
        sReport += sHeader + vbCrLf

        dgv.Rows.Clear()
        dgv.Columns.Clear()
        dgv.BackgroundColor = Drawing.Color.Black
        dgv.ForeColor = Drawing.Color.Lime
        Dim grr As New GridcoinReader.GridcoinRow
        Dim sHeading As String = "CPID;Magnitude;RAC;Expiration;Synced;Address;CPID_Valid"

        Dim vHeading() As String = Split(sHeading, ";")

        PopulateHeadings(vHeading, dgv)

        Dim sData As String = modPersistedDataSystem.GetMagnitudeContractDetails()

        Dim vData() As String = Split(sData, ";")
        Dim iRow As Long = 0
        Dim sValue As String
        For y = 0 To UBound(vData) - 1
            dgv.Rows.Add()
            sReportRow = ""
            For x = 0 To UBound(vHeading)
                Dim vRow() As String = Split(vData(y), ",")
                sValue = vRow(x)
                dgv.Rows(iRow).Cells(x).Value = sValue
                sReportRow += sValue + ","
            Next x
            sReport += sReportRow + vbCrLf
            iRow = iRow + 1
        Next
        'Get the Neural Hash
        Dim sMyNeuralHash As String
        Dim sContract = GetMagnitudeContract()
        sMyNeuralHash = GetMd5String(sContract)
        dgv.Rows.Add()
        dgv.Rows(iRow).Cells(0).Value = "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"
        sReport += "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"
        msNeuralReport = sReport


    End Sub

    Private Sub TabControl1_SelectedIndexChanged(sender As Object, e As System.EventArgs) Handles TabControl1.SelectedIndexChanged
        If TabControl1.SelectedIndex = 2 Then
            PopulateNeuralData()

        End If
    End Sub


    Private Sub TabControl1_TabIndexChanged(sender As Object, e As System.EventArgs) Handles TabControl1.TabIndexChanged
        
    End Sub

    Private Sub dgv_CellContentClick(sender As System.Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentClick

    End Sub

    Private Sub dgv_CellContentDoubleClick(sender As Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentDoubleClick
        'Drill into CPID
        If e.RowIndex < 0 Then Exit Sub

        Dim sCPID As String = Trim(dgv.Rows(e.RowIndex).Cells(0).Value)
        If sCPID.Contains("Hash") Then Exit Sub

        If Len(sCPID) > 1 Then
            '6-7-2015
            Dim dgvProjects As New DataGridView
            Dim sHeading As String = "CPID,Project,RAC,ProjectAvgRAC,Expiration"
            Dim vHeading() As String = Split(sHeading, ",")
            PopulateHeadings(vHeading, dgvProjects)

            Dim surrogatePrj As New Row
            surrogatePrj.Database = "Project"
            surrogatePrj.Table = "Projects"
            Dim lstProjects As List(Of Row) = GetList(surrogatePrj, "*")
            Dim iRow As Long = 0
            dgvProjects.Rows.Clear()

            For Each prj As Row In lstProjects
                Dim surrogatePrjCPID As New Row
                surrogatePrjCPID.Database = "Project"
                surrogatePrjCPID.Table = prj.PrimaryKey + "CPID"
                surrogatePrjCPID.PrimaryKey = prj.PrimaryKey + "_" + sCPID
                Dim rowRAC = Read(surrogatePrjCPID)
                Dim CPIDRAC As Double = Val(rowRAC.RAC)
                Dim PrjRAC As Double = Val(prj.RAC)
                Dim avgRac As Double = CPIDRAC / (PrjRAC + 0.01) * 100

                If CPIDRAC > 0 Then
                    iRow += 1

                    dgvProjects.Rows.Add()
                    dgvProjects.Rows(iRow - 1).Cells(0).Value = sCPID
                    dgvProjects.Rows(iRow - 1).Cells(1).Value = prj.PrimaryKey
                    dgvProjects.Rows(iRow - 1).Cells(2).Value = Trim(CPIDRAC)
                    dgvProjects.Rows(iRow - 1).Cells(3).Value = Trim(PrjRAC)
                    dgvProjects.Rows(iRow - 1).Cells(4).Value = Trim(prj.Expiration)

                End If

            Next

            Dim oNewForm As New Form
            oNewForm.Width = Screen.PrimaryScreen.WorkingArea.Width / 2
            oNewForm.Height = Screen.PrimaryScreen.WorkingArea.Height / 2.5

            oNewForm.Controls.Add(dgvProjects)
            dgvProjects.Left = 5
            dgvProjects.Top = 5
            Dim TotalControlHeight As Long = (dgvProjects.RowTemplate.Height * (iRow + 2)) + 20
            dgvProjects.Height = TotalControlHeight
            oNewForm.Height = dgvProjects.Height + 285
            dgvProjects.Width = oNewForm.Width - 25
            Dim rtbRac As New System.Windows.Forms.RichTextBox

            Dim sXML As String = GetXMLOnly(sCPID)
            rtbRac.Left = 5
            rtbRac.Top = dgvProjects.Height + 8
            rtbRac.Height = 245
            rtbRac.Width = oNewForm.Width - 5
            rtbRac.Text = sXML
            oNewForm.Controls.Add(rtbRac)
            oNewForm.Show()


        End If


    End Sub

    Private Sub ContractDetailsToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles ContractDetailsToolStripMenuItem.Click
        Dim sData As String = GetMagnitudeContract()
        MsgBox(sData)

    End Sub

    Private Sub btnExport_Click(sender As System.Object, e As System.EventArgs) Handles btnExport.Click
        Dim sWritePath As String = GetGridFolder() + "reports\NeuralMagnitudeReport.csv"
        If Not System.IO.Directory.Exists(GetGridFolder() + "reports") Then MkDir(GetGridFolder() + "reports")

        Using objWriter As New System.IO.StreamWriter(sWritePath)
            objWriter.WriteLine(msNeuralReport)
            objWriter.Close()
        End Using
        ExportToCSV2()

        MsgBox("Exported to Reports\" + "NeuralMagnitudeReport.csv")

    End Sub

    Private Sub TextBox1_TextChanged(sender As System.Object, e As System.EventArgs) Handles txtSearch.TextChanged
        Dim sPhrase As String = txtSearch.Text
        For y = 1 To dgv.Rows.Count - 1
            For x = 0 To dgv.Rows(y).Cells.Count - 1
                If LCase(Trim("" & dgv.Rows(y).Cells(x).Value)).Contains(LCase(Trim(txtSearch.Text))) Then
                    dgv.Rows(y).Selected = True
                    dgv.CurrentCell = dgv.Rows(y).Cells(0)
                    Exit Sub
                End If
            Next
        Next
    End Sub
End Class