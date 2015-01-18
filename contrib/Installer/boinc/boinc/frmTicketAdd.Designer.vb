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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmTicketAdd))
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
        Me.txtAttachment = New System.Windows.Forms.TextBox()
        Me.Label9 = New System.Windows.Forms.Label()
        Me.btnOpenAttachment = New System.Windows.Forms.Button()
        Me.btnOpenFolder = New System.Windows.Forms.Button()
        Me.txtUpdatedBy = New System.Windows.Forms.TextBox()
        Me.Label10 = New System.Windows.Forms.Label()
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
        Me.Label2.Location = New System.Drawing.Point(69, 146)
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
        Me.txtTicketId.BackColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.txtTicketId.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtTicketId.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.txtTicketId.Location = New System.Drawing.Point(145, 8)
        Me.txtTicketId.Name = "txtTicketId"
        Me.txtTicketId.ReadOnly = True
        Me.txtTicketId.Size = New System.Drawing.Size(312, 22)
        Me.txtTicketId.TabIndex = 1
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.ForeColor = System.Drawing.Color.Lime
        Me.Label5.Location = New System.Drawing.Point(69, 17)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(54, 13)
        Me.Label5.TabIndex = 8
        Me.Label5.Text = "Ticket ID:"
        '
        'btnSubmit
        '
        Me.btnSubmit.BackColor = System.Drawing.Color.Gray
        Me.btnSubmit.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
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
        Me.Label7.Location = New System.Drawing.Point(69, 66)
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
        Me.rtbNotes.Size = New System.Drawing.Size(823, 250)
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
        Me.tvTicketHistory.Size = New System.Drawing.Size(824, 147)
        Me.tvTicketHistory.TabIndex = 12
        '
        'Label8
        '
        Me.Label8.AutoSize = True
        Me.Label8.ForeColor = System.Drawing.Color.Lime
        Me.Label8.Location = New System.Drawing.Point(70, 43)
        Me.Label8.Name = "Label8"
        Me.Label8.Size = New System.Drawing.Size(72, 13)
        Me.Label8.TabIndex = 16
        Me.Label8.Text = "Submitted By:"
        '
        'txtSubmittedBy
        '
        Me.txtSubmittedBy.BackColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.txtSubmittedBy.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtSubmittedBy.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.txtSubmittedBy.Location = New System.Drawing.Point(145, 34)
        Me.txtSubmittedBy.Name = "txtSubmittedBy"
        Me.txtSubmittedBy.ReadOnly = True
        Me.txtSubmittedBy.Size = New System.Drawing.Size(312, 22)
        Me.txtSubmittedBy.TabIndex = 2
        '
        'btnAddAttachment
        '
        Me.btnAddAttachment.BackColor = System.Drawing.Color.Gray
        Me.btnAddAttachment.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.btnAddAttachment.Location = New System.Drawing.Point(548, 130)
        Me.btnAddAttachment.Name = "btnAddAttachment"
        Me.btnAddAttachment.Size = New System.Drawing.Size(112, 29)
        Me.btnAddAttachment.TabIndex = 17
        Me.btnAddAttachment.Text = "Add Attachment"
        Me.btnAddAttachment.UseVisualStyleBackColor = False
        '
        'txtAttachment
        '
        Me.txtAttachment.BackColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.txtAttachment.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtAttachment.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.txtAttachment.Location = New System.Drawing.Point(537, 85)
        Me.txtAttachment.Name = "txtAttachment"
        Me.txtAttachment.ReadOnly = True
        Me.txtAttachment.Size = New System.Drawing.Size(359, 22)
        Me.txtAttachment.TabIndex = 18
        '
        'Label9
        '
        Me.Label9.AutoSize = True
        Me.Label9.ForeColor = System.Drawing.Color.Lime
        Me.Label9.Location = New System.Drawing.Point(462, 93)
        Me.Label9.Name = "Label9"
        Me.Label9.Size = New System.Drawing.Size(64, 13)
        Me.Label9.TabIndex = 19
        Me.Label9.Text = "Attachment:"
        '
        'btnOpenAttachment
        '
        Me.btnOpenAttachment.BackColor = System.Drawing.Color.Gray
        Me.btnOpenAttachment.ForeColor = System.Drawing.Color.Red
        Me.btnOpenAttachment.Location = New System.Drawing.Point(666, 130)
        Me.btnOpenAttachment.Name = "btnOpenAttachment"
        Me.btnOpenAttachment.Size = New System.Drawing.Size(112, 29)
        Me.btnOpenAttachment.TabIndex = 20
        Me.btnOpenAttachment.Text = "Open Attachment"
        Me.btnOpenAttachment.UseVisualStyleBackColor = False
        '
        'btnOpenFolder
        '
        Me.btnOpenFolder.BackColor = System.Drawing.Color.Gray
        Me.btnOpenFolder.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.btnOpenFolder.Location = New System.Drawing.Point(784, 130)
        Me.btnOpenFolder.Name = "btnOpenFolder"
        Me.btnOpenFolder.Size = New System.Drawing.Size(112, 29)
        Me.btnOpenFolder.TabIndex = 21
        Me.btnOpenFolder.Text = "Open Folder"
        Me.btnOpenFolder.UseVisualStyleBackColor = False
        '
        'txtUpdatedBy
        '
        Me.txtUpdatedBy.BackColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.txtUpdatedBy.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtUpdatedBy.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.txtUpdatedBy.Location = New System.Drawing.Point(537, 34)
        Me.txtUpdatedBy.Name = "txtUpdatedBy"
        Me.txtUpdatedBy.ReadOnly = True
        Me.txtUpdatedBy.Size = New System.Drawing.Size(359, 22)
        Me.txtUpdatedBy.TabIndex = 22
        '
        'Label10
        '
        Me.Label10.AutoSize = True
        Me.Label10.ForeColor = System.Drawing.Color.Lime
        Me.Label10.Location = New System.Drawing.Point(462, 43)
        Me.Label10.Name = "Label10"
        Me.Label10.Size = New System.Drawing.Size(66, 13)
        Me.Label10.TabIndex = 23
        Me.Label10.Text = "Updated By:"
        '
        'frmTicketAdd
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(937, 666)
        Me.Controls.Add(Me.txtUpdatedBy)
        Me.Controls.Add(Me.Label10)
        Me.Controls.Add(Me.btnOpenFolder)
        Me.Controls.Add(Me.btnOpenAttachment)
        Me.Controls.Add(Me.txtAttachment)
        Me.Controls.Add(Me.Label9)
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
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmTicketAdd"
        Me.Text = "Gridcoin - Add or Edit Ticket v1.1"
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
    Friend WithEvents txtAttachment As System.Windows.Forms.TextBox
    Friend WithEvents Label9 As System.Windows.Forms.Label
    Friend WithEvents btnOpenAttachment As System.Windows.Forms.Button
    Friend WithEvents btnOpenFolder As System.Windows.Forms.Button
    Friend WithEvents txtUpdatedBy As System.Windows.Forms.TextBox
    Friend WithEvents Label10 As System.Windows.Forms.Label
End Class
