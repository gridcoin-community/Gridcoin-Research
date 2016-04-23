<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmPlaceVote
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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmPlaceVote))
        Me.lblTitle = New System.Windows.Forms.Label()
        Me.lblQuestion = New System.Windows.Forms.Label()
        Me.btnVote = New System.Windows.Forms.Button()
        Me.Button1 = New System.Windows.Forms.Button()
        Me.lblMultipleChoice = New System.Windows.Forms.Label()
        Me.lnkURL = New System.Windows.Forms.LinkLabel()
        Me.Label1 = New System.Windows.Forms.Label()
        Me.SuspendLayout()
        '
        'lblTitle
        '
        Me.lblTitle.AutoSize = True
        Me.lblTitle.Font = New System.Drawing.Font("Microsoft Sans Serif", 15.75!, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblTitle.ForeColor = System.Drawing.Color.Lime
        Me.lblTitle.Location = New System.Drawing.Point(143, 5)
        Me.lblTitle.Name = "lblTitle"
        Me.lblTitle.Size = New System.Drawing.Size(58, 25)
        Me.lblTitle.TabIndex = 1
        Me.lblTitle.Text = "Title"
        '
        'lblQuestion
        '
        Me.lblQuestion.AutoSize = True
        Me.lblQuestion.Font = New System.Drawing.Font("Microsoft Sans Serif", 14.25!, System.Drawing.FontStyle.Underline, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblQuestion.ForeColor = System.Drawing.Color.Lime
        Me.lblQuestion.Location = New System.Drawing.Point(29, 81)
        Me.lblQuestion.Name = "lblQuestion"
        Me.lblQuestion.Size = New System.Drawing.Size(91, 24)
        Me.lblQuestion.TabIndex = 2
        Me.lblQuestion.Text = "Question:"
        '
        'btnVote
        '
        Me.btnVote.ForeColor = System.Drawing.Color.Lime
        Me.btnVote.Location = New System.Drawing.Point(480, 553)
        Me.btnVote.Name = "btnVote"
        Me.btnVote.Size = New System.Drawing.Size(118, 35)
        Me.btnVote.TabIndex = 57
        Me.btnVote.Text = "Place Vote"
        Me.btnVote.UseVisualStyleBackColor = False
        '
        'Button1
        '
        Me.Button1.Location = New System.Drawing.Point(1053, 570)
        Me.Button1.Name = "Button1"
        Me.Button1.Size = New System.Drawing.Size(56, 18)
        Me.Button1.TabIndex = 58
        Me.Button1.Text = "Button1"
        Me.Button1.UseVisualStyleBackColor = True
        Me.Button1.Visible = False
        '
        'lblMultipleChoice
        '
        Me.lblMultipleChoice.AutoSize = True
        Me.lblMultipleChoice.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblMultipleChoice.ForeColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.lblMultipleChoice.Location = New System.Drawing.Point(936, 57)
        Me.lblMultipleChoice.Name = "lblMultipleChoice"
        Me.lblMultipleChoice.Size = New System.Drawing.Size(158, 13)
        Me.lblMultipleChoice.TabIndex = 59
        Me.lblMultipleChoice.Text = "(Multiple Choice Voting Allowed)"
        '
        'lnkURL
        '
        Me.lnkURL.AutoSize = True
        Me.lnkURL.Font = New System.Drawing.Font("Microsoft Sans Serif", 12.0!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lnkURL.LinkColor = System.Drawing.Color.Olive
        Me.lnkURL.Location = New System.Drawing.Point(182, 49)
        Me.lnkURL.Name = "lnkURL"
        Me.lnkURL.Size = New System.Drawing.Size(162, 20)
        Me.lnkURL.TabIndex = 61
        Me.lnkURL.TabStop = True
        Me.lnkURL.Text = "http://www.gridcoin.us"
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.Font = New System.Drawing.Font("Microsoft Sans Serif", 14.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label1.ForeColor = System.Drawing.Color.Lime
        Me.Label1.Location = New System.Drawing.Point(29, 46)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(147, 24)
        Me.Label1.TabIndex = 60
        Me.Label1.Text = "Discussion URL:"
        '
        'frmPlaceVote
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(1106, 633)
        Me.Controls.Add(Me.lnkURL)
        Me.Controls.Add(Me.Label1)
        Me.Controls.Add(Me.lblMultipleChoice)
        Me.Controls.Add(Me.Button1)
        Me.Controls.Add(Me.btnVote)
        Me.Controls.Add(Me.lblQuestion)
        Me.Controls.Add(Me.lblTitle)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.KeyPreview = True
        Me.Name = "frmPlaceVote"
        Me.Text = "Place Vote"
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents lblTitle As System.Windows.Forms.Label
    Friend WithEvents lblQuestion As System.Windows.Forms.Label
    Friend WithEvents btnVote As System.Windows.Forms.Button
    Friend WithEvents Button1 As System.Windows.Forms.Button
    Friend WithEvents lblMultipleChoice As System.Windows.Forms.Label
    Friend WithEvents lnkURL As System.Windows.Forms.LinkLabel
    Friend WithEvents Label1 As System.Windows.Forms.Label
End Class
