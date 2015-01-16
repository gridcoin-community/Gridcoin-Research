<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmTicketAdd
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
        Me.Label1 = New System.Windows.Forms.Label()
        Me.cmbAssignedTo = New System.Windows.Forms.ComboBox()
        Me.Label2 = New System.Windows.Forms.Label()
        Me.txtDescription = New System.Windows.Forms.TextBox()
        Me.Label4 = New System.Windows.Forms.Label()
        Me.cmbDisposition = New System.Windows.Forms.ComboBox()
        Me.txtTicketId = New System.Windows.Forms.TextBox()
        Me.Label5 = New System.Windows.Forms.Label()
        Me.btnSubmit = New System.Windows.Forms.Button()
        Me.cmbType = New System.Windows.Forms.ComboBox()
        Me.Label7 = New System.Windows.Forms.Label()
        Me.rtbNotes = New System.Windows.Forms.RichTextBox()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.Label6 = New System.Windows.Forms.Label()
        Me.tvTicketHistory = New System.Windows.Forms.TreeView()
        Me.Label8 = New System.Windows.Forms.Label()
        Me.txtSubmittedBy = New System.Windows.Forms.TextBox()
        Me.btnAddAttachment = New System.Windows.Forms.Button()
        Me.SuspendLayout()
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.ForeColor = System.Drawing.Color.Lime
        Me.Label1.Location = New System.Drawing.Point(69, 93)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(69, 13)
        Me.Label1.TabIndex = 0
        Me.Label1.Text = "Assigned To:"
        '
        'cmbAssignedTo
        '
        Me.cmbAssignedTo.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.cmbAssignedTo.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cmbAssignedTo.ForeColor = System.Drawing.Color.Lime
        Me.cmbAssignedTo.FormattingEnabled = True
        Me.cmbAssignedTo.Location = New System.Drawing.Point(144, 85)
        Me.cmbAssignedTo.Name = "cmbAssignedTo"
        Me.cmbAssignedTo.Size = New System.Drawing.Size(313, 21)
        Me.cmbAssignedTo.TabIndex = 4
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.ForeColor = System.Drawing.Color.Lime
        Me.Label2.Location = New System.Drawing.Point(70, 146)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(63, 13)
        Me.Label2.TabIndex = 3
        Me.Label2.Text = "Description:"
        '
        'txtDescription
        '
        Me.txtDescription.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.txtDescription.ForeColor = System.Drawing.Color.Lime
        Me.txtDescription.Location = New System.Drawing.Point(145, 139)
        Me.txtDescription.Name = "txtDescription"
        Me.txtDescription.Size = New System.Drawing.Size(312, 20)
        Me.txtDescription.TabIndex = 6
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.ForeColor = System.Drawing.Color.Lime
        Me.Label4.Location = New System.Drawing.Point(69, 120)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(61, 13)
        Me.Label4.TabIndex = 6
        Me.Label4.Text = "Disposition:"
        '
        'cmbDisposition
        '
        Me.cmbDisposition.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.cmbDisposition.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cmbDisposition.ForeColor = System.Drawing.Color.Lime
        Me.cmbDisposition.FormattingEnabled = True
        Me.cmbDisposition.Location = New System.Drawing.Point(145, 112)
        Me.cmbDisposition.Name = "cmbDisposition"
        Me.cmbDisposition.Size = New System.Drawing.Size(313, 21)
        Me.cmbDisposition.TabIndex = 5
        '
        'txtTicketId
        '
        Me.txtTicketId.BackColor = System.Drawing.Color.Teal
        Me.txtTicketId.ForeColor = System.Drawing.Color.Yellow
        Me.txtTicketId.Location = New System.Drawing.Point(145, 8)
        Me.txtTicketId.Name = "txtTicketId"
        Me.txtTicketId.ReadOnly = True
        Me.txtTicketId.Size = New System.Drawing.Size(312, 20)
        Me.txtTicketId.TabIndex = 1
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.ForeColor = System.Drawing.Color.Lime
        Me.Label5.Location = New System.Drawing.Point(70, 17)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(54, 13)
        Me.Label5.TabIndex = 8
        Me.Label5.Text = "Ticket ID:"
        '
        'btnSubmit
        '
        Me.btnSubmit.BackColor = System.Drawing.Color.Gray
        Me.btnSubmit.ForeColor = System.Drawing.Color.Lime
        Me.btnSubmit.Location = New System.Drawing.Point(464, 130)
        Me.btnSubmit.Name = "btnSubmit"
        Me.btnSubmit.Size = New System.Drawing.Size(78, 29)
        Me.btnSubmit.TabIndex = 8
        Me.btnSubmit.Text = "Submit"
        Me.btnSubmit.UseVisualStyleBackColor = False
        '
        'cmbType
        '
        Me.cmbType.BackColor = System.Drawing.Color.Teal
        Me.cmbType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cmbType.ForeColor = System.Drawing.Color.Lime
        Me.cmbType.FormattingEnabled = True
        Me.cmbType.Location = New System.Drawing.Point(144, 58)
        Me.cmbType.Name = "cmbType"
        Me.cmbType.Size = New System.Drawing.Size(313, 21)
        Me.cmbType.TabIndex = 3
        '
        'Label7
        '
        Me.Label7.AutoSize = True
        Me.Label7.ForeColor = System.Drawing.Color.Lime
        Me.Label7.Location = New System.Drawing.Point(70, 66)
        Me.Label7.Name = "Label7"
        Me.Label7.Size = New System.Drawing.Size(34, 13)
        Me.Label7.TabIndex = 14
        Me.Label7.Text = "Type:"
        '
        'rtbNotes
        '
        Me.rtbNotes.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.rtbNotes.ForeColor = System.Drawing.Color.Lime
        Me.rtbNotes.Location = New System.Drawing.Point(73, 208)
        Me.rtbNotes.Name = "rtbNotes"
        Me.rtbNotes.Size = New System.Drawing.Size(744, 250)
        Me.rtbNotes.TabIndex = 7
        Me.rtbNotes.Text = ""
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.ForeColor = System.Drawing.Color.Lime
        Me.Label3.Location = New System.Drawing.Point(76, 192)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(38, 13)
        Me.Label3.TabIndex = 5
        Me.Label3.Text = "Notes:"
        '
        'Label6
        '
        Me.Label6.AutoSize = True
        Me.Label6.ForeColor = System.Drawing.Color.Lime
        Me.Label6.Location = New System.Drawing.Point(70, 470)
        Me.Label6.Name = "Label6"
        Me.Label6.Size = New System.Drawing.Size(42, 13)
        Me.Label6.TabIndex = 11
        Me.Label6.Text = "History:"
        '
        'tvTicketHistory
        '
        Me.tvTicketHistory.Anchor = CType((((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Bottom) _
            Or System.Windows.Forms.AnchorStyles.Left) _
            Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.tvTicketHistory.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.tvTicketHistory.ForeColor = System.Drawing.Color.Lime
        Me.tvTicketHistory.Location = New System.Drawing.Point(72, 486)
        Me.tvTicketHistory.Name = "tvTicketHistory"
        Me.tvTicketHistory.Size = New System.Drawing.Size(745, 147)
        Me.tvTicketHistory.TabIndex = 12
        '
        'Label8
        '
        Me.Label8.AutoSize = True
        Me.Label8.ForeColor = System.Drawing.Color.Lime
        Me.Label8.Location = New System.Drawing.Point(70, 42)
        Me.Label8.Name = "Label8"
        Me.Label8.Size = New System.Drawing.Size(72, 13)
        Me.Label8.TabIndex = 16
        Me.Label8.Text = "Submitted By:"
        '
        'txtSubmittedBy
        '
        Me.txtSubmittedBy.BackColor = System.Drawing.Color.Teal
        Me.txtSubmittedBy.ForeColor = System.Drawing.Color.Yellow
        Me.txtSubmittedBy.Location = New System.Drawing.Point(145, 34)
        Me.txtSubmittedBy.Name = "txtSubmittedBy"
        Me.txtSubmittedBy.ReadOnly = True
        Me.txtSubmittedBy.Size = New System.Drawing.Size(312, 20)
        Me.txtSubmittedBy.TabIndex = 2
        '
        'btnAddAttachment
        '
        Me.btnAddAttachment.BackColor = System.Drawing.Color.Gray
        Me.btnAddAttachment.ForeColor = System.Drawing.Color.Lime
        Me.btnAddAttachment.Location = New System.Drawing.Point(548, 130)
        Me.btnAddAttachment.Name = "btnAddAttachment"
        Me.btnAddAttachment.Size = New System.Drawing.Size(112, 29)
        Me.btnAddAttachment.TabIndex = 17
        Me.btnAddAttachment.Text = "Add Attachment"
        Me.btnAddAttachment.UseVisualStyleBackColor = False
        '
        'frmTicketAdd
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(937, 666)
        Me.Controls.Add(Me.btnAddAttachment)
        Me.Controls.Add(Me.txtSubmittedBy)
        Me.Controls.Add(Me.Label8)
        Me.Controls.Add(Me.cmbType)
        Me.Controls.Add(Me.Label7)
        Me.Controls.Add(Me.tvTicketHistory)
        Me.Controls.Add(Me.Label6)
        Me.Controls.Add(Me.btnSubmit)
        Me.Controls.Add(Me.txtTicketId)
        Me.Controls.Add(Me.Label5)
        Me.Controls.Add(Me.cmbDisposition)
        Me.Controls.Add(Me.Label4)
        Me.Controls.Add(Me.Label3)
        Me.Controls.Add(Me.txtDescription)
        Me.Controls.Add(Me.Label2)
        Me.Controls.Add(Me.rtbNotes)
        Me.Controls.Add(Me.cmbAssignedTo)
        Me.Controls.Add(Me.Label1)
        Me.Name = "frmTicketAdd"
        Me.Text = "Gridcoin - Add or Edit Ticket"
        Me.ResumeLayout(false)
        Me.PerformLayout

End Sub
    Friend WithEvents Label1 As System.Windows.Forms.Label
    Friend WithEvents cmbAssignedTo As System.Windows.Forms.ComboBox
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents txtDescription As System.Windows.Forms.TextBox
    Friend WithEvents Label4 As System.Windows.Forms.Label
    Friend WithEvents cmbDisposition As System.Windows.Forms.ComboBox
    Friend WithEvents txtTicketId As System.Windows.Forms.TextBox
    Friend WithEvents Label5 As System.Windows.Forms.Label
    Friend WithEvents btnSubmit As System.Windows.Forms.Button
    Friend WithEvents cmbType As System.Windows.Forms.ComboBox
    Friend WithEvents Label7 As System.Windows.Forms.Label
    Friend WithEvents rtbNotes As System.Windows.Forms.RichTextBox
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents Label6 As System.Windows.Forms.Label
    Private WithEvents tvTicketHistory As System.Windows.Forms.TreeView
    Friend WithEvents Label8 As System.Windows.Forms.Label
    Friend WithEvents txtSubmittedBy As System.Windows.Forms.TextBox
    Friend WithEvents btnAddAttachment As System.Windows.Forms.Button
End Class
