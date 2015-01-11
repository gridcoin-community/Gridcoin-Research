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
        Me.rtbNotes = New System.Windows.Forms.RichTextBox()
        Me.Label2 = New System.Windows.Forms.Label()
        Me.TextBox1 = New System.Windows.Forms.TextBox()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.Label4 = New System.Windows.Forms.Label()
        Me.cmbDisposition = New System.Windows.Forms.ComboBox()
        Me.txtTicketId = New System.Windows.Forms.TextBox()
        Me.Label5 = New System.Windows.Forms.Label()
        Me.btnSubmit = New System.Windows.Forms.Button()
        Me.Label6 = New System.Windows.Forms.Label()
        Me.listMessages = New System.Windows.Forms.TreeView()
        Me.SuspendLayout()
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.ForeColor = System.Drawing.Color.Lime
        Me.Label1.Location = New System.Drawing.Point(71, 47)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(69, 13)
        Me.Label1.TabIndex = 0
        Me.Label1.Text = "Assigned To:"
        '
        'cmbAssignedTo
        '
        Me.cmbAssignedTo.BackColor = System.Drawing.Color.Silver
        Me.cmbAssignedTo.ForeColor = System.Drawing.Color.Yellow
        Me.cmbAssignedTo.FormattingEnabled = True
        Me.cmbAssignedTo.Location = New System.Drawing.Point(146, 39)
        Me.cmbAssignedTo.Name = "cmbAssignedTo"
        Me.cmbAssignedTo.Size = New System.Drawing.Size(313, 21)
        Me.cmbAssignedTo.TabIndex = 1
        '
        'rtbNotes
        '
        Me.rtbNotes.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.rtbNotes.ForeColor = System.Drawing.Color.Lime
        Me.rtbNotes.Location = New System.Drawing.Point(74, 173)
        Me.rtbNotes.Name = "rtbNotes"
        Me.rtbNotes.Size = New System.Drawing.Size(744, 250)
        Me.rtbNotes.TabIndex = 2
        Me.rtbNotes.Text = ""
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.ForeColor = System.Drawing.Color.Lime
        Me.Label2.Location = New System.Drawing.Point(71, 118)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(63, 13)
        Me.Label2.TabIndex = 3
        Me.Label2.Text = "Description:"
        '
        'TextBox1
        '
        Me.TextBox1.BackColor = System.Drawing.Color.Silver
        Me.TextBox1.ForeColor = System.Drawing.Color.Yellow
        Me.TextBox1.Location = New System.Drawing.Point(146, 111)
        Me.TextBox1.Name = "TextBox1"
        Me.TextBox1.Size = New System.Drawing.Size(312, 20)
        Me.TextBox1.TabIndex = 4
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.ForeColor = System.Drawing.Color.Lime
        Me.Label3.Location = New System.Drawing.Point(77, 157)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(38, 13)
        Me.Label3.TabIndex = 5
        Me.Label3.Text = "Notes:"
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.ForeColor = System.Drawing.Color.Lime
        Me.Label4.Location = New System.Drawing.Point(71, 79)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(61, 13)
        Me.Label4.TabIndex = 6
        Me.Label4.Text = "Disposition:"
        '
        'cmbDisposition
        '
        Me.cmbDisposition.BackColor = System.Drawing.Color.Silver
        Me.cmbDisposition.ForeColor = System.Drawing.Color.Yellow
        Me.cmbDisposition.FormattingEnabled = True
        Me.cmbDisposition.Location = New System.Drawing.Point(145, 76)
        Me.cmbDisposition.Name = "cmbDisposition"
        Me.cmbDisposition.Size = New System.Drawing.Size(313, 21)
        Me.cmbDisposition.TabIndex = 7
        '
        'txtTicketId
        '
        Me.txtTicketId.BackColor = System.Drawing.Color.Teal
        Me.txtTicketId.ForeColor = System.Drawing.Color.Yellow
        Me.txtTicketId.Location = New System.Drawing.Point(145, 8)
        Me.txtTicketId.Name = "txtTicketId"
        Me.txtTicketId.ReadOnly = True
        Me.txtTicketId.Size = New System.Drawing.Size(312, 20)
        Me.txtTicketId.TabIndex = 9
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
        Me.btnSubmit.Location = New System.Drawing.Point(482, 102)
        Me.btnSubmit.Name = "btnSubmit"
        Me.btnSubmit.Size = New System.Drawing.Size(78, 29)
        Me.btnSubmit.TabIndex = 10
        Me.btnSubmit.Text = "Submit"
        Me.btnSubmit.UseVisualStyleBackColor = False
        '
        'Label6
        '
        Me.Label6.AutoSize = True
        Me.Label6.ForeColor = System.Drawing.Color.Lime
        Me.Label6.Location = New System.Drawing.Point(71, 435)
        Me.Label6.Name = "Label6"
        Me.Label6.Size = New System.Drawing.Size(42, 13)
        Me.Label6.TabIndex = 11
        Me.Label6.Text = "History:"
        '
        'listMessages
        '
        Me.listMessages.Anchor = CType((((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Bottom) _
            Or System.Windows.Forms.AnchorStyles.Left) _
            Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.listMessages.BackColor = System.Drawing.Color.FromArgb(CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.listMessages.ForeColor = System.Drawing.Color.Lime
        Me.listMessages.Location = New System.Drawing.Point(73, 451)
        Me.listMessages.Name = "listMessages"
        Me.listMessages.Size = New System.Drawing.Size(745, 147)
        Me.listMessages.TabIndex = 12
        '
        'frmTicketAdd
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(937, 666)
        Me.Controls.Add(Me.listMessages)
        Me.Controls.Add(Me.Label6)
        Me.Controls.Add(Me.btnSubmit)
        Me.Controls.Add(Me.txtTicketId)
        Me.Controls.Add(Me.Label5)
        Me.Controls.Add(Me.cmbDisposition)
        Me.Controls.Add(Me.Label4)
        Me.Controls.Add(Me.Label3)
        Me.Controls.Add(Me.TextBox1)
        Me.Controls.Add(Me.Label2)
        Me.Controls.Add(Me.rtbNotes)
        Me.Controls.Add(Me.cmbAssignedTo)
        Me.Controls.Add(Me.Label1)
        Me.Name = "frmTicketAdd"
        Me.Text = "Gridcoin - Add or Edit Ticket"
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents Label1 As System.Windows.Forms.Label
    Friend WithEvents cmbAssignedTo As System.Windows.Forms.ComboBox
    Friend WithEvents rtbNotes As System.Windows.Forms.RichTextBox
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents TextBox1 As System.Windows.Forms.TextBox
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents Label4 As System.Windows.Forms.Label
    Friend WithEvents cmbDisposition As System.Windows.Forms.ComboBox
    Friend WithEvents txtTicketId As System.Windows.Forms.TextBox
    Friend WithEvents Label5 As System.Windows.Forms.Label
    Friend WithEvents btnSubmit As System.Windows.Forms.Button
    Friend WithEvents Label6 As System.Windows.Forms.Label
    Private WithEvents listMessages As System.Windows.Forms.TreeView
End Class
