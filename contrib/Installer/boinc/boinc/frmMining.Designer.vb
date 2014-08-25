<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmMining
    Inherits System.Windows.Forms.Form

    'Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()> _
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
        Finally
            MyBase.Dispose(disposing)
        End Try
    End Sub

    'Required by the Windows Form Designer
    Private components As System.ComponentModel.IContainer

    'NOTE: The following procedure is required by the Windows Form Designer
    'It can be modified using the Windows Form Designer.  
    'Do not modify it using the code editor.
    <System.Diagnostics.DebuggerStepThrough()> _
    Private Sub InitializeComponent()
        Me.components = New System.ComponentModel.Container()
        Dim ChartArea1 As System.Windows.Forms.DataVisualization.Charting.ChartArea = New System.Windows.Forms.DataVisualization.Charting.ChartArea()
        Dim Legend1 As System.Windows.Forms.DataVisualization.Charting.Legend = New System.Windows.Forms.DataVisualization.Charting.Legend()
        Dim Series1 As System.Windows.Forms.DataVisualization.Charting.Series = New System.Windows.Forms.DataVisualization.Charting.Series()
        Dim ChartArea2 As System.Windows.Forms.DataVisualization.Charting.ChartArea = New System.Windows.Forms.DataVisualization.Charting.ChartArea()
        Dim Legend2 As System.Windows.Forms.DataVisualization.Charting.Legend = New System.Windows.Forms.DataVisualization.Charting.Legend()
        Dim Series2 As System.Windows.Forms.DataVisualization.Charting.Series = New System.Windows.Forms.DataVisualization.Charting.Series()
        Dim ChartArea3 As System.Windows.Forms.DataVisualization.Charting.ChartArea = New System.Windows.Forms.DataVisualization.Charting.ChartArea()
        Dim Series3 As System.Windows.Forms.DataVisualization.Charting.Series = New System.Windows.Forms.DataVisualization.Charting.Series()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmMining))
        Me.btnRefresh = New System.Windows.Forms.Button()
        Me.Chart1 = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.ChartUtilization = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.lblTxtVersion = New System.Windows.Forms.Label()
        Me.lbltxtAvgCredits = New System.Windows.Forms.Label()
        Me.lblThanks = New System.Windows.Forms.Label()
        Me.lblAvgCredits = New System.Windows.Forms.Label()
        Me.lblVersion = New System.Windows.Forms.Label()
        Me.lblMD5 = New System.Windows.Forms.Label()
        Me.Label2 = New System.Windows.Forms.Label()
        Me.tOneMinute = New System.Windows.Forms.Timer(Me.components)
        Me.btnRestartMiner = New System.Windows.Forms.Button()
        Me.lblRestartMiner = New System.Windows.Forms.Label()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.lblRestartWallet = New System.Windows.Forms.Label()
        Me.Label4 = New System.Windows.Forms.Label()
        Me.btnClose = New System.Windows.Forms.Button()
        Me.btnHide = New System.Windows.Forms.Button()
        Me.timerBoincBlock = New System.Windows.Forms.Timer(Me.components)
        Me.lblCPUMinerElapsed = New System.Windows.Forms.Label()
        Me.Label5 = New System.Windows.Forms.Label()
        Me.lblAccepted = New System.Windows.Forms.Label()
        Me.Label7 = New System.Windows.Forms.Label()
        Me.lblGPUMhs = New System.Windows.Forms.Label()
        Me.Label9 = New System.Windows.Forms.Label()
        Me.lblStale = New System.Windows.Forms.Label()
        Me.Label11 = New System.Windows.Forms.Label()
        Me.lblInvalid = New System.Windows.Forms.Label()
        Me.Label13 = New System.Windows.Forms.Label()
        Me.timerReaper = New System.Windows.Forms.Timer(Me.components)
        Me.MenuStrip1 = New System.Windows.Forms.MenuStrip()
        Me.FileToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.HideToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.ConfigurationToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.PoolsToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.chkFullSpeed = New System.Windows.Forms.CheckBox()
        Me.chkMiningEnabled = New System.Windows.Forms.CheckBox()
        Me.txtGPU0 = New System.Windows.Forms.Label()
        Me.txtGPU1 = New System.Windows.Forms.Label()
        Me.txtGPU2 = New System.Windows.Forms.Label()
        Me.txtGPU3 = New System.Windows.Forms.Label()
        Me.txtGPU4 = New System.Windows.Forms.Label()
        Me.TabControl1 = New System.Windows.Forms.TabControl()
        Me.TabCGMINER = New System.Windows.Forms.TabPage()
        Me.GroupBox4 = New System.Windows.Forms.GroupBox()
        Me.GroupBox3 = New System.Windows.Forms.GroupBox()
        Me.RichTextBox1 = New System.Windows.Forms.RichTextBox()
        Me.GroupBox2 = New System.Windows.Forms.GroupBox()
        Me.ChartHashRate = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.GroupBox1 = New System.Windows.Forms.GroupBox()
        Me.chkCGMonitor = New System.Windows.Forms.CheckBox()
        Me.lblCGMessage = New System.Windows.Forms.Label()
        Me.Label1 = New System.Windows.Forms.Label()
        Me.cmbSelectedGPU = New System.Windows.Forms.ComboBox()
        Me.btnHideCgminer = New System.Windows.Forms.Button()
        Me.btnShowCgminer = New System.Windows.Forms.Button()
        Me.Label16 = New System.Windows.Forms.Label()
        Me.Label15 = New System.Windows.Forms.Label()
        Me.Label14 = New System.Windows.Forms.Label()
        Me.Label12 = New System.Windows.Forms.Label()
        Me.Label8 = New System.Windows.Forms.Label()
        Me.pbCgminer = New System.Windows.Forms.PictureBox()
        Me.TabGuiMiner = New System.Windows.Forms.TabPage()
        Me.Pb1 = New System.Windows.Forms.PictureBox()
        Me.TabPage1 = New System.Windows.Forms.TabPage()
        Me.BoincWebBrowser = New System.Windows.Forms.WebBrowser()
        Me.TimerCGMonitor = New System.Windows.Forms.Timer(Me.components)
        Me.lblWarning = New System.Windows.Forms.Label()
        Me.msBlockHeight = New System.Windows.Forms.Label()
        Me.Label18 = New System.Windows.Forms.Label()
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.MenuStrip1.SuspendLayout()
        Me.TabControl1.SuspendLayout()
        Me.TabCGMINER.SuspendLayout()
        Me.GroupBox3.SuspendLayout()
        Me.GroupBox2.SuspendLayout()
        CType(Me.ChartHashRate, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.GroupBox1.SuspendLayout()
        CType(Me.pbCgminer, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.TabGuiMiner.SuspendLayout()
        CType(Me.Pb1, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.TabPage1.SuspendLayout()
        Me.SuspendLayout()
        '
        'btnRefresh
        '
        Me.btnRefresh.Location = New System.Drawing.Point(12, 734)
        Me.btnRefresh.Name = "btnRefresh"
        Me.btnRefresh.Size = New System.Drawing.Size(118, 35)
        Me.btnRefresh.TabIndex = 0
        Me.btnRefresh.Text = "Refresh"
        Me.btnRefresh.UseVisualStyleBackColor = False
        '
        'Chart1
        '
        Me.Chart1.BackColor = System.Drawing.Color.Transparent
        Me.Chart1.BackImageTransparentColor = System.Drawing.Color.Transparent
        Me.Chart1.BackSecondaryColor = System.Drawing.Color.Transparent
        Me.Chart1.BorderlineColor = System.Drawing.Color.DimGray
        Me.Chart1.BorderSkin.BorderColor = System.Drawing.Color.DimGray
        ChartArea1.AxisX.TitleForeColor = System.Drawing.Color.Lime
        ChartArea1.AxisX2.TitleForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        ChartArea1.AxisY.LineColor = System.Drawing.Color.DimGray
        ChartArea1.AxisY.TitleForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        ChartArea1.AxisY2.TitleForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        ChartArea1.BackColor = System.Drawing.Color.Black
        ChartArea1.BackGradientStyle = System.Windows.Forms.DataVisualization.Charting.GradientStyle.LeftRight
        ChartArea1.Name = "ChartArea1"
        Me.Chart1.ChartAreas.Add(ChartArea1)
        Legend1.BackColor = System.Drawing.Color.Transparent
        Legend1.BackSecondaryColor = System.Drawing.Color.FromArgb(CType(CType(224, Byte), Integer), CType(CType(224, Byte), Integer), CType(CType(224, Byte), Integer))
        Legend1.BorderColor = System.Drawing.Color.FromArgb(CType(CType(192, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(192, Byte), Integer))
        Legend1.BorderDashStyle = System.Windows.Forms.DataVisualization.Charting.ChartDashStyle.NotSet
        Legend1.BorderWidth = 0
        Legend1.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        Legend1.Name = "Legend1"
        Legend1.TitleForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Legend1.TitleSeparatorColor = System.Drawing.Color.Lime
        Me.Chart1.Legends.Add(Legend1)
        Me.Chart1.Location = New System.Drawing.Point(11, 426)
        Me.Chart1.Name = "Chart1"
        Me.Chart1.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Fire
        Series1.BackImageTransparentColor = System.Drawing.Color.Transparent
        Series1.BackSecondaryColor = System.Drawing.Color.Transparent
        Series1.ChartArea = "ChartArea1"
        Series1.LabelBackColor = System.Drawing.Color.Transparent
        Series1.LabelBorderColor = System.Drawing.Color.Transparent
        Series1.LabelForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Series1.Legend = "Legend1"
        Series1.Name = "Series1"
        Series1.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Bright
        Me.Chart1.Series.Add(Series1)
        Me.Chart1.Size = New System.Drawing.Size(917, 128)
        Me.Chart1.SuppressExceptions = True
        Me.Chart1.TabIndex = 2
        Me.Chart1.Text = "Boinc Utilization"
        '
        'ChartUtilization
        '
        Me.ChartUtilization.BackColor = System.Drawing.Color.Transparent
        Me.ChartUtilization.BackImageTransparentColor = System.Drawing.Color.Black
        Me.ChartUtilization.BackSecondaryColor = System.Drawing.Color.Black
        Me.ChartUtilization.BorderlineColor = System.Drawing.Color.Black
        ChartArea2.Area3DStyle.Enable3D = True
        ChartArea2.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        ChartArea2.Name = "ChartArea1"
        Me.ChartUtilization.ChartAreas.Add(ChartArea2)
        Legend2.BackColor = System.Drawing.Color.Transparent
        Legend2.Name = "Legend1"
        Me.ChartUtilization.Legends.Add(Legend2)
        Me.ChartUtilization.Location = New System.Drawing.Point(11, 560)
        Me.ChartUtilization.Name = "ChartUtilization"
        Me.ChartUtilization.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Bright
        Series2.ChartArea = "ChartArea1"
        Series2.Legend = "Legend1"
        Series2.Name = "Series1"
        Me.ChartUtilization.Series.Add(Series2)
        Me.ChartUtilization.Size = New System.Drawing.Size(227, 148)
        Me.ChartUtilization.SuppressExceptions = True
        Me.ChartUtilization.TabIndex = 3
        Me.ChartUtilization.Text = "Chart2"
        '
        'lblTxtVersion
        '
        Me.lblTxtVersion.AutoSize = True
        Me.lblTxtVersion.BackColor = System.Drawing.Color.Transparent
        Me.lblTxtVersion.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblTxtVersion.Location = New System.Drawing.Point(286, 608)
        Me.lblTxtVersion.Name = "lblTxtVersion"
        Me.lblTxtVersion.Size = New System.Drawing.Size(57, 16)
        Me.lblTxtVersion.TabIndex = 6
        Me.lblTxtVersion.Text = "Version:"
        '
        'lbltxtAvgCredits
        '
        Me.lbltxtAvgCredits.AutoSize = True
        Me.lbltxtAvgCredits.BackColor = System.Drawing.Color.Transparent
        Me.lbltxtAvgCredits.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!)
        Me.lbltxtAvgCredits.Location = New System.Drawing.Point(506, 608)
        Me.lbltxtAvgCredits.Name = "lbltxtAvgCredits"
        Me.lbltxtAvgCredits.Size = New System.Drawing.Size(151, 16)
        Me.lbltxtAvgCredits.TabIndex = 7
        Me.lbltxtAvgCredits.Text = "Boinc Avg Daily Credits:"
        '
        'lblThanks
        '
        Me.lblThanks.AutoSize = True
        Me.lblThanks.BackColor = System.Drawing.Color.Transparent
        Me.lblThanks.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblThanks.Location = New System.Drawing.Point(8, 772)
        Me.lblThanks.Name = "lblThanks"
        Me.lblThanks.Size = New System.Drawing.Size(10, 13)
        Me.lblThanks.TabIndex = 8
        Me.lblThanks.Text = "."
        '
        'lblAvgCredits
        '
        Me.lblAvgCredits.AutoSize = True
        Me.lblAvgCredits.BackColor = System.Drawing.Color.Transparent
        Me.lblAvgCredits.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!)
        Me.lblAvgCredits.Location = New System.Drawing.Point(663, 601)
        Me.lblAvgCredits.Name = "lblAvgCredits"
        Me.lblAvgCredits.Size = New System.Drawing.Size(24, 25)
        Me.lblAvgCredits.TabIndex = 11
        Me.lblAvgCredits.Text = "0"
        '
        'lblVersion
        '
        Me.lblVersion.AutoSize = True
        Me.lblVersion.BackColor = System.Drawing.Color.Transparent
        Me.lblVersion.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblVersion.Location = New System.Drawing.Point(431, 608)
        Me.lblVersion.Name = "lblVersion"
        Me.lblVersion.Size = New System.Drawing.Size(15, 16)
        Me.lblVersion.TabIndex = 12
        Me.lblVersion.Text = "0"
        '
        'lblMD5
        '
        Me.lblMD5.AutoSize = True
        Me.lblMD5.BackColor = System.Drawing.Color.Transparent
        Me.lblMD5.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblMD5.Location = New System.Drawing.Point(665, 626)
        Me.lblMD5.Name = "lblMD5"
        Me.lblMD5.Size = New System.Drawing.Size(15, 16)
        Me.lblMD5.TabIndex = 14
        Me.lblMD5.Text = "0"
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.BackColor = System.Drawing.Color.Transparent
        Me.Label2.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label2.Location = New System.Drawing.Point(506, 626)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(76, 16)
        Me.Label2.TabIndex = 13
        Me.Label2.Text = "Boinc MD5:"
        '
        'tOneMinute
        '
        Me.tOneMinute.Enabled = True
        Me.tOneMinute.Interval = 60000
        '
        'btnRestartMiner
        '
        Me.btnRestartMiner.Location = New System.Drawing.Point(136, 734)
        Me.btnRestartMiner.Name = "btnRestartMiner"
        Me.btnRestartMiner.Size = New System.Drawing.Size(118, 35)
        Me.btnRestartMiner.TabIndex = 15
        Me.btnRestartMiner.Text = "Restart Miner"
        Me.btnRestartMiner.UseVisualStyleBackColor = False
        '
        'lblRestartMiner
        '
        Me.lblRestartMiner.AutoSize = True
        Me.lblRestartMiner.BackColor = System.Drawing.Color.Transparent
        Me.lblRestartMiner.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblRestartMiner.Location = New System.Drawing.Point(431, 624)
        Me.lblRestartMiner.Name = "lblRestartMiner"
        Me.lblRestartMiner.Size = New System.Drawing.Size(15, 16)
        Me.lblRestartMiner.TabIndex = 17
        Me.lblRestartMiner.Text = "0"
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.BackColor = System.Drawing.Color.Transparent
        Me.Label3.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label3.Location = New System.Drawing.Point(286, 626)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(134, 16)
        Me.Label3.TabIndex = 16
        Me.Label3.Text = "Restart Miner In Mins:"
        '
        'lblRestartWallet
        '
        Me.lblRestartWallet.AutoSize = True
        Me.lblRestartWallet.BackColor = System.Drawing.Color.Transparent
        Me.lblRestartWallet.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblRestartWallet.Location = New System.Drawing.Point(431, 642)
        Me.lblRestartWallet.Name = "lblRestartWallet"
        Me.lblRestartWallet.Size = New System.Drawing.Size(15, 16)
        Me.lblRestartWallet.TabIndex = 21
        Me.lblRestartWallet.Text = "0"
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.BackColor = System.Drawing.Color.Transparent
        Me.Label4.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label4.Location = New System.Drawing.Point(286, 642)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(139, 16)
        Me.Label4.TabIndex = 20
        Me.Label4.Text = "Restart Wallet In Mins:"
        '
        'btnClose
        '
        Me.btnClose.Location = New System.Drawing.Point(260, 734)
        Me.btnClose.Name = "btnClose"
        Me.btnClose.Size = New System.Drawing.Size(118, 35)
        Me.btnClose.TabIndex = 22
        Me.btnClose.Text = "Close Miner"
        Me.btnClose.UseVisualStyleBackColor = False
        '
        'btnHide
        '
        Me.btnHide.Location = New System.Drawing.Point(384, 734)
        Me.btnHide.Name = "btnHide"
        Me.btnHide.Size = New System.Drawing.Size(118, 35)
        Me.btnHide.TabIndex = 23
        Me.btnHide.Text = "Hide"
        Me.btnHide.UseVisualStyleBackColor = False
        '
        'timerBoincBlock
        '
        Me.timerBoincBlock.Enabled = True
        Me.timerBoincBlock.Interval = 6000
        '
        'lblCPUMinerElapsed
        '
        Me.lblCPUMinerElapsed.AutoSize = True
        Me.lblCPUMinerElapsed.BackColor = System.Drawing.Color.Transparent
        Me.lblCPUMinerElapsed.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblCPUMinerElapsed.Location = New System.Drawing.Point(665, 642)
        Me.lblCPUMinerElapsed.Name = "lblCPUMinerElapsed"
        Me.lblCPUMinerElapsed.Size = New System.Drawing.Size(15, 16)
        Me.lblCPUMinerElapsed.TabIndex = 30
        Me.lblCPUMinerElapsed.Text = "0"
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.BackColor = System.Drawing.Color.Transparent
        Me.Label5.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label5.Location = New System.Drawing.Point(506, 642)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(85, 16)
        Me.Label5.TabIndex = 29
        Me.Label5.Text = "Boinc KH/ps:"
        '
        'lblAccepted
        '
        Me.lblAccepted.AutoSize = True
        Me.lblAccepted.BackColor = System.Drawing.Color.Transparent
        Me.lblAccepted.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblAccepted.Location = New System.Drawing.Point(128, 50)
        Me.lblAccepted.Name = "lblAccepted"
        Me.lblAccepted.Size = New System.Drawing.Size(15, 16)
        Me.lblAccepted.TabIndex = 37
        Me.lblAccepted.Text = "0"
        '
        'Label7
        '
        Me.Label7.AutoSize = True
        Me.Label7.BackColor = System.Drawing.Color.Transparent
        Me.Label7.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label7.Location = New System.Drawing.Point(29, 50)
        Me.Label7.Name = "Label7"
        Me.Label7.Size = New System.Drawing.Size(69, 16)
        Me.Label7.TabIndex = 36
        Me.Label7.Text = "Accepted:"
        '
        'lblGPUMhs
        '
        Me.lblGPUMhs.AutoSize = True
        Me.lblGPUMhs.BackColor = System.Drawing.Color.Transparent
        Me.lblGPUMhs.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblGPUMhs.Location = New System.Drawing.Point(128, 31)
        Me.lblGPUMhs.Name = "lblGPUMhs"
        Me.lblGPUMhs.Size = New System.Drawing.Size(15, 16)
        Me.lblGPUMhs.TabIndex = 35
        Me.lblGPUMhs.Text = "0"
        '
        'Label9
        '
        Me.Label9.AutoSize = True
        Me.Label9.BackColor = System.Drawing.Color.Transparent
        Me.Label9.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label9.Location = New System.Drawing.Point(29, 31)
        Me.Label9.Name = "Label9"
        Me.Label9.Size = New System.Drawing.Size(75, 16)
        Me.Label9.TabIndex = 34
        Me.Label9.Text = "GPU MH/s:"
        '
        'lblStale
        '
        Me.lblStale.AutoSize = True
        Me.lblStale.BackColor = System.Drawing.Color.Transparent
        Me.lblStale.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblStale.Location = New System.Drawing.Point(128, 69)
        Me.lblStale.Name = "lblStale"
        Me.lblStale.Size = New System.Drawing.Size(15, 16)
        Me.lblStale.TabIndex = 39
        Me.lblStale.Text = "0"
        '
        'Label11
        '
        Me.Label11.AutoSize = True
        Me.Label11.BackColor = System.Drawing.Color.Transparent
        Me.Label11.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label11.Location = New System.Drawing.Point(29, 69)
        Me.Label11.Name = "Label11"
        Me.Label11.Size = New System.Drawing.Size(42, 16)
        Me.Label11.TabIndex = 38
        Me.Label11.Text = "Stale:"
        '
        'lblInvalid
        '
        Me.lblInvalid.AutoSize = True
        Me.lblInvalid.BackColor = System.Drawing.Color.Transparent
        Me.lblInvalid.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblInvalid.Location = New System.Drawing.Point(128, 89)
        Me.lblInvalid.Name = "lblInvalid"
        Me.lblInvalid.Size = New System.Drawing.Size(15, 16)
        Me.lblInvalid.TabIndex = 41
        Me.lblInvalid.Text = "0"
        '
        'Label13
        '
        Me.Label13.AutoSize = True
        Me.Label13.BackColor = System.Drawing.Color.Transparent
        Me.Label13.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label13.Location = New System.Drawing.Point(29, 89)
        Me.Label13.Name = "Label13"
        Me.Label13.Size = New System.Drawing.Size(50, 16)
        Me.Label13.TabIndex = 40
        Me.Label13.Text = "Invalid:"
        '
        'timerReaper
        '
        Me.timerReaper.Enabled = True
        Me.timerReaper.Interval = 11111
        '
        'MenuStrip1
        '
        Me.MenuStrip1.AllowItemReorder = True
        Me.MenuStrip1.BackColor = System.Drawing.Color.Transparent
        Me.MenuStrip1.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None
        Me.MenuStrip1.Items.AddRange(New System.Windows.Forms.ToolStripItem() {Me.FileToolStripMenuItem, Me.ConfigurationToolStripMenuItem, Me.PoolsToolStripMenuItem})
        Me.MenuStrip1.Location = New System.Drawing.Point(0, 0)
        Me.MenuStrip1.Name = "MenuStrip1"
        Me.MenuStrip1.Size = New System.Drawing.Size(940, 24)
        Me.MenuStrip1.TabIndex = 42
        Me.MenuStrip1.Text = "MenuStrip1"
        '
        'FileToolStripMenuItem
        '
        Me.FileToolStripMenuItem.BackColor = System.Drawing.Color.Transparent
        Me.FileToolStripMenuItem.DropDownItems.AddRange(New System.Windows.Forms.ToolStripItem() {Me.HideToolStripMenuItem})
        Me.FileToolStripMenuItem.Name = "FileToolStripMenuItem"
        Me.FileToolStripMenuItem.Size = New System.Drawing.Size(37, 20)
        Me.FileToolStripMenuItem.Text = "File"
        '
        'HideToolStripMenuItem
        '
        Me.HideToolStripMenuItem.BackColor = System.Drawing.Color.Transparent
        Me.HideToolStripMenuItem.ForeColor = System.Drawing.Color.Lime
        Me.HideToolStripMenuItem.Name = "HideToolStripMenuItem"
        Me.HideToolStripMenuItem.Size = New System.Drawing.Size(99, 22)
        Me.HideToolStripMenuItem.Text = "Hide"
        '
        'ConfigurationToolStripMenuItem
        '
        Me.ConfigurationToolStripMenuItem.BackColor = System.Drawing.Color.Transparent
        Me.ConfigurationToolStripMenuItem.ImageTransparentColor = System.Drawing.Color.Black
        Me.ConfigurationToolStripMenuItem.Name = "ConfigurationToolStripMenuItem"
        Me.ConfigurationToolStripMenuItem.Size = New System.Drawing.Size(93, 20)
        Me.ConfigurationToolStripMenuItem.Text = "Configuration"
        '
        'PoolsToolStripMenuItem
        '
        Me.PoolsToolStripMenuItem.Name = "PoolsToolStripMenuItem"
        Me.PoolsToolStripMenuItem.Size = New System.Drawing.Size(48, 20)
        Me.PoolsToolStripMenuItem.Text = "Pools"
        '
        'chkFullSpeed
        '
        Me.chkFullSpeed.AutoSize = True
        Me.chkFullSpeed.BackColor = System.Drawing.Color.Transparent
        Me.chkFullSpeed.Checked = True
        Me.chkFullSpeed.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkFullSpeed.Location = New System.Drawing.Point(154, 136)
        Me.chkFullSpeed.Name = "chkFullSpeed"
        Me.chkFullSpeed.Size = New System.Drawing.Size(76, 17)
        Me.chkFullSpeed.TabIndex = 45
        Me.chkFullSpeed.Text = "Full Speed"
        Me.chkFullSpeed.UseVisualStyleBackColor = False
        '
        'chkMiningEnabled
        '
        Me.chkMiningEnabled.AutoSize = True
        Me.chkMiningEnabled.BackColor = System.Drawing.Color.Transparent
        Me.chkMiningEnabled.Checked = True
        Me.chkMiningEnabled.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkMiningEnabled.Location = New System.Drawing.Point(32, 136)
        Me.chkMiningEnabled.Name = "chkMiningEnabled"
        Me.chkMiningEnabled.Size = New System.Drawing.Size(125, 17)
        Me.chkMiningEnabled.TabIndex = 46
        Me.chkMiningEnabled.Text = "GPU Mining Enabled"
        Me.chkMiningEnabled.UseVisualStyleBackColor = False
        '
        'txtGPU0
        '
        Me.txtGPU0.AutoSize = True
        Me.txtGPU0.BackColor = System.Drawing.Color.Transparent
        Me.txtGPU0.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtGPU0.Location = New System.Drawing.Point(262, 31)
        Me.txtGPU0.Name = "txtGPU0"
        Me.txtGPU0.Size = New System.Drawing.Size(15, 16)
        Me.txtGPU0.TabIndex = 48
        Me.txtGPU0.Text = "0"
        '
        'txtGPU1
        '
        Me.txtGPU1.AutoSize = True
        Me.txtGPU1.BackColor = System.Drawing.Color.Transparent
        Me.txtGPU1.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtGPU1.Location = New System.Drawing.Point(263, 50)
        Me.txtGPU1.Name = "txtGPU1"
        Me.txtGPU1.Size = New System.Drawing.Size(15, 16)
        Me.txtGPU1.TabIndex = 49
        Me.txtGPU1.Text = "0"
        '
        'txtGPU2
        '
        Me.txtGPU2.AutoSize = True
        Me.txtGPU2.BackColor = System.Drawing.Color.Transparent
        Me.txtGPU2.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtGPU2.Location = New System.Drawing.Point(263, 69)
        Me.txtGPU2.Name = "txtGPU2"
        Me.txtGPU2.Size = New System.Drawing.Size(15, 16)
        Me.txtGPU2.TabIndex = 50
        Me.txtGPU2.Text = "0"
        '
        'txtGPU3
        '
        Me.txtGPU3.AutoSize = True
        Me.txtGPU3.BackColor = System.Drawing.Color.Transparent
        Me.txtGPU3.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtGPU3.Location = New System.Drawing.Point(263, 89)
        Me.txtGPU3.Name = "txtGPU3"
        Me.txtGPU3.Size = New System.Drawing.Size(15, 16)
        Me.txtGPU3.TabIndex = 51
        Me.txtGPU3.Text = "0"
        '
        'txtGPU4
        '
        Me.txtGPU4.AutoSize = True
        Me.txtGPU4.BackColor = System.Drawing.Color.Transparent
        Me.txtGPU4.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtGPU4.Location = New System.Drawing.Point(263, 111)
        Me.txtGPU4.Name = "txtGPU4"
        Me.txtGPU4.Size = New System.Drawing.Size(15, 16)
        Me.txtGPU4.TabIndex = 52
        Me.txtGPU4.Text = "0"
        '
        'TabControl1
        '
        Me.TabControl1.Controls.Add(Me.TabCGMINER)
        Me.TabControl1.Controls.Add(Me.TabGuiMiner)
        Me.TabControl1.Controls.Add(Me.TabPage1)
        Me.TabControl1.Location = New System.Drawing.Point(14, 39)
        Me.TabControl1.Name = "TabControl1"
        Me.TabControl1.SelectedIndex = 0
        Me.TabControl1.Size = New System.Drawing.Size(913, 383)
        Me.TabControl1.TabIndex = 53
        '
        'TabCGMINER
        '
        Me.TabCGMINER.Controls.Add(Me.GroupBox4)
        Me.TabCGMINER.Controls.Add(Me.GroupBox3)
        Me.TabCGMINER.Controls.Add(Me.GroupBox2)
        Me.TabCGMINER.Controls.Add(Me.GroupBox1)
        Me.TabCGMINER.Controls.Add(Me.pbCgminer)
        Me.TabCGMINER.Location = New System.Drawing.Point(4, 22)
        Me.TabCGMINER.Name = "TabCGMINER"
        Me.TabCGMINER.Padding = New System.Windows.Forms.Padding(3)
        Me.TabCGMINER.Size = New System.Drawing.Size(905, 357)
        Me.TabCGMINER.TabIndex = 0
        Me.TabCGMINER.Text = "Cg Miner API"
        Me.TabCGMINER.UseVisualStyleBackColor = True
        '
        'GroupBox4
        '
        Me.GroupBox4.BackgroundImage = Global.BoincStake.My.Resources.Resources.gradient
        Me.GroupBox4.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.GroupBox4.Location = New System.Drawing.Point(640, 6)
        Me.GroupBox4.Name = "GroupBox4"
        Me.GroupBox4.Size = New System.Drawing.Size(260, 214)
        Me.GroupBox4.TabIndex = 57
        Me.GroupBox4.TabStop = False
        Me.GroupBox4.Text = "Network Status:"
        '
        'GroupBox3
        '
        Me.GroupBox3.BackColor = System.Drawing.Color.Transparent
        Me.GroupBox3.BackgroundImage = Global.BoincStake.My.Resources.Resources.gradient753
        Me.GroupBox3.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch
        Me.GroupBox3.Controls.Add(Me.RichTextBox1)
        Me.GroupBox3.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.GroupBox3.ForeColor = System.Drawing.Color.Green
        Me.GroupBox3.Location = New System.Drawing.Point(6, 226)
        Me.GroupBox3.Name = "GroupBox3"
        Me.GroupBox3.Size = New System.Drawing.Size(893, 125)
        Me.GroupBox3.TabIndex = 55
        Me.GroupBox3.TabStop = False
        Me.GroupBox3.Text = "API Commands:"
        '
        'RichTextBox1
        '
        Me.RichTextBox1.BackColor = System.Drawing.Color.DimGray
        Me.RichTextBox1.ForeColor = System.Drawing.Color.Lime
        Me.RichTextBox1.Location = New System.Drawing.Point(7, 19)
        Me.RichTextBox1.Name = "RichTextBox1"
        Me.RichTextBox1.Size = New System.Drawing.Size(880, 100)
        Me.RichTextBox1.TabIndex = 0
        Me.RichTextBox1.Text = ""
        '
        'GroupBox2
        '
        Me.GroupBox2.BackgroundImage = Global.BoincStake.My.Resources.Resources.gradient
        Me.GroupBox2.Controls.Add(Me.ChartHashRate)
        Me.GroupBox2.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.GroupBox2.Location = New System.Drawing.Point(344, 6)
        Me.GroupBox2.Name = "GroupBox2"
        Me.GroupBox2.Size = New System.Drawing.Size(290, 214)
        Me.GroupBox2.TabIndex = 54
        Me.GroupBox2.TabStop = False
        Me.GroupBox2.Text = "Performance:"
        '
        'ChartHashRate
        '
        Me.ChartHashRate.BackColor = System.Drawing.Color.Transparent
        Me.ChartHashRate.BackImageTransparentColor = System.Drawing.Color.Black
        Me.ChartHashRate.BackSecondaryColor = System.Drawing.Color.Black
        Me.ChartHashRate.BorderlineColor = System.Drawing.Color.Black
        ChartArea3.Area3DStyle.Enable3D = True
        ChartArea3.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        ChartArea3.Name = "ChartArea1"
        Me.ChartHashRate.ChartAreas.Add(ChartArea3)
        Me.ChartHashRate.Location = New System.Drawing.Point(27, 45)
        Me.ChartHashRate.Name = "ChartHashRate"
        Me.ChartHashRate.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Fire
        Series3.ChartArea = "ChartArea1"
        Series3.Name = "Series1"
        Me.ChartHashRate.Series.Add(Series3)
        Me.ChartHashRate.Size = New System.Drawing.Size(231, 122)
        Me.ChartHashRate.SuppressExceptions = True
        Me.ChartHashRate.TabIndex = 45
        Me.ChartHashRate.Text = "Chart2"
        '
        'GroupBox1
        '
        Me.GroupBox1.BackgroundImage = Global.BoincStake.My.Resources.Resources.gradient
        Me.GroupBox1.Controls.Add(Me.chkCGMonitor)
        Me.GroupBox1.Controls.Add(Me.lblCGMessage)
        Me.GroupBox1.Controls.Add(Me.Label1)
        Me.GroupBox1.Controls.Add(Me.cmbSelectedGPU)
        Me.GroupBox1.Controls.Add(Me.btnHideCgminer)
        Me.GroupBox1.Controls.Add(Me.btnShowCgminer)
        Me.GroupBox1.Controls.Add(Me.Label16)
        Me.GroupBox1.Controls.Add(Me.chkFullSpeed)
        Me.GroupBox1.Controls.Add(Me.chkMiningEnabled)
        Me.GroupBox1.Controls.Add(Me.Label15)
        Me.GroupBox1.Controls.Add(Me.Label14)
        Me.GroupBox1.Controls.Add(Me.Label12)
        Me.GroupBox1.Controls.Add(Me.Label8)
        Me.GroupBox1.Controls.Add(Me.Label9)
        Me.GroupBox1.Controls.Add(Me.txtGPU4)
        Me.GroupBox1.Controls.Add(Me.Label11)
        Me.GroupBox1.Controls.Add(Me.txtGPU3)
        Me.GroupBox1.Controls.Add(Me.Label13)
        Me.GroupBox1.Controls.Add(Me.txtGPU2)
        Me.GroupBox1.Controls.Add(Me.Label7)
        Me.GroupBox1.Controls.Add(Me.txtGPU1)
        Me.GroupBox1.Controls.Add(Me.lblGPUMhs)
        Me.GroupBox1.Controls.Add(Me.lblStale)
        Me.GroupBox1.Controls.Add(Me.txtGPU0)
        Me.GroupBox1.Controls.Add(Me.lblAccepted)
        Me.GroupBox1.Controls.Add(Me.lblInvalid)
        Me.GroupBox1.ForeColor = System.Drawing.Color.Lime
        Me.GroupBox1.Location = New System.Drawing.Point(6, 6)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(332, 214)
        Me.GroupBox1.TabIndex = 53
        Me.GroupBox1.TabStop = False
        Me.GroupBox1.Text = "CG Miner Statistics:"
        '
        'chkCGMonitor
        '
        Me.chkCGMonitor.AutoSize = True
        Me.chkCGMonitor.BackColor = System.Drawing.Color.Transparent
        Me.chkCGMonitor.Checked = True
        Me.chkCGMonitor.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkCGMonitor.Location = New System.Drawing.Point(236, 136)
        Me.chkCGMonitor.Name = "chkCGMonitor"
        Me.chkCGMonitor.Size = New System.Drawing.Size(90, 17)
        Me.chkCGMonitor.TabIndex = 64
        Me.chkCGMonitor.Text = "Monitor Miner"
        Me.chkCGMonitor.UseVisualStyleBackColor = False
        '
        'lblCGMessage
        '
        Me.lblCGMessage.AutoSize = True
        Me.lblCGMessage.BackColor = System.Drawing.Color.Transparent
        Me.lblCGMessage.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblCGMessage.Location = New System.Drawing.Point(29, 111)
        Me.lblCGMessage.Name = "lblCGMessage"
        Me.lblCGMessage.Size = New System.Drawing.Size(11, 16)
        Me.lblCGMessage.TabIndex = 63
        Me.lblCGMessage.Text = "."
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.BackColor = System.Drawing.Color.Transparent
        Me.Label1.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label1.Location = New System.Drawing.Point(29, 164)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(61, 16)
        Me.Label1.TabIndex = 62
        Me.Label1.Text = "Instance:"
        '
        'cmbSelectedGPU
        '
        Me.cmbSelectedGPU.BackColor = System.Drawing.Color.FromArgb(CType(CType(84, Byte), Integer), CType(CType(84, Byte), Integer), CType(CType(84, Byte), Integer))
        Me.cmbSelectedGPU.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.cmbSelectedGPU.FormattingEnabled = True
        Me.cmbSelectedGPU.Location = New System.Drawing.Point(96, 159)
        Me.cmbSelectedGPU.Name = "cmbSelectedGPU"
        Me.cmbSelectedGPU.Size = New System.Drawing.Size(219, 21)
        Me.cmbSelectedGPU.TabIndex = 61
        '
        'btnHideCgminer
        '
        Me.btnHideCgminer.BackColor = System.Drawing.Color.FromArgb(CType(CType(84, Byte), Integer), CType(CType(84, Byte), Integer), CType(CType(84, Byte), Integer))
        Me.btnHideCgminer.ForeColor = System.Drawing.Color.Lime
        Me.btnHideCgminer.Location = New System.Drawing.Point(207, 186)
        Me.btnHideCgminer.Name = "btnHideCgminer"
        Me.btnHideCgminer.Size = New System.Drawing.Size(108, 22)
        Me.btnHideCgminer.TabIndex = 60
        Me.btnHideCgminer.Text = "Hide CG Miner"
        Me.btnHideCgminer.UseVisualStyleBackColor = False
        '
        'btnShowCgminer
        '
        Me.btnShowCgminer.BackColor = System.Drawing.Color.FromArgb(CType(CType(84, Byte), Integer), CType(CType(84, Byte), Integer), CType(CType(84, Byte), Integer))
        Me.btnShowCgminer.ForeColor = System.Drawing.Color.Lime
        Me.btnShowCgminer.Location = New System.Drawing.Point(32, 186)
        Me.btnShowCgminer.Name = "btnShowCgminer"
        Me.btnShowCgminer.Size = New System.Drawing.Size(118, 22)
        Me.btnShowCgminer.TabIndex = 59
        Me.btnShowCgminer.Text = "Show CG Miner"
        Me.btnShowCgminer.UseVisualStyleBackColor = False
        '
        'Label16
        '
        Me.Label16.AutoSize = True
        Me.Label16.BackColor = System.Drawing.Color.Transparent
        Me.Label16.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label16.Location = New System.Drawing.Point(186, 111)
        Me.Label16.Name = "Label16"
        Me.Label16.Size = New System.Drawing.Size(71, 16)
        Me.Label16.TabIndex = 58
        Me.Label16.Text = "CgMiner 4:"
        '
        'Label15
        '
        Me.Label15.AutoSize = True
        Me.Label15.BackColor = System.Drawing.Color.Transparent
        Me.Label15.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label15.Location = New System.Drawing.Point(186, 89)
        Me.Label15.Name = "Label15"
        Me.Label15.Size = New System.Drawing.Size(71, 16)
        Me.Label15.TabIndex = 57
        Me.Label15.Text = "CgMiner 3:"
        '
        'Label14
        '
        Me.Label14.AutoSize = True
        Me.Label14.BackColor = System.Drawing.Color.Transparent
        Me.Label14.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label14.Location = New System.Drawing.Point(186, 69)
        Me.Label14.Name = "Label14"
        Me.Label14.Size = New System.Drawing.Size(71, 16)
        Me.Label14.TabIndex = 56
        Me.Label14.Text = "CgMiner 2:"
        '
        'Label12
        '
        Me.Label12.AutoSize = True
        Me.Label12.BackColor = System.Drawing.Color.Transparent
        Me.Label12.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label12.Location = New System.Drawing.Point(185, 50)
        Me.Label12.Name = "Label12"
        Me.Label12.Size = New System.Drawing.Size(71, 16)
        Me.Label12.TabIndex = 55
        Me.Label12.Text = "CgMiner 1:"
        '
        'Label8
        '
        Me.Label8.AutoSize = True
        Me.Label8.BackColor = System.Drawing.Color.Transparent
        Me.Label8.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label8.Location = New System.Drawing.Point(185, 31)
        Me.Label8.Name = "Label8"
        Me.Label8.Size = New System.Drawing.Size(71, 16)
        Me.Label8.TabIndex = 53
        Me.Label8.Text = "CgMiner 0:"
        '
        'pbCgminer
        '
        Me.pbCgminer.BackgroundImage = Global.BoincStake.My.Resources.Resources.gradient
        Me.pbCgminer.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch
        Me.pbCgminer.Location = New System.Drawing.Point(0, 1)
        Me.pbCgminer.Name = "pbCgminer"
        Me.pbCgminer.Size = New System.Drawing.Size(909, 360)
        Me.pbCgminer.TabIndex = 3
        Me.pbCgminer.TabStop = False
        '
        'TabGuiMiner
        '
        Me.TabGuiMiner.Controls.Add(Me.Pb1)
        Me.TabGuiMiner.Location = New System.Drawing.Point(4, 22)
        Me.TabGuiMiner.Name = "TabGuiMiner"
        Me.TabGuiMiner.Padding = New System.Windows.Forms.Padding(3)
        Me.TabGuiMiner.Size = New System.Drawing.Size(905, 357)
        Me.TabGuiMiner.TabIndex = 1
        Me.TabGuiMiner.Text = "Gui Miner"
        Me.TabGuiMiner.UseVisualStyleBackColor = True
        '
        'Pb1
        '
        Me.Pb1.BackgroundImage = Global.BoincStake.My.Resources.Resources.gradient
        Me.Pb1.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch
        Me.Pb1.Location = New System.Drawing.Point(-4, -4)
        Me.Pb1.Name = "Pb1"
        Me.Pb1.Size = New System.Drawing.Size(905, 359)
        Me.Pb1.TabIndex = 2
        Me.Pb1.TabStop = False
        '
        'TabPage1
        '
        Me.TabPage1.Controls.Add(Me.BoincWebBrowser)
        Me.TabPage1.Location = New System.Drawing.Point(4, 22)
        Me.TabPage1.Name = "TabPage1"
        Me.TabPage1.Padding = New System.Windows.Forms.Padding(3)
        Me.TabPage1.Size = New System.Drawing.Size(905, 357)
        Me.TabPage1.TabIndex = 2
        Me.TabPage1.Text = "Boinc Stats"
        Me.TabPage1.UseVisualStyleBackColor = True
        '
        'BoincWebBrowser
        '
        Me.BoincWebBrowser.Dock = System.Windows.Forms.DockStyle.Fill
        Me.BoincWebBrowser.Location = New System.Drawing.Point(3, 3)
        Me.BoincWebBrowser.MinimumSize = New System.Drawing.Size(20, 20)
        Me.BoincWebBrowser.Name = "BoincWebBrowser"
        Me.BoincWebBrowser.ScriptErrorsSuppressed = True
        Me.BoincWebBrowser.Size = New System.Drawing.Size(899, 351)
        Me.BoincWebBrowser.TabIndex = 0
        Me.BoincWebBrowser.Url = New System.Uri("http://boincstats.com/en/stats/-1/team/detail/118094994/overview", System.UriKind.Absolute)
        '
        'TimerCGMonitor
        '
        Me.TimerCGMonitor.Enabled = True
        Me.TimerCGMonitor.Interval = 300000
        '
        'lblWarning
        '
        Me.lblWarning.AutoSize = True
        Me.lblWarning.BackColor = System.Drawing.Color.Transparent
        Me.lblWarning.Font = New System.Drawing.Font("Segoe Print", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblWarning.ForeColor = System.Drawing.Color.Red
        Me.lblWarning.Location = New System.Drawing.Point(152, 673)
        Me.lblWarning.Name = "lblWarning"
        Me.lblWarning.Size = New System.Drawing.Size(25, 37)
        Me.lblWarning.TabIndex = 55
        Me.lblWarning.Text = " "
        '
        'msBlockHeight
        '
        Me.msBlockHeight.AutoSize = True
        Me.msBlockHeight.BackColor = System.Drawing.Color.Transparent
        Me.msBlockHeight.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.msBlockHeight.Location = New System.Drawing.Point(869, 522)
        Me.msBlockHeight.Name = "msBlockHeight"
        Me.msBlockHeight.Size = New System.Drawing.Size(15, 16)
        Me.msBlockHeight.TabIndex = 57
        Me.msBlockHeight.Text = "0"
        '
        'Label18
        '
        Me.Label18.AutoSize = True
        Me.Label18.BackColor = System.Drawing.Color.Transparent
        Me.Label18.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label18.Location = New System.Drawing.Point(818, 522)
        Me.Label18.Name = "Label18"
        Me.Label18.Size = New System.Drawing.Size(45, 16)
        Me.Label18.TabIndex = 56
        Me.Label18.Text = "Block:"
        '
        'frmMining
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.BackgroundImage = Global.BoincStake.My.Resources.Resources.GradientU
        Me.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch
        Me.ClientSize = New System.Drawing.Size(940, 793)
        Me.Controls.Add(Me.msBlockHeight)
        Me.Controls.Add(Me.Label18)
        Me.Controls.Add(Me.lblWarning)
        Me.Controls.Add(Me.TabControl1)
        Me.Controls.Add(Me.lblCPUMinerElapsed)
        Me.Controls.Add(Me.Label5)
        Me.Controls.Add(Me.btnHide)
        Me.Controls.Add(Me.btnClose)
        Me.Controls.Add(Me.lblRestartWallet)
        Me.Controls.Add(Me.Label4)
        Me.Controls.Add(Me.lblRestartMiner)
        Me.Controls.Add(Me.Label3)
        Me.Controls.Add(Me.btnRestartMiner)
        Me.Controls.Add(Me.lblMD5)
        Me.Controls.Add(Me.Label2)
        Me.Controls.Add(Me.lblVersion)
        Me.Controls.Add(Me.lblAvgCredits)
        Me.Controls.Add(Me.lblThanks)
        Me.Controls.Add(Me.lbltxtAvgCredits)
        Me.Controls.Add(Me.lblTxtVersion)
        Me.Controls.Add(Me.ChartUtilization)
        Me.Controls.Add(Me.Chart1)
        Me.Controls.Add(Me.btnRefresh)
        Me.Controls.Add(Me.MenuStrip1)
        Me.DoubleBuffered = True
        Me.ForeColor = System.Drawing.Color.Lime
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.MainMenuStrip = Me.MenuStrip1
        Me.Name = "frmMining"
        Me.Text = "Gridcoin Mining Module 2.0"
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).EndInit()
        Me.MenuStrip1.ResumeLayout(False)
        Me.MenuStrip1.PerformLayout()
        Me.TabControl1.ResumeLayout(False)
        Me.TabCGMINER.ResumeLayout(False)
        Me.GroupBox3.ResumeLayout(False)
        Me.GroupBox2.ResumeLayout(False)
        CType(Me.ChartHashRate, System.ComponentModel.ISupportInitialize).EndInit()
        Me.GroupBox1.ResumeLayout(False)
        Me.GroupBox1.PerformLayout()
        CType(Me.pbCgminer, System.ComponentModel.ISupportInitialize).EndInit()
        Me.TabGuiMiner.ResumeLayout(False)
        CType(Me.Pb1, System.ComponentModel.ISupportInitialize).EndInit()
        Me.TabPage1.ResumeLayout(False)
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents btnRefresh As System.Windows.Forms.Button
    Friend WithEvents Chart1 As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents ChartUtilization As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents lblTxtVersion As System.Windows.Forms.Label
    Friend WithEvents lbltxtAvgCredits As System.Windows.Forms.Label
    Friend WithEvents lblThanks As System.Windows.Forms.Label
    Friend WithEvents lblAvgCredits As System.Windows.Forms.Label
    Friend WithEvents lblVersion As System.Windows.Forms.Label
    Friend WithEvents lblMD5 As System.Windows.Forms.Label
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents tOneMinute As System.Windows.Forms.Timer
    Friend WithEvents btnRestartMiner As System.Windows.Forms.Button
    Friend WithEvents lblRestartMiner As System.Windows.Forms.Label
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents lblRestartWallet As System.Windows.Forms.Label
    Friend WithEvents Label4 As System.Windows.Forms.Label
    Friend WithEvents btnClose As System.Windows.Forms.Button
    Friend WithEvents btnHide As System.Windows.Forms.Button
    Friend WithEvents timerBoincBlock As System.Windows.Forms.Timer
    Friend WithEvents lblCPUMinerElapsed As System.Windows.Forms.Label
    Friend WithEvents Label5 As System.Windows.Forms.Label
    Friend WithEvents lblAccepted As System.Windows.Forms.Label
    Friend WithEvents Label7 As System.Windows.Forms.Label
    Friend WithEvents lblGPUMhs As System.Windows.Forms.Label
    Friend WithEvents Label9 As System.Windows.Forms.Label
    Friend WithEvents lblStale As System.Windows.Forms.Label
    Friend WithEvents Label11 As System.Windows.Forms.Label
    Friend WithEvents lblInvalid As System.Windows.Forms.Label
    Friend WithEvents Label13 As System.Windows.Forms.Label
    Friend WithEvents timerReaper As System.Windows.Forms.Timer
    Friend WithEvents MenuStrip1 As System.Windows.Forms.MenuStrip
    Friend WithEvents FileToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents HideToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents ConfigurationToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents chkFullSpeed As System.Windows.Forms.CheckBox
    Friend WithEvents chkMiningEnabled As System.Windows.Forms.CheckBox
    Friend WithEvents txtGPU0 As System.Windows.Forms.Label
    Friend WithEvents txtGPU1 As System.Windows.Forms.Label
    Friend WithEvents txtGPU2 As System.Windows.Forms.Label
    Friend WithEvents txtGPU3 As System.Windows.Forms.Label
    Friend WithEvents txtGPU4 As System.Windows.Forms.Label
    Friend WithEvents TabControl1 As System.Windows.Forms.TabControl
    Friend WithEvents TabCGMINER As System.Windows.Forms.TabPage
    Friend WithEvents TabGuiMiner As System.Windows.Forms.TabPage
    Friend WithEvents pbCgminer As System.Windows.Forms.PictureBox
    Friend WithEvents Pb1 As System.Windows.Forms.PictureBox
    Friend WithEvents GroupBox3 As System.Windows.Forms.GroupBox
    Friend WithEvents RichTextBox1 As System.Windows.Forms.RichTextBox
    Friend WithEvents GroupBox2 As System.Windows.Forms.GroupBox
    Friend WithEvents ChartHashRate As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents Label16 As System.Windows.Forms.Label
    Friend WithEvents Label15 As System.Windows.Forms.Label
    Friend WithEvents Label14 As System.Windows.Forms.Label
    Friend WithEvents Label12 As System.Windows.Forms.Label
    Friend WithEvents Label8 As System.Windows.Forms.Label
    Friend WithEvents btnHideCgminer As System.Windows.Forms.Button
    Friend WithEvents btnShowCgminer As System.Windows.Forms.Button
    Friend WithEvents Label1 As System.Windows.Forms.Label
    Friend WithEvents cmbSelectedGPU As System.Windows.Forms.ComboBox
    Friend WithEvents lblCGMessage As System.Windows.Forms.Label
    Friend WithEvents TimerCGMonitor As System.Windows.Forms.Timer
    Friend WithEvents chkCGMonitor As System.Windows.Forms.CheckBox
    Friend WithEvents lblWarning As System.Windows.Forms.Label
    Friend WithEvents GroupBox4 As System.Windows.Forms.GroupBox
    Friend WithEvents msBlockHeight As System.Windows.Forms.Label
    Friend WithEvents Label18 As System.Windows.Forms.Label
    Friend WithEvents PoolsToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents TabPage1 As System.Windows.Forms.TabPage
    Friend WithEvents BoincWebBrowser As System.Windows.Forms.WebBrowser
End Class
