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
        Dim DataGridViewCellStyle1 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle2 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle3 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle4 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle5 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmMining))
        Me.btnRefresh = New System.Windows.Forms.Button()
        Me.Chart1 = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.ChartUtilization = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.lbltxtAvgCredits = New System.Windows.Forms.Label()
        Me.lblThanks = New System.Windows.Forms.Label()
        Me.lblAvgCredits = New System.Windows.Forms.Label()
        Me.tOneMinute = New System.Windows.Forms.Timer(Me.components)
        Me.btnHide = New System.Windows.Forms.Button()
        Me.MenuStrip1 = New System.Windows.Forms.MenuStrip()
        Me.FileToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.HideToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.ConfigurationToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.PoolsToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.TabControl1 = New System.Windows.Forms.TabControl()
        Me.TabOverview = New System.Windows.Forms.TabPage()
        Me.GroupBox4 = New System.Windows.Forms.GroupBox()
        Me.GroupBox3 = New System.Windows.Forms.GroupBox()
        Me.RichTextBox1 = New System.Windows.Forms.RichTextBox()
        Me.GroupBox2 = New System.Windows.Forms.GroupBox()
        Me.ChartHashRate = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.GroupBox1 = New System.Windows.Forms.GroupBox()
        Me.pbCgminer = New System.Windows.Forms.PictureBox()
        Me.TabPage1 = New System.Windows.Forms.TabPage()
        Me.TabPage2 = New System.Windows.Forms.TabPage()
        Me.dgv = New System.Windows.Forms.DataGridView()
        Me.lblWarning = New System.Windows.Forms.Label()
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.MenuStrip1.SuspendLayout()
        Me.TabControl1.SuspendLayout()
        Me.TabOverview.SuspendLayout()
        Me.GroupBox3.SuspendLayout()
        Me.GroupBox2.SuspendLayout()
        CType(Me.ChartHashRate, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.pbCgminer, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.TabPage2.SuspendLayout()
        CType(Me.dgv, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.SuspendLayout()
        '
        'btnRefresh
        '
        Me.btnRefresh.Location = New System.Drawing.Point(31, 734)
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
        'lbltxtAvgCredits
        '
        Me.lbltxtAvgCredits.AutoSize = True
        Me.lbltxtAvgCredits.BackColor = System.Drawing.Color.Transparent
        Me.lbltxtAvgCredits.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!)
        Me.lbltxtAvgCredits.Location = New System.Drawing.Point(539, 578)
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
        Me.lblAvgCredits.Location = New System.Drawing.Point(761, 571)
        Me.lblAvgCredits.Name = "lblAvgCredits"
        Me.lblAvgCredits.Size = New System.Drawing.Size(24, 25)
        Me.lblAvgCredits.TabIndex = 11
        Me.lblAvgCredits.Text = "0"
        '
        'tOneMinute
        '
        Me.tOneMinute.Enabled = True
        Me.tOneMinute.Interval = 60000
        '
        'btnHide
        '
        Me.btnHide.Location = New System.Drawing.Point(159, 734)
        Me.btnHide.Name = "btnHide"
        Me.btnHide.Size = New System.Drawing.Size(118, 35)
        Me.btnHide.TabIndex = 23
        Me.btnHide.Text = "Hide"
        Me.btnHide.UseVisualStyleBackColor = False
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
        'TabControl1
        '
        Me.TabControl1.Controls.Add(Me.TabOverview)
        Me.TabControl1.Controls.Add(Me.TabPage1)
        Me.TabControl1.Controls.Add(Me.TabPage2)
        Me.TabControl1.Location = New System.Drawing.Point(14, 39)
        Me.TabControl1.Name = "TabControl1"
        Me.TabControl1.SelectedIndex = 0
        Me.TabControl1.Size = New System.Drawing.Size(913, 383)
        Me.TabControl1.TabIndex = 53
        '
        'TabOverview
        '
        Me.TabOverview.Controls.Add(Me.GroupBox4)
        Me.TabOverview.Controls.Add(Me.GroupBox3)
        Me.TabOverview.Controls.Add(Me.GroupBox2)
        Me.TabOverview.Controls.Add(Me.GroupBox1)
        Me.TabOverview.Controls.Add(Me.pbCgminer)
        Me.TabOverview.Location = New System.Drawing.Point(4, 22)
        Me.TabOverview.Name = "TabOverview"
        Me.TabOverview.Padding = New System.Windows.Forms.Padding(3)
        Me.TabOverview.Size = New System.Drawing.Size(905, 357)
        Me.TabOverview.TabIndex = 0
        Me.TabOverview.Text = "Overview"
        Me.TabOverview.UseVisualStyleBackColor = True
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
        Me.GroupBox1.ForeColor = System.Drawing.Color.Lime
        Me.GroupBox1.Location = New System.Drawing.Point(6, 6)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(332, 214)
        Me.GroupBox1.TabIndex = 53
        Me.GroupBox1.TabStop = False
        Me.GroupBox1.Text = "Neural Statistics:"
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
        'TabPage1
        '
        Me.TabPage1.Location = New System.Drawing.Point(4, 22)
        Me.TabPage1.Name = "TabPage1"
        Me.TabPage1.Padding = New System.Windows.Forms.Padding(3)
        Me.TabPage1.Size = New System.Drawing.Size(905, 357)
        Me.TabPage1.TabIndex = 2
        Me.TabPage1.Text = "Boinc Stats"
        Me.TabPage1.UseVisualStyleBackColor = True
        '
        'TabPage2
        '
        Me.TabPage2.Controls.Add(Me.dgv)
        Me.TabPage2.Location = New System.Drawing.Point(4, 22)
        Me.TabPage2.Name = "TabPage2"
        Me.TabPage2.Size = New System.Drawing.Size(905, 357)
        Me.TabPage2.TabIndex = 3
        Me.TabPage2.Text = "Neural Network"
        Me.TabPage2.UseVisualStyleBackColor = True
        '
        'dgv
        '
        DataGridViewCellStyle1.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle1.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle1.SelectionBackColor = System.Drawing.Color.Gray
        DataGridViewCellStyle1.SelectionForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.dgv.AlternatingRowsDefaultCellStyle = DataGridViewCellStyle1
        Me.dgv.BackgroundColor = System.Drawing.Color.Black
        DataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle2.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle2.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle2.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText
        DataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.[True]
        Me.dgv.ColumnHeadersDefaultCellStyle = DataGridViewCellStyle2
        Me.dgv.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize
        DataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle3.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle3.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle3.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText
        DataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.[False]
        Me.dgv.DefaultCellStyle = DataGridViewCellStyle3
        Me.dgv.EnableHeadersVisualStyles = False
        Me.dgv.GridColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.dgv.Location = New System.Drawing.Point(0, 0)
        Me.dgv.Name = "dgv"
        DataGridViewCellStyle4.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle4.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle4.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle4.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle4.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle4.SelectionForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(0, Byte), Integer))
        DataGridViewCellStyle4.WrapMode = System.Windows.Forms.DataGridViewTriState.[True]
        Me.dgv.RowHeadersDefaultCellStyle = DataGridViewCellStyle4
        DataGridViewCellStyle5.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle5.ForeColor = System.Drawing.Color.Lime
        Me.dgv.RowsDefaultCellStyle = DataGridViewCellStyle5
        Me.dgv.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect
        Me.dgv.Size = New System.Drawing.Size(902, 354)
        Me.dgv.TabIndex = 1
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
        'frmMining
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.BackgroundImage = Global.BoincStake.My.Resources.Resources.GradientU
        Me.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch
        Me.ClientSize = New System.Drawing.Size(940, 793)
        Me.Controls.Add(Me.lblWarning)
        Me.Controls.Add(Me.TabControl1)
        Me.Controls.Add(Me.btnHide)
        Me.Controls.Add(Me.lblAvgCredits)
        Me.Controls.Add(Me.lblThanks)
        Me.Controls.Add(Me.lbltxtAvgCredits)
        Me.Controls.Add(Me.ChartUtilization)
        Me.Controls.Add(Me.Chart1)
        Me.Controls.Add(Me.btnRefresh)
        Me.Controls.Add(Me.MenuStrip1)
        Me.DoubleBuffered = True
        Me.ForeColor = System.Drawing.Color.Lime
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.MainMenuStrip = Me.MenuStrip1
        Me.Name = "frmMining"
        Me.Text = "Gridcoin Neural Network 1.0"
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).EndInit()
        Me.MenuStrip1.ResumeLayout(False)
        Me.MenuStrip1.PerformLayout()
        Me.TabControl1.ResumeLayout(False)
        Me.TabOverview.ResumeLayout(False)
        Me.GroupBox3.ResumeLayout(False)
        Me.GroupBox2.ResumeLayout(False)
        CType(Me.ChartHashRate, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.pbCgminer, System.ComponentModel.ISupportInitialize).EndInit()
        Me.TabPage2.ResumeLayout(false)
        CType(Me.dgv,System.ComponentModel.ISupportInitialize).EndInit
        Me.ResumeLayout(false)
        Me.PerformLayout

