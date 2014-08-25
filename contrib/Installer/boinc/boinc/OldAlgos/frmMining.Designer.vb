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
        CType(Me.PictureBox1, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.SuspendLayout()
        '
        'btnRefresh
        '
        Me.btnRefresh.Location = New System.Drawing.Point(11, 665)
        Me.btnRefresh.Name = "btnRefresh"
        Me.btnRefresh.Size = New System.Drawing.Size(118, 35)
        Me.btnRefresh.TabIndex = 0
        Me.btnRefresh.Text = "Refresh"
        Me.btnRefresh.UseVisualStyleBackColor = True
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
        ChartArea1.Name = "ChartArea1"
        Me.Chart1.ChartAreas.Add(ChartArea1)
        Legend1.Name = "Legend1"
        Me.Chart1.Legends.Add(Legend1)
        Me.Chart1.Location = New System.Drawing.Point(11, 401)
        Me.Chart1.Name = "Chart1"
        Series1.ChartArea = "ChartArea1"
        Series1.Legend = "Legend1"
        Series1.Name = "Series1"
        Me.Chart1.Series.Add(Series1)
        Me.Chart1.Size = New System.Drawing.Size(917, 128)
        Me.Chart1.TabIndex = 2
        Me.Chart1.Text = "Boinc Utilization"
        '
        'ChartUtilization
        '
        ChartArea2.Name = "ChartArea1"
        Me.ChartUtilization.ChartAreas.Add(ChartArea2)
        Legend2.Name = "Legend1"
        Me.ChartUtilization.Legends.Add(Legend2)
        Me.ChartUtilization.Location = New System.Drawing.Point(11, 535)
        Me.ChartUtilization.Name = "ChartUtilization"
        Series2.ChartArea = "ChartArea1"
        Series2.Legend = "Legend1"
        Series2.Name = "Series1"
        Me.ChartUtilization.Series.Add(Series2)
        Me.ChartUtilization.Size = New System.Drawing.Size(242, 124)
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
        Me.lblThanks.Location = New System.Drawing.Point(8, 715)
        Me.lblThanks.Name = "lblThanks"
        Me.lblThanks.Size = New System.Drawing.Size(43, 13)
        Me.lblThanks.TabIndex = 8
        Me.lblThanks.Text = "Thanks"
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
        Me.lblVersion.Location = New System.Drawing.Point(399, 542)
        Me.lblVersion.Name = "lblVersion"
        Me.lblVersion.Size = New System.Drawing.Size(15, 16)
        Me.lblVersion.TabIndex = 12
        Me.lblVersion.Text = "0"
        '
        'lblMD5
        '
        Me.lblMD5.AutoSize = True
        Me.lblMD5.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblMD5.Location = New System.Drawing.Point(576, 625)
        Me.lblMD5.Name = "lblMD5"
        Me.lblMD5.Size = New System.Drawing.Size(15, 16)
        Me.lblMD5.TabIndex = 14
        Me.lblMD5.Text = "0"
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label2.Location = New System.Drawing.Point(494, 625)
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
        Me.btnRestartMiner.Location = New System.Drawing.Point(135, 665)
        Me.btnRestartMiner.Name = "btnRestartMiner"
        Me.btnRestartMiner.Size = New System.Drawing.Size(118, 35)
        Me.btnRestartMiner.TabIndex = 15
        Me.btnRestartMiner.Text = "Restart Miner"
        Me.btnRestartMiner.UseVisualStyleBackColor = True
        '
        'lblRestartMiner
        '
        Me.lblRestartMiner.AutoSize = True
        Me.lblRestartMiner.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblRestartMiner.Location = New System.Drawing.Point(399, 580)
        Me.lblRestartMiner.Name = "lblRestartMiner"
        Me.lblRestartMiner.Size = New System.Drawing.Size(15, 16)
        Me.lblRestartMiner.TabIndex = 17
        Me.lblRestartMiner.Text = "0"
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label3.Location = New System.Drawing.Point(259, 580)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(134, 16)
        Me.Label3.TabIndex = 16
        Me.Label3.Text = "Restart Miner In Mins:"
        '
        'btnRestart
        '
        Me.btnRestart.Location = New System.Drawing.Point(259, 665)
        Me.btnRestart.Name = "btnRestart"
        Me.btnRestart.Size = New System.Drawing.Size(118, 35)
        Me.btnRestart.TabIndex = 18
        Me.btnRestart.Text = "Restart Wallet"
        Me.btnRestart.UseVisualStyleBackColor = True
        '
        'lblRestartWallet
        '
        Me.lblRestartWallet.AutoSize = True
        Me.lblRestartWallet.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblRestartWallet.Location = New System.Drawing.Point(399, 611)
        Me.lblRestartWallet.Name = "lblRestartWallet"
        Me.lblRestartWallet.Size = New System.Drawing.Size(15, 16)
        Me.lblRestartWallet.TabIndex = 21
        Me.lblRestartWallet.Text = "0"
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label4.Location = New System.Drawing.Point(259, 611)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(139, 16)
        Me.Label4.TabIndex = 20
        Me.Label4.Text = "Restart Wallet In Mins:"
        '
        'btnClose
        '
        Me.btnClose.Location = New System.Drawing.Point(383, 665)
        Me.btnClose.Name = "btnClose"
        Me.btnClose.Size = New System.Drawing.Size(118, 35)
        Me.btnClose.TabIndex = 22
        Me.btnClose.Text = "Close Miner"
        Me.btnClose.UseVisualStyleBackColor = True
        '
        'btnHide
        '
        Me.btnHide.Location = New System.Drawing.Point(507, 665)
        Me.btnHide.Name = "btnHide"
        Me.btnHide.Size = New System.Drawing.Size(118, 35)
        Me.btnHide.TabIndex = 23
        Me.btnHide.Text = "Hide"
        Me.btnHide.UseVisualStyleBackColor = True
        '
        'btnCloseWallet
        '
        Me.btnCloseWallet.Location = New System.Drawing.Point(631, 665)
        Me.btnCloseWallet.Name = "btnCloseWallet"
        Me.btnCloseWallet.Size = New System.Drawing.Size(118, 35)
        Me.btnCloseWallet.TabIndex = 24
        Me.btnCloseWallet.Text = "Close Wallet"
        Me.btnCloseWallet.UseVisualStyleBackColor = True
        '
        'frmMining
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.ClientSize = New System.Drawing.Size(940, 729)
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
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmMining"
        Me.Text = "Gridcoin Mining Module"
        CType(Me.PictureBox1, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.Chart1, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.ChartUtilization, System.ComponentModel.ISupportInitialize).EndInit()
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
End Class
