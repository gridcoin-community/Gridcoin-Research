<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmAddAttachment
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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmAddAttachment))
        Me.btnAddAttachment = New System.Windows.Forms.Button()
        Me.txtAttachment = New System.Windows.Forms.TextBox()
        Me.Label9 = New System.Windows.Forms.Label()
        Me.lblMode = New System.Windows.Forms.Label()
        Me.btnOpen = New System.Windows.Forms.Button()
        Me.btnVirusScan = New System.Windows.Forms.Button()
        Me.SuspendLayout
        '
        'btnAddAttachment
        '
        Me.btnAddAttachment.BackColor = System.Drawing.Color.Black
        Me.btnAddAttachment.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128,Byte),Integer), CType(CType(255,Byte),Integer), CType(CType(128,Byte),Integer))
        Me.btnAddAttachment.Location = New System.Drawing.Point(530, 19)
        Me.btnAddAttachment.Name = "btnAddAttachment"
        Me.btnAddAttachment.Size = New System.Drawing.Size(177, 29)
        Me.btnAddAttachment.TabIndex = 17
        Me.btnAddAttachment.Text = "Add Attachment"
        Me.btnAddAttachment.UseVisualStyleBackColor = false
        '
        'txtAttachment
        '
        Me.txtAttachment.BackColor = System.Drawing.Color.FromArgb(CType(CType(0,Byte),Integer), CType(CType(64,Byte),Integer), CType(CType(0,Byte),Integer))
        Me.txtAttachment.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0,Byte))
        Me.txtAttachment.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255,Byte),Integer), CType(CType(255,Byte),Integer), CType(CType(128,Byte),Integer))
        Me.txtAttachment.Location = New System.Drawing.Point(94, 19)
        Me.txtAttachment.Name = "txtAttachment"
        Me.txtAttachment.ReadOnly = true
        Me.txtAttachment.Size = New System.Drawing.Size(415, 22)
        Me.txtAttachment.TabIndex = 18
        '
        'Label9
        '
        Me.Label9.AutoSize = true
        Me.Label9.ForeColor = System.Drawing.Color.Lime
        Me.Label9.Location = New System.Drawing.Point(12, 28)
        Me.Label9.Name = "Label9"
        Me.Label9.Size = New System.Drawing.Size(64, 13)
        Me.Label9.TabIndex = 19
        Me.Label9.Text = "Attachment:"
        '
        'lblMode
        '
        Me.lblMode.AutoSize = true
        Me.lblMode.Font = New System.Drawing.Font("Book Antiqua", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0,Byte))
        Me.lblMode.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255,Byte),Integer), CType(CType(128,Byte),Integer), CType(CType(128,Byte),Integer))
        Me.lblMode.Location = New System.Drawing.Point(767, 19)
        Me.lblMode.Name = "lblMode"
        Me.lblMode.Size = New System.Drawing.Size(70, 17)
        Me.lblMode.TabIndex = 29
        Me.lblMode.Text = "Add Mode"
        '
        'btnOpen
        '
        Me.btnOpen.BackColor = System.Drawing.Color.Black
        Me.btnOpen.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128,Byte),Integer), CType(CType(255,Byte),Integer), CType(CType(128,Byte),Integer))
        Me.btnOpen.Location = New System.Drawing.Point(530, 62)
        Me.btnOpen.Name = "btnOpen"
        Me.btnOpen.Size = New System.Drawing.Size(177, 29)
        Me.btnOpen.TabIndex = 30
        Me.btnOpen.Text = "Open Attachment"
        Me.btnOpen.UseVisualStyleBackColor = false
        '
        'btnVirusScan
        '
        Me.btnVirusScan.BackColor = System.Drawing.Color.Black
        Me.btnVirusScan.ForeColor = System.Drawing.Color.Red
        Me.btnVirusScan.Location = New System.Drawing.Point(725, 62)
        Me.btnVirusScan.Name = "btnVirusScan"
        Me.btnVirusScan.Size = New System.Drawing.Size(112, 29)
        Me.btnVirusScan.TabIndex = 31
        Me.btnVirusScan.Text = "Virus Scan Attchmnt"
        Me.btnVirusScan.UseVisualStyleBackColor = false
        '
        'frmAddAttachment
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6!, 13!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(853, 138)
        Me.Controls.Add(Me.btnVirusScan)
        Me.Controls.Add(Me.btnOpen)
        Me.Controls.Add(Me.lblMode)
        Me.Controls.Add(Me.txtAttachment)
        Me.Controls.Add(Me.Label9)
        Me.Controls.Add(Me.btnAddAttachment)
        Me.Icon = CType(resources.GetObject("$this.Icon"),System.Drawing.Icon)
        Me.Name = "frmAddAttachment"
        Me.Text = "Add Attachment to Transaction 1.0"
        Me.ResumeLayout(false)
        Me.PerformLayout

End Sub
    Friend WithEvents btnAddAttachment As System.Windows.Forms.Button
    Friend WithEvents txtAttachment As System.Windows.Forms.TextBox
    Friend WithEvents Label9 As System.Windows.Forms.Label
    Friend WithEvents lblMode As System.Windows.Forms.Label
    Friend WithEvents btnOpen As System.Windows.Forms.Button
    Friend WithEvents btnVirusScan As System.Windows.Forms.Button
End Class
