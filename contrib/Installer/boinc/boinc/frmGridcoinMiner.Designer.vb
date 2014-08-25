<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmGridcoinMiner
    Inherits System.Windows.Forms.Form

    'Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()> _
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        If disposing AndAlso components IsNot Nothing Then
            components.Dispose()
        End If
        MyBase.Dispose(disposing)
    End Sub

    'Required by the Windows Form Designer
    Private components As System.ComponentModel.IContainer

    'NOTE: The following procedure is required by the Windows Form Designer
    'It can be modified using the Windows Form Designer.  
    'Do not modify it using the code editor.
    <System.Diagnostics.DebuggerStepThrough()> _
    Private Sub InitializeComponent()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmGridcoinMiner))
        Me.txt_gpu_thread_concurrency = New System.Windows.Forms.TextBox()
        Me.Label1 = New System.Windows.Forms.Label()
        Me.txt_worksize = New System.Windows.Forms.TextBox()
        Me.Label2 = New System.Windows.Forms.Label()
        Me.txt_intensity = New System.Windows.Forms.TextBox()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.txt_lookup_gap = New System.Windows.Forms.TextBox()
        Me.Label4 = New System.Windows.Forms.Label()
        Me.btnSave = New System.Windows.Forms.Button()
        Me.GroupBox1 = New System.Windows.Forms.GroupBox()
        Me.btnDownloadCgminer = New System.Windows.Forms.Button()
        Me.btnCreateCgminerInstance = New System.Windows.Forms.Button()
        Me.txt_enabled = New System.Windows.Forms.CheckBox()
        Me.Label7 = New System.Windows.Forms.Label()
        Me.cmbDefaults = New System.Windows.Forms.ComboBox()
        Me.btnRefresh = New System.Windows.Forms.Button()
        Me.lblDeviceName = New System.Windows.Forms.Label()
        Me.cmbDeviceID = New System.Windows.Forms.ComboBox()
        Me.Label6 = New System.Windows.Forms.Label()
        Me.Label5 = New System.Windows.Forms.Label()
        Me.GroupBox1.SuspendLayout()
        Me.SuspendLayout()
        '
        'txt_gpu_thread_concurrency
        '
        Me.txt_gpu_thread_concurrency.BackColor = System.Drawing.Color.Black
        Me.txt_gpu_thread_concurrency.ForeColor = System.Drawing.Color.Lime
        Me.txt_gpu_thread_concurrency.Location = New System.Drawing.Point(184, 77)
        Me.txt_gpu_thread_concurrency.Name = "txt_gpu_thread_concurrency"
        Me.txt_gpu_thread_concurrency.Size = New System.Drawing.Size(171, 20)
        Me.txt_gpu_thread_concurrency.TabIndex = 0
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.BackColor = System.Drawing.Color.Black
        Me.Label1.ForeColor = System.Drawing.Color.Lime
        Me.Label1.Location = New System.Drawing.Point(25, 84)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(130, 13)
        Me.Label1.TabIndex = 1
        Me.Label1.Text = "GPU Thread Concurrency"
        '
        'txt_worksize
        '
        Me.txt_worksize.BackColor = System.Drawing.Color.Black
        Me.txt_worksize.ForeColor = System.Drawing.Color.Lime
        Me.txt_worksize.Location = New System.Drawing.Point(184, 104)
        Me.txt_worksize.Name = "txt_worksize"
        Me.txt_worksize.Size = New System.Drawing.Size(171, 20)
        Me.txt_worksize.TabIndex = 2
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.BackColor = System.Drawing.Color.Black
        Me.Label2.ForeColor = System.Drawing.Color.Lime
        Me.Label2.Location = New System.Drawing.Point(25, 111)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(51, 13)
        Me.Label2.TabIndex = 3
        Me.Label2.Text = "Worksize"
        '
        'txt_intensity
        '
        Me.txt_intensity.BackColor = System.Drawing.Color.Black
        Me.txt_intensity.ForeColor = System.Drawing.Color.Lime
        Me.txt_intensity.Location = New System.Drawing.Point(184, 129)
        Me.txt_intensity.Name = "txt_intensity"
        Me.txt_intensity.Size = New System.Drawing.Size(171, 20)
        Me.txt_intensity.TabIndex = 4
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.BackColor = System.Drawing.Color.Black
        Me.Label3.ForeColor = System.Drawing.Color.Lime
        Me.Label3.Location = New System.Drawing.Point(25, 136)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(46, 13)
        Me.Label3.TabIndex = 5
        Me.Label3.Text = "Intensity"
        '
        'txt_lookup_gap
        '
        Me.txt_lookup_gap.BackColor = System.Drawing.Color.Black
        Me.txt_lookup_gap.ForeColor = System.Drawing.Color.Lime
        Me.txt_lookup_gap.Location = New System.Drawing.Point(184, 159)
        Me.txt_lookup_gap.Name = "txt_lookup_gap"
        Me.txt_lookup_gap.Size = New System.Drawing.Size(171, 20)
        Me.txt_lookup_gap.TabIndex = 6
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.BackColor = System.Drawing.Color.Black
        Me.Label4.ForeColor = System.Drawing.Color.Lime
        Me.Label4.Location = New System.Drawing.Point(25, 162)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(66, 13)
        Me.Label4.TabIndex = 7
        Me.Label4.Text = "Lookup Gap"
        '
        'btnSave
        '
        Me.btnSave.BackColor = System.Drawing.Color.Black
        Me.btnSave.ForeColor = System.Drawing.Color.Lime
        Me.btnSave.Location = New System.Drawing.Point(19, 285)
        Me.btnSave.Name = "btnSave"
        Me.btnSave.Size = New System.Drawing.Size(95, 32)
        Me.btnSave.TabIndex = 10
        Me.btnSave.Text = "Save"
        Me.btnSave.UseVisualStyleBackColor = False
        '
        'GroupBox1
        '
        Me.GroupBox1.BackColor = System.Drawing.Color.Black
        Me.GroupBox1.Controls.Add(Me.btnDownloadCgminer)
        Me.GroupBox1.Controls.Add(Me.btnCreateCgminerInstance)
        Me.GroupBox1.Controls.Add(Me.txt_enabled)
        Me.GroupBox1.Controls.Add(Me.Label7)
        Me.GroupBox1.Controls.Add(Me.cmbDefaults)
        Me.GroupBox1.Controls.Add(Me.btnRefresh)
        Me.GroupBox1.Controls.Add(Me.lblDeviceName)
        Me.GroupBox1.Controls.Add(Me.cmbDeviceID)
        Me.GroupBox1.Controls.Add(Me.Label6)
        Me.GroupBox1.Controls.Add(Me.Label5)
        Me.GroupBox1.Controls.Add(Me.btnSave)
        Me.GroupBox1.Controls.Add(Me.Label4)
        Me.GroupBox1.Controls.Add(Me.txt_lookup_gap)
        Me.GroupBox1.Controls.Add(Me.Label3)
        Me.GroupBox1.Controls.Add(Me.txt_intensity)
        Me.GroupBox1.Controls.Add(Me.Label2)
        Me.GroupBox1.Controls.Add(Me.txt_worksize)
        Me.GroupBox1.Controls.Add(Me.Label1)
        Me.GroupBox1.Controls.Add(Me.txt_gpu_thread_concurrency)
        Me.GroupBox1.ForeColor = System.Drawing.Color.Lime
        Me.GroupBox1.Location = New System.Drawing.Point(25, 36)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(777, 328)
        Me.GroupBox1.TabIndex = 3
        Me.GroupBox1.TabStop = False
        Me.GroupBox1.Text = "Miner Settings"
        '
        'btnDownloadCgminer
        '
        Me.btnDownloadCgminer.BackColor = System.Drawing.Color.Black
        Me.btnDownloadCgminer.ForeColor = System.Drawing.Color.Lime
        Me.btnDownloadCgminer.Location = New System.Drawing.Point(382, 285)
        Me.btnDownloadCgminer.Name = "btnDownloadCgminer"
        Me.btnDownloadCgminer.Size = New System.Drawing.Size(196, 32)
        Me.btnDownloadCgminer.TabIndex = 20
        Me.btnDownloadCgminer.Text = "Download Kalroth-CGminer 3.7.2"
        Me.btnDownloadCgminer.UseVisualStyleBackColor = False
        '
        'btnCreateCgminerInstance
        '
        Me.btnCreateCgminerInstance.BackColor = System.Drawing.Color.Black
        Me.btnCreateCgminerInstance.ForeColor = System.Drawing.Color.Lime
        Me.btnCreateCgminerInstance.Location = New System.Drawing.Point(601, 285)
        Me.btnCreateCgminerInstance.Name = "btnCreateCgminerInstance"
        Me.btnCreateCgminerInstance.Size = New System.Drawing.Size(140, 32)
        Me.btnCreateCgminerInstance.TabIndex = 19
        Me.btnCreateCgminerInstance.Text = "Create CG Miner Instance"
        Me.btnCreateCgminerInstance.UseVisualStyleBackColor = False
        '
        'txt_enabled
        '
        Me.txt_enabled.AutoSize = True
        Me.txt_enabled.Location = New System.Drawing.Point(464, 23)
        Me.txt_enabled.Name = "txt_enabled"
        Me.txt_enabled.Size = New System.Drawing.Size(65, 17)
        Me.txt_enabled.TabIndex = 18
        Me.txt_enabled.Text = "Enabled"
        Me.txt_enabled.UseVisualStyleBackColor = True
        '
        'Label7
        '
        Me.Label7.AutoSize = True
        Me.Label7.BackColor = System.Drawing.Color.Black
        Me.Label7.ForeColor = System.Drawing.Color.Lime
        Me.Label7.Location = New System.Drawing.Point(23, 240)
        Me.Label7.Name = "Label7"
        Me.Label7.Size = New System.Drawing.Size(91, 13)
        Me.Label7.TabIndex = 17
        Me.Label7.Text = "Load Defaults for:"
        '
        'cmbDefaults
        '
        Me.cmbDefaults.BackColor = System.Drawing.Color.Black
        Me.cmbDefaults.ForeColor = System.Drawing.Color.Lime
        Me.cmbDefaults.FormattingEnabled = True
        Me.cmbDefaults.Location = New System.Drawing.Point(184, 232)
        Me.cmbDefaults.Name = "cmbDefaults"
        Me.cmbDefaults.Size = New System.Drawing.Size(171, 21)
        Me.cmbDefaults.TabIndex = 16
        '
        'btnRefresh
        '
        Me.btnRefresh.BackColor = System.Drawing.Color.Black
        Me.btnRefresh.ForeColor = System.Drawing.Color.Lime
        Me.btnRefresh.Location = New System.Drawing.Point(260, 285)
        Me.btnRefresh.Name = "btnRefresh"
        Me.btnRefresh.Size = New System.Drawing.Size(95, 32)
        Me.btnRefresh.TabIndex = 15
        Me.btnRefresh.Text = "Refresh"
        Me.btnRefresh.UseVisualStyleBackColor = False
        '
        'lblDeviceName
        '
        Me.lblDeviceName.AutoSize = True
        Me.lblDeviceName.BackColor = System.Drawing.Color.Black
        Me.lblDeviceName.ForeColor = System.Drawing.Color.Lime
        Me.lblDeviceName.Location = New System.Drawing.Point(181, 51)
        Me.lblDeviceName.Name = "lblDeviceName"
        Me.lblDeviceName.Size = New System.Drawing.Size(30, 13)
        Me.lblDeviceName.TabIndex = 14
        Me.lblDeviceName.Text = "GPU"
        '
        'cmbDeviceID
        '
        Me.cmbDeviceID.BackColor = System.Drawing.Color.Black
        Me.cmbDeviceID.ForeColor = System.Drawing.Color.Lime
        Me.cmbDeviceID.FormattingEnabled = True
        Me.cmbDeviceID.Location = New System.Drawing.Point(184, 21)
        Me.cmbDeviceID.Name = "cmbDeviceID"
        Me.cmbDeviceID.Size = New System.Drawing.Size(254, 21)
        Me.cmbDeviceID.TabIndex = 13
        '
        'Label6
        '
        Me.Label6.AutoSize = True
        Me.Label6.BackColor = System.Drawing.Color.Black
        Me.Label6.ForeColor = System.Drawing.Color.Lime
        Me.Label6.Location = New System.Drawing.Point(25, 51)
        Me.Label6.Name = "Label6"
        Me.Label6.Size = New System.Drawing.Size(75, 13)
        Me.Label6.TabIndex = 12
        Me.Label6.Text = "Device Name:"
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.BackColor = System.Drawing.Color.Black
        Me.Label5.ForeColor = System.Drawing.Color.Lime
        Me.Label5.Location = New System.Drawing.Point(25, 29)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(58, 13)
        Me.Label5.TabIndex = 11
        Me.Label5.Text = "Device ID:"
        '
        'frmGridcoinMiner
        '
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(836, 432)
        Me.Controls.Add(Me.GroupBox1)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmGridcoinMiner"
        Me.Text = "Gridcoin GPU Configuration Management 2.0"
        Me.GroupBox1.ResumeLayout(False)
        Me.GroupBox1.PerformLayout()
        Me.ResumeLayout(False)

    End Sub
    Friend WithEvents txt_gpu_thread_concurrency As System.Windows.Forms.TextBox
    Friend WithEvents Label1 As System.Windows.Forms.Label
    Friend WithEvents txt_worksize As System.Windows.Forms.TextBox
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents txt_intensity As System.Windows.Forms.TextBox
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents txt_lookup_gap As System.Windows.Forms.TextBox
    Friend WithEvents Label4 As System.Windows.Forms.Label
    Friend WithEvents btnSave As System.Windows.Forms.Button
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents Label6 As System.Windows.Forms.Label
    Friend WithEvents Label5 As System.Windows.Forms.Label
    Friend WithEvents lblDeviceName As System.Windows.Forms.Label
    Friend WithEvents cmbDeviceID As System.Windows.Forms.ComboBox
    Friend WithEvents btnRefresh As System.Windows.Forms.Button
    Friend WithEvents Label7 As System.Windows.Forms.Label
    Friend WithEvents cmbDefaults As System.Windows.Forms.ComboBox
    Friend WithEvents txt_enabled As System.Windows.Forms.CheckBox
    Friend WithEvents btnCreateCgminerInstance As System.Windows.Forms.Button
    Friend WithEvents btnDownloadCgminer As System.Windows.Forms.Button

End Class
