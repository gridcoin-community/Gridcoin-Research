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
        Dim ChartArea3 As System.Windows.Forms.DataVisualization.Charting.ChartArea = New System.Windows.Forms.DataVisualization.Charting.ChartArea()
        Dim Legend3 As System.Windows.Forms.DataVisualization.Charting.Legend = New System.Windows.Forms.DataVisualization.Charting.Legend()
        Dim Series3 As System.Windows.Forms.DataVisualization.Charting.Series = New System.Windows.Forms.DataVisualization.Charting.Series()
        Dim ChartArea4 As System.Windows.Forms.DataVisualization.Charting.ChartArea = New System.Windows.Forms.DataVisualization.Charting.ChartArea()
        Dim Legend4 As System.Windows.Forms.DataVisualization.Charting.Legend = New System.Windows.Forms.DataVisualization.Charting.Legend()
        Dim Series4 As System.Windows.Forms.DataVisualization.Charting.Series = New System.Windows.Forms.DataVisualization.Charting.Series()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmMining))
        Me.btnRefresh = New System.Windows.Forms.Button()
        Me.PictureBox1 = New System.Windows.Forms.PictureBox()
        Me.Chart1 = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.ChartUtilization = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.lblTxtPower = New System.Windows.Forms.Label()
        Me.lblTxtThreadCount = New System.Windows.Forms.Label()
        Me.lblTxtVersion = New System.Windows.Forms.Label()
        Me.lbltxtAvgCredits = New System.Windows.Forms.Label()
        Me.lblThanks = New System.Windows.Forms.Label()
        Me.lblPower = New System.Windows.Forms.Label()
        Me.lblThreadCount = New System.Windows.Forms.Label()
        Me.lblAvgCredits = New System.Windows.Forms.Label()
        Me.lblVersion = New System.Windows.Forms.Label()
        Me.lblMD5 = New System.Windows.Forms.Label()
        Me.Label2 = New System.Windows.Forms.Label()
        Me.Timer1 = New System.Windows.Forms.Timer(Me.components)
        Me.btnRestartMiner = New System.Windows.Forms.Button()
        Me.lblRestartMiner = New System.Windows.Forms.Label()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.btnRestart = New System.Windows.Forms.Button()
        Me.lblRestartWallet = New System.Windows.Forms.Label()
        Me.Label4 = New System.Windows.Forms.Label()
        Me.btnClose = New System.Windows.Forms.Button()
        Me.btnHide = New System.Windows.Forms.Button()
        Me.btnCloseWallet = New System.Windows.Forms.Button()
        Me.btnTestCPUMiner = New System.Windows.Forms.Button()
        Me.pbBoincBlock = New System.Windows.Forms.ProgressBar()
        Me.lblBoincBlock = New System.Windows.Forms.Label()
        Me.lblBlock = New System.Windows.Forms.Label()
        Me.timerBoincBlock = New System.Windows.Forms.Timer(Me.components)
        Me.lblCPUMinerElapsed = New System.Windows.Forms.Label()
        Me.Label5 = New System.Windows.Forms.Label()
        Me.lblLastBlockHash = New System.Windows.Forms.Label()
        Me.Label6 = New System.Windows.Forms.Label()
        Me.Tc1 = New System.Windows.Forms.TabControl()
        Me.TabPage1 = New System.Windows.Forms.TabPage()
        Me.TabPage2 = New System.Windows.Forms.TabPage()
        Me.TabPage3 = New System.Windows.Forms.TabPage()
        Me.TabPage4 = New System.Windows.Forms.TabPage()
        Me.lblAccepted = New System.Windows.Forms.Label()
        Me.Label7 = New System.Windows.Forms.Label()
        Me.lblGPUMhs = New System.Windows.Forms.Label()
        Me.Label9 = New System.Windows.Forms.Label()
        Me.lblStale = New System.Windows.Forms.Label()
        Me.Label11 = New System.Windows.Forms.Label()
        Me.lblInvalid = New System.Windows.Forms.Label()
        Me.Label13 = New System.Windows.Forms.Label()
        Me.timerReaper = New System.Windows.Forms.Timer(Me.components)
        CType(Me.PictureBox1, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.Tc1.SuspendLayout()
        Me.SuspendLayout()
        '
        'btnRefresh
        '
        Me.btnRefresh.Location = New System.Drawing.Point(12, 709)
        Me.btnRefresh.Name = "btnRefresh"
        Me.btnRefresh.Size = New System.Drawing.Size(118, 35)
        Me.btnRefresh.TabIndex = 0
        Me.btnRefresh.Text = "Refresh"
        Me.btnRefresh.UseVisualStyleBackColor = False
        '
        'PictureBox1
        '
        Me.PictureBox1.BackgroundImage = Global.boinc.My.Resources.Resources.gradient
        Me.PictureBox1.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch
        Me.PictureBox1.Location = New System.Drawing.Point(12, 12)
        Me.PictureBox1.Name = "PictureBox1"
        Me.PictureBox1.Size = New System.Drawing.Size(917, 386)
        Me.PictureBox1.TabIndex = 1
        Me.PictureBox1.TabStop = False
        '
        'Chart1
        '
        Me.Chart1.BackColor = System.Drawing.Color.Black
        Me.Chart1.BackGradientStyle = System.Windows.Forms.DataVisualization.Charting.GradientStyle.HorizontalCenter
        Me.Chart1.BackHatchStyle = System.Windows.Forms.DataVisualization.Charting.ChartHatchStyle.BackwardDiagonal
        Me.Chart1.BackImageTransparentColor = System.Drawing.Color.Black
        Me.Chart1.BackSecondaryColor = System.Drawing.Color.Black
        Me.Chart1.BorderlineColor = System.Drawing.Color.Black
        ChartArea3.Name = "ChartArea1"
        Me.Chart1.ChartAreas.Add(ChartArea3)
        Legend3.Name = "Legend1"
        Me.Chart1.Legends.Add(Legend3)
        Me.Chart1.Location = New System.Drawing.Point(11, 401)
        Me.Chart1.Name = "Chart1"
        Me.Chart1.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.SemiTransparent
        Series3.ChartArea = "ChartArea1"
        Series3.Legend = "Legend1"
        Series3.Name = "Series1"
        Me.Chart1.Series.Add(Series3)
        Me.Chart1.Size = New System.Drawing.Size(917, 128)
        Me.Chart1.TabIndex = 2
        Me.Chart1.Text = "Boinc Utilization"
        '
        'ChartUtilization
        '
        Me.ChartUtilization.BackColor = System.Drawing.Color.Black
        Me.ChartUtilization.BackGradientStyle = System.Windows.Forms.DataVisualization.Charting.GradientStyle.HorizontalCenter
        Me.ChartUtilization.BackHatchStyle = System.Windows.Forms.DataVisualization.Charting.ChartHatchStyle.BackwardDiagonal
        Me.ChartUtilization.BackImageTransparentColor = System.Drawing.Color.Black
        Me.ChartUtilization.BackSecondaryColor = System.Drawing.Color.Black
        Me.ChartUtilization.BorderlineColor = System.Drawing.Color.Black
        ChartArea4.Name = "ChartArea1"
        Me.ChartUtilization.ChartAreas.Add(ChartArea4)
        Legend4.Name = "Legend1"
        Me.ChartUtilization.Legends.Add(Legend4)
        Me.ChartUtilization.Location = New System.Drawing.Point(11, 535)
        Me.ChartUtilization.Name = "ChartUtilization"
        Me.ChartUtilization.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.SemiTransparent
        Series4.ChartArea = "ChartArea1"
        Series4.Legend = "Legend1"
        Series4.Name = "Series1"
        Me.ChartUtilization.Series.Add(Series4)
        Me.ChartUtilization.Size = New System.Drawing.Size(242, 148)
        Me.ChartUtilization.TabIndex = 3
        Me.ChartUtilization.Text = "Chart2"
        '
        'lblTxtPower
        '
        Me.lblTxtPower.AutoSize = True
        Me.lblTxtPower.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblTxtPower.Location = New System.Drawing.Point(492, 535)
        Me.lblTxtPower.Name = "lblTxtPower"
        Me.lblTxtPower.Size = New System.Drawing.Size(274, 25)
        Me.lblTxtPower.TabIndex = 4
        Me.lblTxtPower.Text = "Boinc Processing Power:"
        '
        'lblTxtThreadCount
        '
        Me.lblTxtThreadCount.AutoSize = True
        Me.lblTxtThreadCount.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblTxtThreadCount.Location = New System.Drawing.Point(492, 560)
        Me.lblTxtThreadCount.Name = "lblTxtThreadCount"
        Me.lblTxtThreadCount.Size = New System.Drawing.Size(228, 25)
        Me.lblTxtThreadCount.TabIndex = 5
        Me.lblTxtThreadCount.Text = "Boinc Thread Count:"
        '
        'lblTxtVersion
        '
        Me.lblTxtVersion.AutoSize = True
        Me.lblTxtVersion.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblTxtVersion.Location = New System.Drawing.Point(259, 542)
        Me.lblTxtVersion.Name = "lblTxtVersion"
        Me.lblTxtVersion.Size = New System.Drawing.Size(57, 16)
        Me.lblTxtVersion.TabIndex = 6
        Me.lblTxtVersion.Text = "Version:"
        '
        'lbltxtAvgCredits
        '
        Me.lbltxtAvgCredits.AutoSize = True
        Me.lbltxtAvgCredits.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lbltxtAvgCredits.Location = New System.Drawing.Point(492, 585)
        Me.lbltxtAvgCredits.Name = "lbltxtAvgCredits"
        Me.lbltxtAvgCredits.Size = New System.Drawing.Size(267, 25)
        Me.lbltxtAvgCredits.TabIndex = 7
        Me.lbltxtAvgCredits.Text = "Boinc Avg Daily Credits:"
        '
        'lblThanks
        '
        Me.lblThanks.AutoSize = True
        Me.lblThanks.BackColor = System.Drawing.Color.Transparent
        Me.lblThanks.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblThanks.Location = New System.Drawing.Point(8, 747)
        Me.lblThanks.Name = "lblThanks"
        Me.lblThanks.Size = New System.Drawing.Size(10, 13)
        Me.lblThanks.TabIndex = 8
        Me.lblThanks.Text = "."
        '
        'lblPower
        '
        Me.lblPower.AutoSize = True
        Me.lblPower.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblPower.Location = New System.Drawing.Point(772, 535)
        Me.lblPower.Name = "lblPower"
        Me.lblPower.Size = New System.Drawing.Size(25, 25)
        Me.lblPower.TabIndex = 9
        Me.lblPower.Text = "0"
        '
        'lblThreadCount
        '
        Me.lblThreadCount.AutoSize = True
        Me.lblThreadCount.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblThreadCount.Location = New System.Drawing.Point(772, 560)
        Me.lblThreadCount.Name = "lblThreadCount"
        Me.lblThreadCount.Size = New System.Drawing.Size(25, 25)
        Me.lblThreadCount.TabIndex = 10
        Me.lblThreadCount.Text = "0"
        '
        'lblAvgCredits
        '
        Me.lblAvgCredits.AutoSize = True
        Me.lblAvgCredits.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblAvgCredits.Location = New System.Drawing.Point(772, 585)
        Me.lblAvgCredits.Name = "lblAvgCredits"
        Me.lblAvgCredits.Size = New System.Drawing.Size(25, 25)
        Me.lblAvgCredits.TabIndex = 11
        Me.lblAvgCredits.Text = "0"
        '
        'lblVersion
        '
        Me.lblVersion.AutoSize = True
        Me.lblVersion.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblVersion.Location = New System.Drawing.Point(404, 542)
        Me.lblVersion.Name = "lblVersion"
        Me.lblVersion.Size = New System.Drawing.Size(15, 16)
        Me.lblVersion.TabIndex = 12
        Me.lblVersion.Text = "0"
        '
        'lblMD5
        '
        Me.lblMD5.AutoSize = True
        Me.lblMD5.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblMD5.Location = New System.Drawing.Point(587, 610)
        Me.lblMD5.Name = "lblMD5"
        Me.lblMD5.Size = New System.Drawing.Size(15, 16)
        Me.lblMD5.TabIndex = 14
        Me.lblMD5.Text = "0"
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label2.Location = New System.Drawing.Point(494, 611)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(76, 16)
        Me.Label2.TabIndex = 13
        Me.Label2.Text = "Boinc MD5:"
        '
        'Timer1
        '
        Me.Timer1.Enabled = True
        Me.Timer1.Interval = 300000
        '
        'btnRestartMiner
        '
        Me.btnRestartMiner.Location = New System.Drawing.Point(136, 709)
        Me.btnRestartMiner.Name = "btnRestartMiner"
        Me.btnRestartMiner.Size = New System.Drawing.Size(118, 35)
        Me.btnRestartMiner.TabIndex = 15
        Me.btnRestartMiner.Text = "Restart Miner"
        Me.btnRestartMiner.UseVisualStyleBackColor = False
        '
        'lblRestartMiner
        '
        Me.lblRestartMiner.AutoSize = True
        Me.lblRestartMiner.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblRestartMiner.Location = New System.Drawing.Point(404, 558)
        Me.lblRestartMiner.Name = "lblRestartMiner"
        Me.lblRestartMiner.Size = New System.Drawing.Size(15, 16)
        Me.lblRestartMiner.TabIndex = 17
        Me.lblRestartMiner.Text = "0"
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label3.Location = New System.Drawing.Point(259, 560)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(134, 16)
        Me.Label3.TabIndex = 16
        Me.Label3.Text = "Restart Miner In Mins:"
        '
        'btnRestart
        '
        Me.btnRestart.Location = New System.Drawing.Point(260, 709)
        Me.btnRestart.Name = "btnRestart"
        Me.btnRestart.Size = New System.Drawing.Size(118, 35)
        Me.btnRestart.TabIndex = 18
        Me.btnRestart.Text = "Restart Wallet"
        Me.btnRestart.UseVisualStyleBackColor = False
        '
        'lblRestartWallet
        '
        Me.lblRestartWallet.AutoSize = True
        Me.lblRestartWallet.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblRestartWallet.Location = New System.Drawing.Point(404, 576)
        Me.lblRestartWallet.Name = "lblRestartWallet"
        Me.lblRestartWallet.Size = New System.Drawing.Size(15, 16)
        Me.lblRestartWallet.TabIndex = 21
        Me.lblRestartWallet.Text = "0"
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label4.Location = New System.Drawing.Point(259, 576)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(139, 16)
        Me.Label4.TabIndex = 20
        Me.Label4.Text = "Restart Wallet In Mins:"
        '
        'btnClose
        '
        Me.btnClose.Location = New System.Drawing.Point(384, 709)
        Me.btnClose.Name = "btnClose"
        Me.btnClose.Size = New System.Drawing.Size(118, 35)
        Me.btnClose.TabIndex = 22
        Me.btnClose.Text = "Close Miner"
        Me.btnClose.UseVisualStyleBackColor = False
        '
        'btnHide
        '
        Me.btnHide.Location = New System.Drawing.Point(508, 709)
        Me.btnHide.Name = "btnHide"
        Me.btnHide.Size = New System.Drawing.Size(118, 35)
        Me.btnHide.TabIndex = 23
        Me.btnHide.Text = "Hide"
        Me.btnHide.UseVisualStyleBackColor = False
        '
        'btnCloseWallet
        '
        Me.btnCloseWallet.Location = New System.Drawing.Point(632, 709)
        Me.btnCloseWallet.Name = "btnCloseWallet"
        Me.btnCloseWallet.Size = New System.Drawing.Size(118, 35)
        Me.btnCloseWallet.TabIndex = 24
        Me.btnCloseWallet.Text = "Close Wallet"
        Me.btnCloseWallet.UseVisualStyleBackColor = False
        '
        'btnTestCPUMiner
        '
        Me.btnTestCPUMiner.Location = New System.Drawing.Point(756, 709)
        Me.btnTestCPUMiner.Name = "btnTestCPUMiner"
        Me.btnTestCPUMiner.Size = New System.Drawing.Size(118, 35)
        Me.btnTestCPUMiner.TabIndex = 25
        Me.btnTestCPUMiner.Text = "Test CPUMiner"
        Me.btnTestCPUMiner.UseVisualStyleBackColor = False
        '
        'pbBoincBlock
        '
        Me.pbBoincBlock.Location = New System.Drawing.Point(497, 692)
        Me.pbBoincBlock.Maximum = 60
        Me.pbBoincBlock.Name = "pbBoincBlock"
        Me.pbBoincBlock.Size = New System.Drawing.Size(377, 11)
        Me.pbBoincBlock.TabIndex = 26
        '
        'lblBoincBlock
        '
        Me.lblBoincBlock.AutoSize = True
        Me.lblBoincBlock.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblBoincBlock.Location = New System.Drawing.Point(587, 629)
        Me.lblBoincBlock.Name = "lblBoincBlock"
        Me.lblBoincBlock.Size = New System.Drawing.Size(15, 16)
        Me.lblBoincBlock.TabIndex = 28
        Me.lblBoincBlock.Text = "0"
        '
        'lblBlock
        '
        Me.lblBlock.AutoSize = True
        Me.lblBlock.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblBlock.Location = New System.Drawing.Point(494, 629)
        Me.lblBlock.Name = "lblBlock"
        Me.lblBlock.Size = New System.Drawing.Size(82, 16)
        Me.lblBlock.TabIndex = 27
        Me.lblBlock.Text = "Boinc Block:"
        '
        'timerBoincBlock
        '
        Me.timerBoincBlock.Enabled = True
        Me.timerBoincBlock.Interval = 1000
        '
        'lblCPUMinerElapsed
        '
        Me.lblCPUMinerElapsed.AutoSize = True
        Me.lblCPUMinerElapsed.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblCPUMinerElapsed.Location = New System.Drawing.Point(587, 648)
        Me.lblCPUMinerElapsed.Name = "lblCPUMinerElapsed"
        Me.lblCPUMinerElapsed.Size = New System.Drawing.Size(15, 16)
        Me.lblCPUMinerElapsed.TabIndex = 30
        Me.lblCPUMinerElapsed.Text = "0"
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label5.Location = New System.Drawing.Point(494, 648)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(85, 16)
        Me.Label5.TabIndex = 29
        Me.Label5.Text = "Boinc KH/ps:"
        '
        'lblLastBlockHash
        '
        Me.lblLastBlockHash.AutoSize = True
        Me.lblLastBlockHash.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblLastBlockHash.Location = New System.Drawing.Point(587, 667)
        Me.lblLastBlockHash.Name = "lblLastBlockHash"
        Me.lblLastBlockHash.Size = New System.Drawing.Size(15, 16)
        Me.lblLastBlockHash.TabIndex = 32
        Me.lblLastBlockHash.Text = "0"
        '
        'Label6
        '
        Me.Label6.AutoSize = True
        Me.Label6.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label6.Location = New System.Drawing.Point(494, 667)
        Me.Label6.Name = "Label6"
        Me.Label6.Size = New System.Drawing.Size(73, 16)
        Me.Label6.TabIndex = 31
        Me.Label6.Text = "Last Block:"
        '
        'Tc1
        '
        Me.Tc1.Controls.Add(Me.TabPage1)
        Me.Tc1.Controls.Add(Me.TabPage2)
        Me.Tc1.Controls.Add(Me.TabPage3)
        Me.Tc1.Controls.Add(Me.TabPage4)
        Me.Tc1.Location = New System.Drawing.Point(12, 12)
        Me.Tc1.Name = "Tc1"
        Me.Tc1.SelectedIndex = 0
        Me.Tc1.Size = New System.Drawing.Size(916, 383)
        Me.Tc1.TabIndex = 33
        '
        'TabPage1
        '
        Me.TabPage1.BackColor = System.Drawing.Color.Black
        Me.TabPage1.Location = New System.Drawing.Point(4, 22)
        Me.TabPage1.Name = "TabPage1"
        Me.TabPage1.Padding = New System.Windows.Forms.Padding(3)
        Me.TabPage1.Size = New System.Drawing.Size(908, 357)
        Me.TabPage1.TabIndex = 0
        Me.TabPage1.Text = "Reaper 01"
        '
        'TabPage2
        '
        Me.TabPage2.BackColor = System.Drawing.Color.Black
        Me.TabPage2.Location = New System.Drawing.Point(4, 22)
        Me.TabPage2.Name = "TabPage2"
        Me.TabPage2.Padding = New System.Windows.Forms.Padding(3)
        Me.TabPage2.Size = New System.Drawing.Size(908, 357)
        Me.TabPage2.TabIndex = 1
        Me.TabPage2.Text = "Reaper 02"
        '
        'TabPage3
        '
        Me.TabPage3.BackColor = System.Drawing.Color.Black
        Me.TabPage3.Location = New System.Drawing.Point(4, 22)
        Me.TabPage3.Name = "TabPage3"
        Me.TabPage3.Size = New System.Drawing.Size(908, 357)
        Me.TabPage3.TabIndex = 2
        Me.TabPage3.Text = "Reaper 03"
        '
        'TabPage4
        '
        Me.TabPage4.BackColor = System.Drawing.Color.Black
        Me.TabPage4.Location = New System.Drawing.Point(4, 22)
        Me.TabPage4.Name = "TabPage4"
        Me.TabPage4.Size = New System.Drawing.Size(908, 357)
        Me.TabPage4.TabIndex = 3
        Me.TabPage4.Text = "Reaper 04"
        '
        'lblAccepted
        '
        Me.lblAccepted.AutoSize = True
        Me.lblAccepted.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblAccepted.Location = New System.Drawing.Point(352, 630)
        Me.lblAccepted.Name = "lblAccepted"
        Me.lblAccepted.Size = New System.Drawing.Size(15, 16)
        Me.lblAccepted.TabIndex = 37
        Me.lblAccepted.Text = "0"
        '
        'Label7
        '
        Me.Label7.AutoSize = True
        Me.Label7.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label7.Location = New System.Drawing.Point(259, 630)
        Me.Label7.Name = "Label7"
        Me.Label7.Size = New System.Drawing.Size(69, 16)
        Me.Label7.TabIndex = 36
        Me.Label7.Text = "Accepted:"
        '
        'lblGPUMhs
        '
        Me.lblGPUMhs.AutoSize = True
        Me.lblGPUMhs.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblGPUMhs.Location = New System.Drawing.Point(352, 611)
        Me.lblGPUMhs.Name = "lblGPUMhs"
        Me.lblGPUMhs.Size = New System.Drawing.Size(15, 16)
        Me.lblGPUMhs.TabIndex = 35
        Me.lblGPUMhs.Text = "0"
        '
        'Label9
        '
        Me.Label9.AutoSize = True
        Me.Label9.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label9.Location = New System.Drawing.Point(259, 611)
        Me.Label9.Name = "Label9"
        Me.Label9.Size = New System.Drawing.Size(75, 16)
        Me.Label9.TabIndex = 34
        Me.Label9.Text = "GPU MH/s:"
        '
        'lblStale
        '
        Me.lblStale.AutoSize = True
        Me.lblStale.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblStale.Location = New System.Drawing.Point(352, 649)
        Me.lblStale.Name = "lblStale"
        Me.lblStale.Size = New System.Drawing.Size(15, 16)
        Me.lblStale.TabIndex = 39
        Me.lblStale.Text = "0"
        '
        'Label11
        '
        Me.Label11.AutoSize = True
        Me.Label11.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label11.Location = New System.Drawing.Point(259, 649)
        Me.Label11.Name = "Label11"
        Me.Label11.Size = New System.Drawing.Size(42, 16)
        Me.Label11.TabIndex = 38
        Me.Label11.Text = "Stale:"
        '
        'lblInvalid
        '
        Me.lblInvalid.AutoSize = True
        Me.lblInvalid.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblInvalid.Location = New System.Drawing.Point(352, 669)
        Me.lblInvalid.Name = "lblInvalid"
        Me.lblInvalid.Size = New System.Drawing.Size(15, 16)
        Me.lblInvalid.TabIndex = 41
        Me.lblInvalid.Text = "0"
        '
        'Label13
        '
        Me.Label13.AutoSize = True
        Me.Label13.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label13.Location = New System.Drawing.Point(259, 669)
        Me.Label13.Name = "Label13"
        Me.Label13.Size = New System.Drawing.Size(50, 16)
        Me.Label13.TabIndex = 40
        Me.Label13.Text = "Invalid:"
        '
        'timerReaper
        '
        Me.timerReaper.Enabled = True
        Me.timerReaper.Interval = 5000
        '
        'frmMining
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(940, 760)
        Me.Controls.Add(Me.lblInvalid)
        Me.Controls.Add(Me.Label13)
        Me.Controls.Add(Me.lblStale)
        Me.Controls.Add(Me.Label11)
        Me.Controls.Add(Me.lblAccepted)
        Me.Controls.Add(Me.Label7)
        Me.Controls.Add(Me.lblGPUMhs)
        Me.Controls.Add(Me.Label9)
        Me.Controls.Add(Me.Tc1)
        Me.Controls.Add(Me.lblLastBlockHash)
        Me.Controls.Add(Me.Label6)
        Me.Controls.Add(Me.lblCPUMinerElapsed)
        Me.Controls.Add(Me.Label5)
        Me.Controls.Add(Me.lblBoincBlock)
        Me.Controls.Add(Me.lblBlock)
        Me.Controls.Add(Me.pbBoincBlock)
        Me.Controls.Add(Me.btnTestCPUMiner)
        Me.Controls.Add(Me.btnCloseWallet)
        Me.Controls.Add(Me.btnHide)
        Me.Controls.Add(Me.btnClose)
        Me.Controls.Add(Me.lblRestartWallet)
        Me.Controls.Add(Me.Label4)
        Me.Controls.Add(Me.btnRestart)
        Me.Controls.Add(Me.lblRestartMiner)
        Me.Controls.Add(Me.Label3)
        Me.Controls.Add(Me.btnRestartMiner)
        Me.Controls.Add(Me.lblMD5)
        Me.Controls.Add(Me.Label2)
        Me.Controls.Add(Me.lblVersion)
        Me.Controls.Add(Me.lblAvgCredits)
        Me.Controls.Add(Me.lblThreadCount)
        Me.Controls.Add(Me.lblPower)
        Me.Controls.Add(Me.lblThanks)
        Me.Controls.Add(Me.lbltxtAvgCredits)
        Me.Controls.Add(Me.lblTxtVersion)
        Me.Controls.Add(Me.lblTxtThreadCount)
        Me.Controls.Add(Me.lblTxtPower)
        Me.Controls.Add(Me.ChartUtilization)
        Me.Controls.Add(Me.Chart1)
        Me.Controls.Add(Me.PictureBox1)
        Me.Controls.Add(Me.btnRefresh)
        Me.ForeColor = System.Drawing.Color.Lime
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmMining"
        Me.Text = "Gridcoin Mining Module"
        CType(Me.PictureBox1, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).EndInit()
        Me.Tc1.ResumeLayout(False)
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents btnRefresh As System.Windows.Forms.Button
    Friend WithEvents PictureBox1 As System.Windows.Forms.PictureBox
    Friend WithEvents Chart1 As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents ChartUtilization As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents lblTxtPower As System.Windows.Forms.Label
    Friend WithEvents lblTxtThreadCount As System.Windows.Forms.Label
    Friend WithEvents lblTxtVersion As System.Windows.Forms.Label
    Friend WithEvents lbltxtAvgCredits As System.Windows.Forms.Label
    Friend WithEvents lblThanks As System.Windows.Forms.Label
    Friend WithEvents lblPower As System.Windows.Forms.Label
    Friend WithEvents lblThreadCount As System.Windows.Forms.Label
    Friend WithEvents lblAvgCredits As System.Windows.Forms.Label
    Friend WithEvents lblVersion As System.Windows.Forms.Label
    Friend WithEvents lblMD5 As System.Windows.Forms.Label
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents Timer1 As System.Windows.Forms.Timer
    Friend WithEvents btnRestartMiner As System.Windows.Forms.Button
    Friend WithEvents lblRestartMiner As System.Windows.Forms.Label
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents btnRestart As System.Windows.Forms.Button
    Friend WithEvents lblRestartWallet As System.Windows.Forms.Label
    Friend WithEvents Label4 As System.Windows.Forms.Label
    Friend WithEvents btnClose As System.Windows.Forms.Button
    Friend WithEvents btnHide As System.Windows.Forms.Button
    Friend WithEvents btnCloseWallet As System.Windows.Forms.Button
    Friend WithEvents btnTestCPUMiner As System.Windows.Forms.Button
    Friend WithEvents pbBoincBlock As System.Windows.Forms.ProgressBar
    Friend WithEvents lblBoincBlock As System.Windows.Forms.Label
    Friend WithEvents lblBlock As System.Windows.Forms.Label
    Friend WithEvents timerBoincBlock As System.Windows.Forms.Timer
    Friend WithEvents lblCPUMinerElapsed As System.Windows.Forms.Label
    Friend WithEvents Label5 As System.Windows.Forms.Label
    Friend WithEvents lblLastBlockHash As System.Windows.Forms.Label
    Friend WithEvents Label6 As System.Windows.Forms.Label
    Friend WithEvents Tc1 As System.Windows.Forms.TabControl
    Friend WithEvents TabPage1 As System.Windows.Forms.TabPage
    Friend WithEvents TabPage2 As System.Windows.Forms.TabPage
    Friend WithEvents lblAccepted As System.Windows.Forms.Label
    Friend WithEvents Label7 As System.Windows.Forms.Label
    Friend WithEvents lblGPUMhs As System.Windows.Forms.Label
    Friend WithEvents Label9 As System.Windows.Forms.Label
    Friend WithEvents lblStale As System.Windows.Forms.Label
    Friend WithEvents Label11 As System.Windows.Forms.Label
    Friend WithEvents lblInvalid As System.Windows.Forms.Label
    Friend WithEvents Label13 As System.Windows.Forms.Label
    Friend WithEvents TabPage3 As System.Windows.Forms.TabPage
    Friend WithEvents TabPage4 As System.Windows.Forms.TabPage
    Friend WithEvents timerReaper As System.Windows.Forms.Timer
End Class
