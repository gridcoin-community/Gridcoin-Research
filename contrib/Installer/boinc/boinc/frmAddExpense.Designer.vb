<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmAddExpense
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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmAddExpense))
        Me.Label2 = New System.Windows.Forms.Label()
        Me.txtDescription = New System.Windows.Forms.TextBox()
        Me.txtTicketId = New System.Windows.Forms.TextBox()
        Me.Label5 = New System.Windows.Forms.Label()
        Me.btnSubmit = New System.Windows.Forms.Button()
        Me.rtbNotes = New System.Windows.Forms.RichTextBox()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.Label8 = New System.Windows.Forms.Label()
        Me.txtSubmittedBy = New System.Windows.Forms.TextBox()
        Me.btnAddAttachment = New System.Windows.Forms.Button()
        Me.txtAttachment = New System.Windows.Forms.TextBox()
        Me.Label9 = New System.Windows.Forms.Label()
        Me.rbExpense = New System.Windows.Forms.RadioButton()
        Me.rbCampaign = New System.Windows.Forms.RadioButton()
        Me.GroupBox1 = New System.Windows.Forms.GroupBox()
        Me.Label1 = New System.Windows.Forms.Label()
        Me.dtStart = New System.Windows.Forms.DateTimePicker()
        Me.dtEnd = New System.Windows.Forms.DateTimePicker()
        Me.Label4 = New System.Windows.Forms.Label()
        Me.Label6 = New System.Windows.Forms.Label()
        Me.txtAmount = New System.Windows.Forms.TextBox()
        Me.lblMode = New System.Windows.Forms.Label()
        Me.btnOpen = New System.Windows.Forms.Button()
        Me.btnVirusScan = New System.Windows.Forms.Button()
        Me.GroupBox1.SuspendLayout()
        Me.SuspendLayout()
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.ForeColor = System.Drawing.Color.Lime
        Me.Label2.Location = New System.Drawing.Point(71, 142)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(63, 13)
        Me.Label2.TabIndex = 3
        Me.Label2.Text = "Description:"
        '
        'txtDescription
        '
        Me.txtDescription.BackColor = System.Drawing.Color.Black
        Me.txtDescription.ForeColor = System.Drawing.Color.Lime
        Me.txtDescription.Location = New System.Drawing.Point(145, 134)
        Me.txtDescription.Name = "txtDescription"
        Me.txtDescription.Size = New System.Drawing.Size(358, 20)
        Me.txtDescription.TabIndex = 6
        '
        'txtTicketId
        '
        Me.txtTicketId.BackColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.txtTicketId.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtTicketId.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.txtTicketId.Location = New System.Drawing.Point(144, 12)
        Me.txtTicketId.Name = "txtTicketId"
        Me.txtTicketId.ReadOnly = True
        Me.txtTicketId.Size = New System.Drawing.Size(359, 22)
        Me.txtTicketId.TabIndex = 1
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.ForeColor = System.Drawing.Color.Lime
        Me.Label5.Location = New System.Drawing.Point(69, 21)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(65, 13)
        Me.Label5.TabIndex = 8
        Me.Label5.Text = "Expense ID:"
        '
        'btnSubmit
        '
        Me.btnSubmit.BackColor = System.Drawing.Color.Black
        Me.btnSubmit.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.btnSubmit.Location = New System.Drawing.Point(530, 224)
        Me.btnSubmit.Name = "btnSubmit"
        Me.btnSubmit.Size = New System.Drawing.Size(177, 29)
        Me.btnSubmit.TabIndex = 8
        Me.btnSubmit.Text = "Submit"
        Me.btnSubmit.UseVisualStyleBackColor = False
        '
        'rtbNotes
        '
        Me.rtbNotes.BackColor = System.Drawing.Color.Black
        Me.rtbNotes.ForeColor = System.Drawing.Color.Lime
        Me.rtbNotes.Location = New System.Drawing.Point(72, 291)
        Me.rtbNotes.Name = "rtbNotes"
        Me.rtbNotes.Size = New System.Drawing.Size(823, 332)
        Me.rtbNotes.TabIndex = 7
        Me.rtbNotes.Text = ""
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.ForeColor = System.Drawing.Color.Lime
        Me.Label3.Location = New System.Drawing.Point(70, 275)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(59, 13)
        Me.Label3.TabIndex = 5
        Me.Label3.Text = "Comments:"
        '
        'Label8
        '
        Me.Label8.AutoSize = True
        Me.Label8.ForeColor = System.Drawing.Color.Lime
        Me.Label8.Location = New System.Drawing.Point(70, 61)
        Me.Label8.Name = "Label8"
        Me.Label8.Size = New System.Drawing.Size(72, 13)
        Me.Label8.TabIndex = 16
        Me.Label8.Text = "Submitted By:"
        '
        'txtSubmittedBy
        '
        Me.txtSubmittedBy.BackColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.txtSubmittedBy.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtSubmittedBy.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.txtSubmittedBy.Location = New System.Drawing.Point(145, 52)
        Me.txtSubmittedBy.Name = "txtSubmittedBy"
        Me.txtSubmittedBy.ReadOnly = True
        Me.txtSubmittedBy.Size = New System.Drawing.Size(359, 22)
        Me.txtSubmittedBy.TabIndex = 2
        '
        'btnAddAttachment
        '
        Me.btnAddAttachment.BackColor = System.Drawing.Color.Black
        Me.btnAddAttachment.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.btnAddAttachment.Location = New System.Drawing.Point(530, 172)
        Me.btnAddAttachment.Name = "btnAddAttachment"
        Me.btnAddAttachment.Size = New System.Drawing.Size(177, 29)
        Me.btnAddAttachment.TabIndex = 17
        Me.btnAddAttachment.Text = "Add Attachment"
        Me.btnAddAttachment.UseVisualStyleBackColor = False
        '
        'txtAttachment
        '
        Me.txtAttachment.BackColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.txtAttachment.Font = New System.Drawing.Font("Arial", 9.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.txtAttachment.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.txtAttachment.Location = New System.Drawing.Point(144, 94)
        Me.txtAttachment.Name = "txtAttachment"
        Me.txtAttachment.ReadOnly = True
        Me.txtAttachment.Size = New System.Drawing.Size(359, 22)
        Me.txtAttachment.TabIndex = 18
        '
        'Label9
        '
        Me.Label9.AutoSize = True
        Me.Label9.ForeColor = System.Drawing.Color.Lime
        Me.Label9.Location = New System.Drawing.Point(70, 103)
        Me.Label9.Name = "Label9"
        Me.Label9.Size = New System.Drawing.Size(64, 13)
        Me.Label9.TabIndex = 19
        Me.Label9.Text = "Attachment:"
        '
        'rbExpense
        '
        Me.rbExpense.AutoSize = True
        Me.rbExpense.ForeColor = System.Drawing.Color.Lime
        Me.rbExpense.Location = New System.Drawing.Point(24, 21)
        Me.rbExpense.Name = "rbExpense"
        Me.rbExpense.Size = New System.Drawing.Size(142, 17)
        Me.rbExpense.TabIndex = 20
        Me.rbExpense.TabStop = True
        Me.rbExpense.Text = "Expense Reimbursement"
        Me.rbExpense.UseVisualStyleBackColor = True
        '
        'rbCampaign
        '
        Me.rbCampaign.AutoSize = True
        Me.rbCampaign.ForeColor = System.Drawing.Color.Lime
        Me.rbCampaign.Location = New System.Drawing.Point(24, 44)
        Me.rbCampaign.Name = "rbCampaign"
        Me.rbCampaign.Size = New System.Drawing.Size(97, 17)
        Me.rbCampaign.TabIndex = 21
        Me.rbCampaign.TabStop = True
        Me.rbCampaign.Text = "New Campaign"
        Me.rbCampaign.UseVisualStyleBackColor = True
        '
        'GroupBox1
        '
        Me.GroupBox1.Controls.Add(Me.rbExpense)
        Me.GroupBox1.Controls.Add(Me.rbCampaign)
        Me.GroupBox1.ForeColor = System.Drawing.Color.Lime
        Me.GroupBox1.Location = New System.Drawing.Point(530, 12)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(177, 73)
        Me.GroupBox1.TabIndex = 22
        Me.GroupBox1.TabStop = False
        Me.GroupBox1.Text = "Select Type"
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.ForeColor = System.Drawing.Color.Lime
        Me.Label1.Location = New System.Drawing.Point(71, 179)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(58, 13)
        Me.Label1.TabIndex = 23
        Me.Label1.Text = "Start Date:"
        '
        'dtStart
        '
        Me.dtStart.CalendarForeColor = System.Drawing.Color.Lime
        Me.dtStart.CalendarMonthBackground = System.Drawing.Color.Black
        Me.dtStart.CalendarTitleBackColor = System.Drawing.Color.Black
        Me.dtStart.CalendarTitleForeColor = System.Drawing.Color.Lime
        Me.dtStart.Location = New System.Drawing.Point(145, 172)
        Me.dtStart.Name = "dtStart"
        Me.dtStart.Size = New System.Drawing.Size(358, 20)
        Me.dtStart.TabIndex = 24
        '
        'dtEnd
        '
        Me.dtEnd.CalendarForeColor = System.Drawing.Color.Lime
        Me.dtEnd.CalendarMonthBackground = System.Drawing.Color.Black
        Me.dtEnd.CalendarTitleBackColor = System.Drawing.Color.Black
        Me.dtEnd.CalendarTitleForeColor = System.Drawing.Color.Lime
        Me.dtEnd.Location = New System.Drawing.Point(144, 205)
        Me.dtEnd.Name = "dtEnd"
        Me.dtEnd.Size = New System.Drawing.Size(358, 20)
        Me.dtEnd.TabIndex = 26
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.ForeColor = System.Drawing.Color.Lime
        Me.Label4.Location = New System.Drawing.Point(70, 212)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(55, 13)
        Me.Label4.TabIndex = 25
        Me.Label4.Text = "End Date:"
        '
        'Label6
        '
        Me.Label6.AutoSize = True
        Me.Label6.ForeColor = System.Drawing.Color.Lime
        Me.Label6.Location = New System.Drawing.Point(71, 240)
        Me.Label6.Name = "Label6"
        Me.Label6.Size = New System.Drawing.Size(46, 13)
        Me.Label6.TabIndex = 27
        Me.Label6.Text = "Amount:"
        '
        'txtAmount
        '
        Me.txtAmount.BackColor = System.Drawing.Color.Black
        Me.txtAmount.ForeColor = System.Drawing.Color.Lime
        Me.txtAmount.Location = New System.Drawing.Point(144, 233)
        Me.txtAmount.Name = "txtAmount"
        Me.txtAmount.Size = New System.Drawing.Size(358, 20)
        Me.txtAmount.TabIndex = 28
        '
        'lblMode
        '
        Me.lblMode.AutoSize = True
        Me.lblMode.Font = New System.Drawing.Font("Book Antiqua", 9.75!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblMode.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.lblMode.Location = New System.Drawing.Point(825, 12)
        Me.lblMode.Name = "lblMode"
        Me.lblMode.Size = New System.Drawing.Size(70, 17)
        Me.lblMode.TabIndex = 29
        Me.lblMode.Text = "Add Mode"
        '
        'btnOpen
        '
        Me.btnOpen.BackColor = System.Drawing.Color.Black
        Me.btnOpen.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.btnOpen.Location = New System.Drawing.Point(530, 126)
        Me.btnOpen.Name = "btnOpen"
        Me.btnOpen.Size = New System.Drawing.Size(177, 29)
        Me.btnOpen.TabIndex = 30
        Me.btnOpen.Text = "Open Attachment"
        Me.btnOpen.UseVisualStyleBackColor = False
        '
        'btnVirusScan
        '
        Me.btnVirusScan.BackColor = System.Drawing.Color.Black
        Me.btnVirusScan.ForeColor = System.Drawing.Color.Red
        Me.btnVirusScan.Location = New System.Drawing.Point(725, 129)
        Me.btnVirusScan.Name = "btnVirusScan"
        Me.btnVirusScan.Size = New System.Drawing.Size(112, 29)
        Me.btnVirusScan.TabIndex = 31
        Me.btnVirusScan.Text = "Virus Scan Attchmnt"
        Me.btnVirusScan.UseVisualStyleBackColor = False
        '
        'frmAddExpense
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(937, 666)
        Me.Controls.Add(Me.btnVirusScan)
        Me.Controls.Add(Me.btnOpen)
        Me.Controls.Add(Me.lblMode)
        Me.Controls.Add(Me.txtAmount)
        Me.Controls.Add(Me.Label6)
        Me.Controls.Add(Me.dtEnd)
        Me.Controls.Add(Me.Label4)
        Me.Controls.Add(Me.dtStart)
        Me.Controls.Add(Me.Label1)
        Me.Controls.Add(Me.GroupBox1)
        Me.Controls.Add(Me.txtAttachment)
        Me.Controls.Add(Me.Label9)
        Me.Controls.Add(Me.btnAddAttachment)
        Me.Controls.Add(Me.txtSubmittedBy)
        Me.Controls.Add(Me.Label8)
        Me.Controls.Add(Me.btnSubmit)
        Me.Controls.Add(Me.txtTicketId)
        Me.Controls.Add(Me.Label5)
        Me.Controls.Add(Me.Label3)
        Me.Controls.Add(Me.txtDescription)
        Me.Controls.Add(Me.Label2)
        Me.Controls.Add(Me.rtbNotes)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmAddExpense"
        Me.Text = "Gridcoin Foundation - Expense/New Campaign Submission Form 1.1"
        Me.GroupBox1.ResumeLayout(False)
        Me.GroupBox1.PerformLayout()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents txtDescription As System.Windows.Forms.TextBox
    Friend WithEvents txtTicketId As System.Windows.Forms.TextBox
    Friend WithEvents Label5 As System.Windows.Forms.Label
    Friend WithEvents btnSubmit As System.Windows.Forms.Button
    Friend WithEvents rtbNotes As System.Windows.Forms.RichTextBox
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents Label8 As System.Windows.Forms.Label
    Friend WithEvents txtSubmittedBy As System.Windows.Forms.TextBox
    Friend WithEvents btnAddAttachment As System.Windows.Forms.Button
    Friend WithEvents txtAttachment As System.Windows.Forms.TextBox
    Friend WithEvents Label9 As System.Windows.Forms.Label
    Friend WithEvents rbExpense As System.Windows.Forms.RadioButton
    Friend WithEvents rbCampaign As System.Windows.Forms.RadioButton
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents Label1 As System.Windows.Forms.Label
    Friend WithEvents dtStart As System.Windows.Forms.DateTimePicker
    Friend WithEvents dtEnd As System.Windows.Forms.DateTimePicker
    Friend WithEvents Label4 As System.Windows.Forms.Label
    Friend WithEvents Label6 As System.Windows.Forms.Label
    Friend WithEvents txtAmount As System.Windows.Forms.TextBox
    Friend WithEvents lblMode As System.Windows.Forms.Label
    Friend WithEvents btnOpen As System.Windows.Forms.Button
    Friend WithEvents btnVirusScan As System.Windows.Forms.Button
End Class
