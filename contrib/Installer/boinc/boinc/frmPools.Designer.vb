<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmPools
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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmPools))
        Me.GroupBox1 = New System.Windows.Forms.GroupBox()
        Me.lnkPool1 = New System.Windows.Forms.LinkLabel()
        Me.btnSave = New System.Windows.Forms.Button()
        Me.GroupBox1.SuspendLayout()
        Me.SuspendLayout()
        '
        'GroupBox1
        '
        Me.GroupBox1.BackColor = System.Drawing.Color.Black
        Me.GroupBox1.Controls.Add(Me.lnkPool1)
        Me.GroupBox1.Controls.Add(Me.btnSave)
        Me.GroupBox1.ForeColor = System.Drawing.Color.Lime
        Me.GroupBox1.Location = New System.Drawing.Point(25, 36)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(661, 328)
        Me.GroupBox1.TabIndex = 3
        Me.GroupBox1.TabStop = False
        Me.GroupBox1.Text = "Pools:"
        '
        'lnkPool1
        '
        Me.lnkPool1.AutoSize = True
        Me.lnkPool1.LinkColor = System.Drawing.Color.Lime
        Me.lnkPool1.Location = New System.Drawing.Point(79, 32)
        Me.lnkPool1.Name = "lnkPool1"
        Me.lnkPool1.Size = New System.Drawing.Size(141, 13)
        Me.lnkPool1.TabIndex = 18
        Me.lnkPool1.TabStop = True
        Me.lnkPool1.Text = "1.     https://pool.gridcoin.us"
        '
        'btnSave
        '
        Me.btnSave.BackColor = System.Drawing.Color.Black
        Me.btnSave.ForeColor = System.Drawing.Color.Lime
        Me.btnSave.Location = New System.Drawing.Point(82, 285)
        Me.btnSave.Name = "btnSave"
        Me.btnSave.Size = New System.Drawing.Size(95, 32)
        Me.btnSave.TabIndex = 10
        Me.btnSave.Text = "Authenticate"
        Me.btnSave.UseVisualStyleBackColor = False
        '
        'frmPools
        '
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(715, 432)
        Me.Controls.Add(Me.GroupBox1)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmPools"
        Me.Text = "Boinc Compatible Pools"
        Me.GroupBox1.ResumeLayout(False)
        Me.GroupBox1.PerformLayout()
        Me.ResumeLayout(False)

    End Sub
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents btnSave As System.Windows.Forms.Button
    Friend WithEvents lnkPool1 As System.Windows.Forms.LinkLabel

End Class
