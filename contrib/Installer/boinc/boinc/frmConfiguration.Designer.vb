<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmConfiguration
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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmConfiguration))
        Me.GroupBox1 = New System.Windows.Forms.GroupBox()
        Me.chkSpeech = New System.Windows.Forms.CheckBox()
        Me.btnSave = New System.Windows.Forms.Button()
        Me.MenuStrip1 = New System.Windows.Forms.MenuStrip()
        Me.FileToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.HideToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.OptionalModulesToolStripMenuItem = New System.Windows.Forms.ToolStripMenuItem()
        Me.InstallGridcoinGalazaToolStripMenuItem1 = New System.Windows.Forms.ToolStripMenuItem()
        Me.lblTestnet = New System.Windows.Forms.Label()
        Me.GroupBox1.SuspendLayout()
        Me.MenuStrip1.SuspendLayout()
        Me.SuspendLayout()
        '
        'GroupBox1
        '
        Me.GroupBox1.BackColor = System.Drawing.Color.Black
        Me.GroupBox1.Controls.Add(Me.chkSpeech)
        Me.GroupBox1.ForeColor = System.Drawing.Color.Lime
        Me.GroupBox1.Location = New System.Drawing.Point(25, 36)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(661, 328)
        Me.GroupBox1.TabIndex = 3
        Me.GroupBox1.TabStop = False
        Me.GroupBox1.Text = "Configuration:"
        '
        'chkSpeech
        '
        Me.chkSpeech.AutoSize = True
        Me.chkSpeech.Location = New System.Drawing.Point(32, 23)
        Me.chkSpeech.Name = "chkSpeech"
        Me.chkSpeech.Size = New System.Drawing.Size(99, 17)
        Me.chkSpeech.TabIndex = 0
        Me.chkSpeech.Text = "Enable Speech"
        Me.chkSpeech.UseVisualStyleBackColor = True
        '
        'btnSave
        '
        Me.btnSave.BackColor = System.Drawing.Color.Black
        Me.btnSave.ForeColor = System.Drawing.Color.Lime
        Me.btnSave.Location = New System.Drawing.Point(25, 370)
        Me.btnSave.Name = "btnSave"
        Me.btnSave.Size = New System.Drawing.Size(95, 32)
        Me.btnSave.TabIndex = 10
        Me.btnSave.Text = "Save"
        Me.btnSave.UseVisualStyleBackColor = False
        '
        'MenuStrip1
        '
        Me.MenuStrip1.AllowItemReorder = True
        Me.MenuStrip1.BackColor = System.Drawing.Color.Transparent
        Me.MenuStrip1.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None
        Me.MenuStrip1.Items.AddRange(New System.Windows.Forms.ToolStripItem() {Me.OptionalModulesToolStripMenuItem, Me.FileToolStripMenuItem})
        Me.MenuStrip1.Location = New System.Drawing.Point(0, 0)
        Me.MenuStrip1.Name = "MenuStrip1"
        Me.MenuStrip1.Size = New System.Drawing.Size(715, 24)
        Me.MenuStrip1.TabIndex = 43
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
        Me.HideToolStripMenuItem.Size = New System.Drawing.Size(152, 22)
        Me.HideToolStripMenuItem.Text = "Hide"
        '
        'OptionalModulesToolStripMenuItem
        '
        Me.OptionalModulesToolStripMenuItem.BackColor = System.Drawing.Color.Black
        Me.OptionalModulesToolStripMenuItem.DropDownItems.AddRange(New System.Windows.Forms.ToolStripItem() {Me.InstallGridcoinGalazaToolStripMenuItem1})
        Me.OptionalModulesToolStripMenuItem.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.OptionalModulesToolStripMenuItem.Name = "OptionalModulesToolStripMenuItem"
        Me.OptionalModulesToolStripMenuItem.Size = New System.Drawing.Size(114, 20)
        Me.OptionalModulesToolStripMenuItem.Text = "Optional Modules"
        '
        'InstallGridcoinGalazaToolStripMenuItem1
        '
        Me.InstallGridcoinGalazaToolStripMenuItem1.BackColor = System.Drawing.Color.Black
        Me.InstallGridcoinGalazaToolStripMenuItem1.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.InstallGridcoinGalazaToolStripMenuItem1.Name = "InstallGridcoinGalazaToolStripMenuItem1"
        Me.InstallGridcoinGalazaToolStripMenuItem1.Size = New System.Drawing.Size(190, 22)
        Me.InstallGridcoinGalazaToolStripMenuItem1.Text = "Install Gridcoin Galaza"
        '
        'lblTestnet
        '
        Me.lblTestnet.AutoSize = True
        Me.lblTestnet.BackColor = System.Drawing.Color.Transparent
        Me.lblTestnet.Font = New System.Drawing.Font("Microsoft Sans Serif", 12.0!, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblTestnet.ForeColor = System.Drawing.Color.Red
        Me.lblTestnet.Location = New System.Drawing.Point(322, 13)
        Me.lblTestnet.Name = "lblTestnet"
        Me.lblTestnet.Size = New System.Drawing.Size(13, 20)
        Me.lblTestnet.TabIndex = 64
        Me.lblTestnet.Text = " "
        '
        'frmConfiguration
        '
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(715, 432)
        Me.Controls.Add(Me.lblTestnet)
        Me.Controls.Add(Me.MenuStrip1)
        Me.Controls.Add(Me.btnSave)
        Me.Controls.Add(Me.GroupBox1)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmConfiguration"
        Me.Text = "Gridcoin Advanced Configuration Options 1.0"
        Me.GroupBox1.ResumeLayout(False)
        Me.GroupBox1.PerformLayout()
        Me.MenuStrip1.ResumeLayout(False)
        Me.MenuStrip1.PerformLayout()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents btnSave As System.Windows.Forms.Button
    Friend WithEvents MenuStrip1 As System.Windows.Forms.MenuStrip
    Friend WithEvents FileToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents HideToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents OptionalModulesToolStripMenuItem As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents InstallGridcoinGalazaToolStripMenuItem1 As System.Windows.Forms.ToolStripMenuItem
    Friend WithEvents chkSpeech As System.Windows.Forms.CheckBox
    Friend WithEvents lblTestnet As System.Windows.Forms.Label

End Class
