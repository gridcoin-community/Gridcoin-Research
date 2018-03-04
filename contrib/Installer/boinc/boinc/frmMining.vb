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
    Private WM_SETREDRAW = &HB
    Private dgvDrillProjects As New DataGridView
    Private rtbDrillRAC As New RichTextBox

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
            lblCPID.Text = "CPID: " + KeyValue("PrimaryCPID")
            Dim r As Row = GetDataValue("Historical", "Magnitude", "LastTimeSynced")

            lblLastSynced.Text = "Last Synced: " + Trim(r.Synced)
            r = GetDataValue("Historical", "Magnitude", "QuorumHash")

            lblSuperblockAge.Text = "Superblock Age: " + Trim(r.DataColumn1)
            lblQuorumHash.Text = "Popular Quorum Hash: " + Trim(r.DataColumn2)
            lblTimestamp.Text = "Superblock Timestamp: " + Trim(r.DataColumn3)
            lblBlock.Text = "Superblock Block #: " + Trim(r.DataColumn4)


        Catch exx As Exception
            Log("One minute update:" + exx.Message)
        End Try
    End Sub
    
    Public Sub ChartBoinc()
        'Dim seriesAvgCredits As New Series
        Dim seriesNetworkMagnitude As New Series
        Dim seriesUserMagnitude As New Series

        Try
            'If bCharting Then Exit Sub
            'bCharting = True
            If Chart1.Titles.Count < 1 Then
                Chart1.Series.Clear()

                Chart1.Titles.Clear()
                Chart1.Titles.Add("Historical Contribution")
                Chart1.Titles(0).ForeColor = Color.LightGreen
                Chart1.BackColor = Color.Transparent : Chart1.ForeColor = Color.Lime
                Chart1.ChartAreas(0).AxisX.IntervalType = DateTimeIntervalType.Weeks : Chart1.ChartAreas(0).AxisX.TitleForeColor = Color.White
                Chart1.ChartAreas(0).BackSecondaryColor = Color.Transparent : Chart1.ChartAreas(0).AxisX.LabelStyle.ForeColor = Color.Lime
                Chart1.ChartAreas(0).AxisY.LabelStyle.ForeColor = Color.Lime : Chart1.ChartAreas(0).ShadowColor = Color.Chocolate
                Chart1.ChartAreas(0).BackSecondaryColor = Color.Gray : Chart1.ChartAreas(0).BorderColor = Color.Gray
                Chart1.Legends(0).ForeColor = Color.Lime
                Chart1.ChartAreas(0).AxisX.LabelStyle.Format = "MM-dd-yyyy"
                Chart1.ChartAreas(0).AxisX.Interval = 2
                Chart1.ForeColor = Color.GreenYellow
                'Network Magnitude
                seriesNetworkMagnitude.ChartType = SeriesChartType.FastLine
                seriesNetworkMagnitude.Name = "Network Magnitude"
                ' seriesNetworkMagnitude.LabelForeColor = Color.GreenYellow
                Chart1.Series.Add(seriesNetworkMagnitude)
                'User Magnitude
                seriesUserMagnitude.ChartType = SeriesChartType.FastLine
                seriesUserMagnitude.Name = "User Magnitude"
                'seriesUserMagnitude.LabelForeColor = Color.GreenYellow
                Chart1.Series.Add(seriesUserMagnitude)
            End If
            seriesNetworkMagnitude.Points.Clear()
            seriesUserMagnitude.Points.Clear()
            '''''''''''''''''''''''''''''' Chart Bar of Historical Contribution ''''''''''''''''''''''''''''''''''''''''
            Dim lUserMag As Double = 0
            Dim lNetworkMag As Double = 0
            Dim lAvgNetMag As Double = 0
            Dim lAvgUserMag As Double = 0
            Dim sCPID As String = KeyValue("PrimaryCPID")
            Dim lAvgUserMagH As Long = 0
            Dim lAvgUserMagHCount As Long = 0
            Dim lAvgNetworkMag As Long = 0
            Dim lAvgNetworkMagCount As Long = 0
            Dim lAUM As Long = 0
            Dim lANM As Long = 0
            For x = 30 To 1 Step -1
                'Dim dpAvgCredits As New DataPoint
                'dpAvgCredits.SetValueXY(ChartDate, lAvgCredits)
                'seriesAvgCredits.Points.Add(pCreditsAvg)
                Dim ChartDate As Date = DateAdd(DateInterval.Day, -x, Now)
                lUserMag = GetHistoricalMagnitude(ChartDate, sCPID, lAvgUserMag)
                lNetworkMag = GetHistoricalMagnitude(ChartDate, "Network", lAvgNetMag)
                If lUserMag > 0 Then
                    lAvgUserMagH += lUserMag
                    lAvgUserMagHCount += 1
                    lAUM = lAvgUserMagH / lAvgUserMagHCount
                End If
                If lNetworkMag > 0 Then
                    lAvgNetworkMag += lNetworkMag
                    lAvgNetworkMagCount += 1
                    lANM = lAvgNetworkMag / lAvgNetworkMagCount
                End If
                Dim dpUserMag As New DataPoint()
                dpUserMag.SetValueXY(ChartDate, lAUM)
                seriesUserMagnitude.Points.Add(dpUserMag)
                Dim dpNetworkMag As New DataPoint()
                dpNetworkMag.SetValueXY(ChartDate, lANM)
                seriesNetworkMagnitude.Points.Add(dpNetworkMag)
            Next
            '''''''''''''''''''''''''''''''  Chart Pie of Current Contribution '''''''''''''''''''''''''''''''''''''''''

            Call ChartBoincUtilization(lAUM, lANM)

        Catch ex As Exception

        End Try
        'bCharting = False
    End Sub

    Public Sub ChartBoincUtilization(bu As Long, netBU As Long)
        Try
            chtCurCont.Titles.Clear()

            If chtCurCont.Titles.Count < 1 Then
                chtCurCont.Series.Clear()
                chtCurCont.Titles.Clear()
                chtCurCont.BackColor = Color.Transparent : chtCurCont.ForeColor = Color.Blue
                chtCurCont.Titles.Add("Contribution")
                chtCurCont.Titles(0).ForeColor = Color.LightGreen
                chtCurCont.ChartAreas(0).BackColor = Color.Transparent
                chtCurCont.ChartAreas(0).BackSecondaryColor = Color.White
                chtCurCont.Legends(0).BackColor = Color.Transparent
                chtCurCont.Legends(0).ForeColor = Color.Honeydew
                Dim sUtilization As New Series
                sUtilization.Name = "Magnitude" : sUtilization.ChartType = SeriesChartType.Pie
                sUtilization.LegendText = "Boinc Magnitude"
                sUtilization.LabelBackColor = Color.Lime : sUtilization.IsValueShownAsLabel = False
                sUtilization.LabelForeColor = Color.Honeydew
                chtCurCont.Series.Add(sUtilization)
            End If
            chtCurCont.Series(0).Points.Clear()
            If Not bUICharted Then bUICharted = True : bu = 2
            chtCurCont.Series(0).Points.AddY(bu)
            chtCurCont.Series(0).LabelBackColor = Color.Transparent
            chtCurCont.Series(0).Points(0).Label = Trim(bu)
            chtCurCont.Series(0).Points(0).Color = Color.Blue
            chtCurCont.Series(0).Points(0).LegendToolTip = Trim(bu) + " magnitude"
            chtCurCont.Series(0).Points.AddY(netBU - bu)
            chtCurCont.Series(0).Points(1).IsVisibleInLegend = False
            chtCurCont.Series(0)("PointWidth") = "0.5"
            chtCurCont.Series(0).IsValueShownAsLabel = False
            chtCurCont.Series(0)("BarLabelStyle") = "Center"
            chtCurCont.ChartAreas(0).Area3DStyle.Enable3D = True
            chtCurCont.Series(0)("DrawingStyle") = "Cylinder"
        Catch ex As Exception
            Dim sMsg As String = ex.Message

        End Try
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


        Catch ex As Exception
        End Try

    End Sub
    Private Sub frmMining_Load(sender As Object, e As System.EventArgs) Handles Me.Load

        Try

            Call OneMinuteUpdate()
            Me.TabControl1.SelectedIndex = 2
            If mbTestNet Then lblTestnet.Text = "TESTNET"
            PopulateNeuralData()

        Catch ex As Exception

        End Try


    End Sub


    Public Sub New()
        InitializeComponent()
    End Sub
    Public Sub PopulateNeuralDataViaContractFile()
        Dim sReport As String = ""
        Dim sReportRow As String = ""
        Dim sMemoryName = IIf(mbTestNet, "magnitudes_testnet", "magnitudes")
        Dim sHeader As String = "CPID,Magnitude,Avg Magnitude,Total RAC,Synced Til,Address,CPID Valid,Witnesses,Rank"
        sReport += sHeader + vbCrLf
        dgv.Rows.Clear()
        dgv.Columns.Clear()
        dgv.BackgroundColor = Drawing.Color.Black
        dgv.ForeColor = Drawing.Color.Lime
        dgv.ReadOnly = True
        Dim grr As New GridcoinReader.GridcoinRow
        Dim vHeading() As String = Split(sHeader, ",")
        PopulateHeadings(vHeading, dgv, False)
        dgv.Columns(2).Visible = False
        Dim sData As String = modPersistedDataSystem.GetMagnitudeContract()
        Dim sMagnitudes = ExtractXML(sData, "<MAGNITUDES>")
        Dim sProjects = ExtractXML(sData, "<AVERAGES>")
        Dim iRow As Long = 0
        Dim sValue As String
        dgv.AutoResizeColumns(DataGridViewAutoSizeColumnsMode.ColumnHeader)
        dgv.ReadOnly = True
        dgv.EditingPanel.Visible = False
        dgv.Columns(7).Visible = False
        For x = 0 To 7
            If x = 0 Or x = 1 Then
                dgv.Columns(x).Visible = True
            Else
                dgv.Columns(x).Visible = False
            End If
        Next
        Dim dTotalMag As Double = 0
        Dim dAvgMag As Double = 0

        dgv.Columns(8).Visible = True
        Dim vMagnitudes() As String = Split(sMagnitudes, ";")
        Dim vProjects() As String = Split(sProjects, ";")
        For y = 0 To UBound(vMagnitudes) - 1
            Dim vRow() As String = Split(vMagnitudes(y), ",")
            If Len(vRow(0)) > 10 Then
                dgv.Rows.Add()
                sReportRow = ""
                dgv.Rows(iRow).Cells(0).Value = vRow(0)
                dgv.Rows(iRow).Cells(1).Value = Val(vRow(1))
                dTotalMag += Val(vRow(1))

                iRow = iRow + 1
            End If
        Next

        dAvgMag = dTotalMag / (iRow + 0.01)

        'Get the Neural Hash
        Dim sMyNeuralHash As String
        Dim sContract = GetMagnitudeContract()
        sMyNeuralHash = GetQuorumHash(sContract)
        dgv.Rows.Add()
        dgv.Rows(iRow).Cells(0).Value = "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"
        sReport += "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"
        'Populate Projects

        dgvProjects.Rows.Clear()
        dgvProjects.Columns.Clear()
        dgvProjects.EditingPanel.Visible = False
        dgvProjects.AllowUserToAddRows = False
        dgvProjects.BackgroundColor = Drawing.Color.Black
        dgvProjects.ForeColor = Drawing.Color.Lime
        Dim sHeading As String = "Project Name;Total RAC;Avg RAC;Whitelisted"
        vHeading = Split(sHeading, ";")
        PopulateHeadings(vHeading, dgvProjects, False)
        Dim surrogateRow As New Row
        Dim WhitelistedProjects As Double = 0
        Dim PrjCount As Double = 0
        iRow = 0
        dgvProjects.Columns(1).Visible = False

        For y = 0 To UBound(vProjects) - 1
            dgvProjects.Rows.Add()
            Dim vProjRow() As String = Split(vProjects(y), ",")
            dgvProjects.Rows(iRow).Cells(0).Value = vProjRow(0)
            dgvProjects.Rows(iRow).Cells(2).Value = vProjRow(2)
            dgvProjects.Rows(iRow).Cells(3).Value = "True"
            iRow += 1
        Next
        lblTotalProjects.Text = Trim(vProjects.Length)
        lblWhitelistedProjects.Text = Trim(vProjects.Length)
        dgv.Sort(dgv.Columns(1), System.ComponentModel.ListSortDirection.Descending)

        For y = 0 To dgv.Rows.Count - 2
            dgv.Rows(y).Cells(8).Value = y + 1
        Next
        'populate the average magnitude
        lblAvgMagnitude.Text = "Average Magnitude: " + Trim(dAvgMag)


        SetAutoSizeMode2(vHeading, dgv)

    End Sub



    Public Sub PopulateNeuralData()

        Dim sReport As String = ""
        Dim sReportRow As String = ""
        Dim sMemoryName = IIf(mbTestNet, "magnitudes_testnet", "magnitudes")
        If GetWindowsFileAge(GetGridPath("NeuralNetwork") + "\contract.dat") < 240 Then
            PopulateNeuralDataViaContractFile()
            Exit Sub
        End If
        Dim sHeader As String = "CPID,Magnitude,Avg Magnitude,Total RAC,Synced Til,Address,CPID Valid,Witnesses,Rank"
        sReport += sHeader + vbCrLf
