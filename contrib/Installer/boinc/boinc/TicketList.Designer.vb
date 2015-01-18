<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmTicketList
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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmTicketList))
        Me.Label3 = New System.Windows.Forms.Label()
        Me.cmbFilter = New System.Windows.Forms.ComboBox()
        Me.tvTicketHistory = New System.Windows.Forms.TreeView()
        Me.btnAdd = New System.Windows.Forms.Button()
        Me.SuspendLayout()
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.ForeColor = System.Drawing.Color.Lime
        Me.Label3.Location = New System.Drawing.Point(12, 16)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(90, 13)
        Me.Label3.TabIndex = 5
        Me.Label3.Text = "Show Tickets for:"
        '
        'cmbFilter
        '
        Me.cmbFilter.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.cmbFilter.ForeColor = System.Drawing.Color.Lime
        Me.cmbFilter.FormattingEnabled = True
        Me.cmbFilter.Location = New System.Drawing.Point(108, 8)
        Me.cmbFilter.Name = "cmbFilter"
        Me.cmbFilter.Size = New System.Drawing.Size(302, 21)
        Me.cmbFilter.TabIndex = 14
        '
        'tvTicketHistory
        '
        Me.tvTicketHistory.Anchor = CType((((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Bottom) _
            Or System.Windows.Forms.AnchorStyles.Left) _
            Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.tvTicketHistory.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.tvTicketHistory.ForeColor = System.Drawing.Color.Lime
        Me.tvTicketHistory.Location = New System.Drawing.Point(12, 47)
        Me.tvTicketHistory.Name = "tvTicketHistory"
        Me.tvTicketHistory.Size = New System.Drawing.Size(878, 607)
        Me.tvTicketHistory.TabIndex = 16
        '
        'btnAdd
        '
        Me.btnAdd.BackColor = System.Drawing.Color.Gray
        Me.btnAdd.ForeColor = System.Drawing.Color.Lime
        Me.btnAdd.Location = New System.Drawing.Point(430, 8)
        Me.btnAdd.Name = "btnAdd"
        Me.btnAdd.Size = New System.Drawing.Size(78, 24)
        Me.btnAdd.TabIndex = 15
        Me.btnAdd.Text = "Add"
        Me.btnAdd.UseVisualStyleBackColor = False
        '
        'frmTicketList
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(902, 666)
        Me.Controls.Add(Me.tvTicketHistory)
        Me.Controls.Add(Me.btnAdd)
        Me.Controls.Add(Me.cmbFilter)
        Me.Controls.Add(Me.Label3)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmTicketList"
        Me.Text = "Gridcoin - Ticket List"
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents cmbFilter As System.Windows.Forms.ComboBox
    Private WithEvents tvTicketHistory As System.Windows.Forms.TreeView
    Friend WithEvents btnAdd As System.Windows.Forms.Button
End Class