End Sub
    Friend WithEvents btnRefresh As System.Windows.Forms.Button
    Friend WithEvents Chart1 As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents ChartUtilization As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents lbltxtAvgCredits As System.Windows.Forms.Label
    Friend WithEvents lblThanks As System.Windows.Forms.Label
    Friend WithEvents lblAvgCredits As System.Windows.Forms.Label
    Friend WithEvents tOneMinute As System.Windows.Forms.Timer
    Friend WithEvents btnHide As System.Windows.Forms.Button
    Friend WithEvents MenuStrip1 As System.Windows.Forms.MenuStrip
    Friend WithEvents FileToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents HideToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents ConfigurationToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents TabControl1 As System.Windows.Forms.TabControl
    Friend WithEvents TabOverview As System.Windows.Forms.TabPage
    Friend WithEvents pbCgminer As System.Windows.Forms.PictureBox
    Friend WithEvents GroupBox3 As System.Windows.Forms.GroupBox
    Friend WithEvents RichTextBox1 As System.Windows.Forms.RichTextBox
    Friend WithEvents GroupBox2 As System.Windows.Forms.GroupBox
    Friend WithEvents ChartHashRate As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents lblWarning As System.Windows.Forms.Label
    Friend WithEvents GroupBox4 As System.Windows.Forms.GroupBox
    Friend WithEvents PoolsToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents TabPage1 As System.Windows.Forms.TabPage
    Friend WithEvents TabPage2 As System.Windows.Forms.TabPage
    Friend WithEvents dgv As System.Windows.Forms.DataGridView
End Class