Refresh:

        dgv.Rows.Clear()
        dgv.Columns.Clear()
        dgv.BackgroundColor = Drawing.Color.Black
        dgv.ForeColor = Drawing.Color.Lime
        dgv.ReadOnly = True

        Dim grr As New GridcoinReader.GridcoinRow
        Dim vHeading() As String = Split(sHeader, ",")

        PopulateHeadings(vHeading, dgv, False)
        dgv.Columns(2).Visible = False
        Dim sData As String = modPersistedDataSystem.GetMagnitudeContractDetails()
        Dim vData() As String = Split(sData, ";")
        Dim iRow As Long = 0
        Dim sValue As String
        'dgv.Visible = False
        Me.Cursor.Current = Cursors.WaitCursor
        dgv.AutoResizeColumns(DataGridViewAutoSizeColumnsMode.ColumnHeader)
        dgv.ReadOnly = True
        dgv.EditingPanel.Visible = False
        dgv.Columns(7).Visible = False '(Witnesses)

        For y = 0 To UBound(vData) - 1
            dgv.Rows.Add()
            sReportRow = ""
            For x = 0 To UBound(vHeading) - 1
                Dim vRow() As String = Split(vData(y), ",")
                sValue = vRow(x)
                'Sort numerically:
                If x = 1 Or x = 2 Or x = 3 Then
                    dgv.Rows(iRow).Cells(x).Value = Val(sValue)
                Else
                    dgv.Rows(iRow).Cells(x).Value = sValue
                End If
                sReportRow += sValue + ","
            Next x

            If LCase(dgv.Rows(iRow).Cells(0).Value) = "grc" Or LCase(dgv.Rows(iRow).Cells(0).Value) = "btc" Then
                dgv.Rows(iRow).Visible = False 'No need to pollute the view page with quotes
            End If

            sReport += sReportRow + vbCrLf
            iRow = iRow + 1
            If iRow Mod 50 = 0 Then Application.DoEvents()

        Next

        Me.Cursor.Current = Cursors.Default

        'Get the Neural Hash
        Dim sMyNeuralHash As String
        Dim sContract = GetMagnitudeContract()
        sMyNeuralHash = GetQuorumHash(sContract)
        dgv.Rows.Add()
        dgv.Rows(iRow).Cells(0).Value = "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"
        sReport += "Hash: " + sMyNeuralHash + " (" + Trim(iRow) + ")"

        msNeuralReport = sReport
        'Populate Projects

        dgvProjects.Rows.Clear()
        dgvProjects.Columns.Clear()

        dgvProjects.EditingPanel.Visible = False
        dgvProjects.AllowUserToAddRows = False


        dgvProjects.BackgroundColor = Drawing.Color.Black
        dgvProjects.ForeColor = Drawing.Color.Lime
        Dim sHeading As String = "Project Name;Total RAC;Avg RAC;Whitelisted"
        vHeading = Split(sHeading, ";")

        PopulateHeadings(vHeading, dgvProjects, False)

        Dim surrogateRow As New Row
        Dim lstWhitelist As List(Of Row)
        Dim surrogateWhitelistRow As New Row
        surrogateWhitelistRow.Database = "Whitelist"
        surrogateWhitelistRow.Table = "Whitelist"
        lstWhitelist = GetList(surrogateWhitelistRow, "*")
        Dim WhitelistedProjects As Double = 0
        Dim PrjCount As Double = 0
        iRow = 0

        'Loop through the whitelist
        lstWhitelist.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim rPRJ As New Row
        rPRJ.Database = "Project"
        rPRJ.Table = "Projects"

        Dim lstProjects As List(Of Row) = GetList(rPRJ, "*")
        lstProjects.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))

        PrjCount = lstWhitelist.Count
        For Each prj As Row In lstProjects
            Dim bIsThisWhitelisted = IsInList(prj.PrimaryKey, lstWhitelist, False)
            If bIsThisWhitelisted Then
                WhitelistedProjects += 1
            End If
            If prj.PrimaryKey <> "neuralnetwork" Then
                dgvProjects.Rows.Add()
                dgvProjects.Rows(iRow).Cells(0).Value = prj.PrimaryKey
                dgvProjects.Rows(iRow).Cells(1).Value = prj.RAC
                dgvProjects.Rows(iRow).Cells(2).Value = prj.AvgRAC
                dgvProjects.Rows(iRow).Cells(3).Value = Trim(bIsThisWhitelisted)
                iRow = iRow + 1
            End If
        Next
        lblTotalProjects.Text = Trim(PrjCount)
        lblWhitelistedProjects.Text = Trim(WhitelistedProjects)
        dgv.Sort(dgv.Columns(1), System.ComponentModel.ListSortDirection.Descending)

        For y = 0 To dgv.Rows.Count - 2
            dgv.Rows(y).Cells(8).Value = y + 1
        Next

        SetAutoSizeMode2(vHeading, dgv)

    End Sub
    Private Sub TabControl1_SelectedIndexChanged(sender As Object, e As System.EventArgs) Handles TabControl1.SelectedIndexChanged
    End Sub
    Private Shared Function InlineAssignHelper(Of T)(ByRef target As T, ByVal value As T) As T
        target = value
        Return value
    End Function
    Private Sub HighlightKeyword(o As RichTextBox, color__1 As Color, startIndex As Integer, sFind As String)
        If o.Text.Contains(sFind) Then
            Dim index As Integer = -1
            Dim selectStart As Integer = o.SelectionStart

            While (InlineAssignHelper(index, o.Text.IndexOf(sFind, (index + 1)))) <> -1
                o.[Select]((index + startIndex), sFind.Length)
                o.SelectionColor = color__1
                o.[Select](selectStart, 0)
                o.SelectionColor = Color.Black
            End While
        End If
    End Sub

    Private Sub dgv_CellContentDoubleClick(sender As Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentDoubleClick
        'Drill into CPID
        If e.RowIndex < 0 Then Exit Sub
        'Get whitelist total first
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

        'Loop through the whitelist
        lstWhitelist.Sort(Function(x, y) x.PrimaryKey.CompareTo(y.PrimaryKey))
        Dim TotalRAC As Double = 0
        Dim TotalNetworkRAC As Double = 0

        'Drill
        Dim sCPID As String = Trim(dgv.Rows(e.RowIndex).Cells(0).Value)
        If sCPID.Contains("Hash") Then Exit Sub
        If Len(sCPID) > 1 Then
            '7-10-2015 - Expose Project Mag and Cumulative Mag:
            dgvDrillProjects = New DataGridView

            Dim sHeading As String = "CPID,Project,RAC,Project Total RAC,Project Avg RAC,Project Mag,Cumulative RAC,Cumulative Mag"
            Dim vHeading() As String = Split(sHeading, ",")
            PopulateHeadings(vHeading, dgvDrillProjects, True)
            Dim surrogatePrj As New Row
            surrogatePrj.Database = "Project"
            surrogatePrj.Table = "Projects"
            Dim lstProjects As List(Of Row) = GetList(surrogatePrj, "*")
            Dim iRow As Long = 0
            dgvDrillProjects.Rows.Clear()
            dgvDrillProjects.ReadOnly = True

            Dim CumulativeMag As Double = 0
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
                    dgvDrillProjects.Rows.Add()
                    dgvDrillProjects.Rows(iRow - 1).Cells(0).Value = sCPID
                    dgvDrillProjects.Rows(iRow - 1).Cells(1).Value = prj.PrimaryKey
                    dgvDrillProjects.Rows(iRow - 1).Cells(2).Value = Val(Trim(CPIDRAC))
                    dgvDrillProjects.Rows(iRow - 1).Cells(3).Value = Val(Trim(prj.RAC))
                    dgvDrillProjects.Rows(iRow - 1).Cells(4).Value = Val(Trim(prj.AvgRAC))
                    'Cumulative Mag:
                    Dim bIsThisWhitelisted As Boolean = False
                    bIsThisWhitelisted = IsInList(prj.PrimaryKey, lstWhitelist, False)
                    Dim IndMag As Double = 0
                    If Not bIsThisWhitelisted Then
                        dgvDrillProjects.Rows(iRow - 1).Cells(2).Style.BackColor = Color.Red
                    End If

                    If bIsThisWhitelisted Then
                        IndMag = Math.Round(((CPIDRAC / (PrjRAC + 0.01)) / (WhitelistedProjects + 0.01)) * NeuralNetworkMultiplier, 2)
                        CumulativeMag += IndMag
                        TotalRAC += CPIDRAC
                        TotalNetworkRAC += PrjRAC
                    End If
                    dgvDrillProjects.Rows(iRow - 1).Cells(5).Value = Val(IndMag)
                    dgvDrillProjects.Rows(iRow - 1).Cells(6).Value = Val(Math.Round(TotalRAC, 2))
                    dgvDrillProjects.Rows(iRow - 1).Cells(7).Value = Val(Math.Round(CumulativeMag, 2))

                End If

            Next

            'Formula for individual drill-in for Magnitude
            'Magnitude = (TotalRACContributions  /  ProjectRAC) / (WhitelistedProjectsCount)) * NeuralNetworkMultiplier

            iRow += 1
            dgvDrillProjects.Rows.Add()
            dgvDrillProjects.Rows(iRow - 1).Cells(0).Value = "Total Mag: " + Trim(RoundedMag(CumulativeMag))
            dgvDrillProjects.Rows(iRow - 1).Cells(3).Value = RoundedMag(TotalNetworkRAC)
            dgvDrillProjects.Rows(iRow - 1).Cells(6).Value = RoundedMag(TotalRAC)
            dgvDrillProjects.Rows(iRow - 1).Cells(7).Value = RoundedMag(CumulativeMag)
            dgvDrillProjects.RowHeadersVisible = True
            dgvDrillProjects.RowHeadersDefaultCellStyle.BackColor = Color.Black

            dgvDrillProjects.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.AllCells

            System.Windows.Forms.Cursor.Current = Cursors.WaitCursor
            Dim oNewForm As New Form
            oNewForm.Width = Screen.PrimaryScreen.WorkingArea.Width / 2
            oNewForm.Height = Screen.PrimaryScreen.WorkingArea.Height / 1.2
            oNewForm.BackColor = Color.Black
            oNewForm.Text = "CPID Magnitude Details - Gridcoin Neural Network - (Red=Blacklisted)"
            oNewForm.Controls.Add(dgvDrillProjects)

            dgvDrillProjects.Left = 5
            dgvDrillProjects.Top = 5
            dgvDrillProjects.AutoResizeColumns()
            dgvDrillProjects.AutoResizeRows()


            Dim TotalControlHeight As Long = (dgvDrillProjects.RowTemplate.Height * (iRow + 2)) + 18
            dgvDrillProjects.Height = TotalControlHeight + 5
            oNewForm.Height = dgvDrillProjects.Height + 285
            dgvDrillProjects.Width = oNewForm.Width - 25

            rtbDrillRAC = New System.Windows.Forms.RichTextBox
            Dim sXML As String = GetRAC(sCPID)
            rtbDrillRAC.Font = New Font("Verdana", 12)
            rtbDrillRAC.Left = 5
            rtbDrillRAC.Top = dgvDrillProjects.Height + 8
            rtbDrillRAC.Height = 245
            rtbDrillRAC.Width = oNewForm.Width - 30
            rtbDrillRAC.Text = sXML
            rtbDrillRAC.BackColor = Color.Black
            rtbDrillRAC.ForeColor = Color.Green
            HighlightKeyword(rtbDrillRAC, Color.Brown, 0, "<project>")

            oNewForm.Controls.Add(rtbDrillRAC)
            oNewForm.Show()
            AddHandler oNewForm.Resize, AddressOf ResizeDrillProjectsForm
            System.Windows.Forms.Cursor.Current = Cursors.Default
        End If
    End Sub

    Private Sub ResizeDrillProjectsForm(sender As Object, e As System.EventArgs)
        Dim TotalControlHeight As Long = (dgvDrillProjects.RowTemplate.Height * (dgvDrillProjects.Rows.Count + 2)) + 18
        dgvDrillProjects.Height = TotalControlHeight + 5
        Dim oSender As Form = sender
        dgvDrillProjects.Width = oSender.Width - 25
        For x = 0 To dgvDrillProjects.ColumnCount - 1
            dgvDrillProjects.Columns(x).Width += 1
        Next
        dgvDrillProjects.BackgroundColor = Color.Black
        dgvDrillProjects.Refresh()
        dgvDrillProjects.Update()

        'oNewForm.Height = dgvDrillProjects.Height + 285
        '        dgvDrillProjects.Refresh()
        '       dgvDrillProjects.Update()
        rtbDrillRAC.Top = dgvDrillProjects.Height + 8
        rtbDrillRAC.Height = oSender.Height - TotalControlHeight

        rtbDrillRAC.Width = oSender.Width - 30
        rtbDrillRAC.Update()


    End Sub

    Private Sub ContractDetailsToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles ContractDetailsToolStripMenuItem.Click
        Dim sData As String = GetMagnitudeContract()
        Dim sMags As String = ExtractXML(sData, "<MAGNITUDES>")
        Dim vCt() As String = Split(sMags, ";")
        Dim sHash As String = GetQuorumHash(sData)
        MsgBox(Mid(sData, 1, 500) + " - Count " + Trim(vCt.Length() - 1) + " - Hash " + sHash)
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


    Private Sub Draw3DBorder(g As Graphics)
        Dim PenWidth As Integer = CInt(Pens.White.Width)

        g.DrawLine(Pens.DarkGray, New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Top), New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Top))
        g.DrawLine(Pens.DarkGray, New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Top), New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Height - PenWidth))
        g.DrawLine(Pens.White, New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Height - PenWidth), New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Height - PenWidth))
        g.DrawLine(Pens.White, New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Top), New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Height - PenWidth))
    End Sub


    Private Sub TextBox1_TextChanged(sender As System.Object, e As System.EventArgs) Handles txtSearch.TextChanged
        Try

            Dim sPhrase As String = txtSearch.Text
            For y = 1 To dgv.Rows.Count - 1
                For x = 0 To dgv.Rows(y).Cells.Count - 1
                    If LCase(Trim("" & dgv.Rows(y).Cells(x).Value)) Like LCase(Trim(txtSearch.Text)) + "*" Then
                        dgv.Rows(y).Selected = True
                        dgv.CurrentCell = dgv.Rows(y).Cells(0)
                        Exit Sub
                    End If
                Next x
            Next y

        Catch ex As Exception
            MsgBox("Slow down.", MsgBoxStyle.Critical)
        End Try

    End Sub

    Private Sub btnRefresh_Click(sender As System.Object, e As System.EventArgs) Handles btnRefresh.Click
        PopulateNeuralData()
        Call OneMinuteUpdate()
        'ChartBoinc()

    End Sub
    Public Sub DoEvents()
        Application.DoEvents()
    End Sub
    Private Sub TimerSync_Tick(sender As System.Object, e As System.EventArgs) Handles TimerSync.Tick
        If mlPercentComplete <> 0 Then
            pbSync.Visible = True
            DisableForm(False)
            pbSync.Maximum = 101
            lblQueue.Text = "Queue: " + Trim(mlQueue) : lblQueue.Visible = True
            If mlPercentComplete <= pbSync.Maximum Then pbSync.Value = mlPercentComplete
            If mlPercentComplete < 50 Then pbSync.ForeColor = Color.Red
            If mlPercentComplete > 50 And mlPercentComplete < 80 Then pbSync.ForeColor = Color.Orange
            If mlPercentComplete > 80 And mlPercentComplete < 90 Then pbSync.ForeColor = Color.Yellow
            If mlPercentComplete > 90 Then pbSync.ForeColor = Color.White : lblQueue.Text = "Queue: Final calculation phase"
            If mlPercentComplete = 1 Then pbSync.ForeColor = Color.Brown : lblQueue.Text = "Data gathering phase" : lblNeuralDetail.Text = msNeuralDetail
            Application.DoEvents()
        Else
            If pbSync.Visible = True Then pbSync.Visible = False : Application.DoEvents()
            pbSync.Visible = False : pbSync.Height = 18
            DisableForm(True) : lblNeuralDetail.Text = ""
            lblQueue.Visible = False
            Application.DoEvents()
            If bNeedsDgvRefreshed Then
                bNeedsDgvRefreshed = False
                PopulateNeuralData()
            End If
        End If

    End Sub
    Private Sub DisableForm(bEnabled As Boolean)
        'Lock the controls, but allow the user to move the screen around so we dont appear Frozen.
        'dgv.Enabled = bEnabled - dont lock the grid
        btnExport.Enabled = bEnabled
        btnRefresh.Enabled = bEnabled
        chtCurCont.Enabled = bEnabled
        btnSync.Enabled = bEnabled
        If bEnabled Then
            Me.BackColor = Color.Black
        Else
            Me.BackColor = Color.Green
        End If

    End Sub
    Private Sub PoolsToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs)

    End Sub

    Private Sub tOneMinute_Tick(sender As System.Object, e As System.EventArgs) Handles tOneMinute.Tick
        Call OneMinuteUpdate()

    End Sub

    Private Sub dgv_CellContentClick(sender As System.Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentClick

    End Sub
    Private Sub Sync()
        mclsUtilization.UpdateMagnitudesOnly()
        bNeedsDgvRefreshed = True
    End Sub
    Private Sub btnSync_Click(sender As System.Object, e As System.EventArgs) Handles btnSync.Click
        btnSync.Enabled = False
        Dim thSync As New Threading.Thread(AddressOf Sync) : thSync.Start() : mdLastNeuralNetworkSync = Now
        Me.BackColor = Color.Green
        For x As Integer = 1 To 3
            Threading.Thread.Sleep(1000)
            Application.DoEvents()
        Next
    End Sub


    Private Sub TabControl1_Click(sender As System.Object, e As System.EventArgs) Handles TabControl1.Click
        If TabControl1.SelectedIndex = 1 Then
            WebBrowserBoinc.Navigate("https://gridcoin.us")
        End If

        ' If TabControl1.SelectedIndex = 2 Then
        '  WebBrowserChat.Navigate("https://kiwiirc.com/client/irc.freenode.net:+7000/#gridcoin")
        ' End If
    End Sub

    Private Sub LinkLabel1_LinkClicked(sender As System.Object, e As System.Windows.Forms.LinkLabelLinkClickedEventArgs)
        Dim sURL As String = "https://kiwiirc.com/client/irc.freenode.net:+7000/#gridcoin"
        Process.Start(sURL)

    End Sub

    Private Sub dgvProjects_CellContentDoubleClick(sender As System.Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgvProjects.CellContentDoubleClick

        'Drill into CPID
        If e.RowIndex < 0 Then Exit Sub
        'Get whitelist total first

        Dim sProject As String = Trim(dgvProjects.Rows(e.RowIndex).Cells(0).Value)

        If Len(sProject) > 1 Then
            Dim fRain As New frmRain
            fRain.msProject = sProject
            fRain.Show()

        End If

    End Sub
    Private Function GetAgeOfContract() As Double

        Return GetWindowsFileAge(GetGridFolder() + "NeuralNetwork\contract.dat")

    End Function
    Private Sub Button1_Click(sender As System.Object, e As System.EventArgs)
        'Export Contract 
        Dim sFullPath As String = GetGridFolder() + "NeuralNetwork\contract.dat"
        Dim swContract As New StreamWriter(sFullPath)
        Dim sContract As String = GetMagnitudeContract()
        swContract.Write(sContract)
        swContract.Close()
    End Sub
End Class
