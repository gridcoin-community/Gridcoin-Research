<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmDiagnostics
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
        Dim DataGridViewCellStyle1 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle2 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle3 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle4 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle5 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmDiagnostics))
        Me.btnRunDiagnostics = New System.Windows.Forms.Button()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.dvDiag = New System.Windows.Forms.DataGridView()
        CType(Me.dvDiag, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.SuspendLayout()
        '
        'btnRunDiagnostics
        '
        Me.btnRunDiagnostics.BackColor = System.Drawing.Color.Black
        Me.btnRunDiagnostics.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.btnRunDiagnostics.Location = New System.Drawing.Point(25, 417)
        Me.btnRunDiagnostics.Name = "btnRunDiagnostics"
        Me.btnRunDiagnostics.Size = New System.Drawing.Size(177, 29)
        Me.btnRunDiagnostics.TabIndex = 8
        Me.btnRunDiagnostics.Text = "Run Diagnostics"
        Me.btnRunDiagnostics.UseVisualStyleBackColor = False
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.ForeColor = System.Drawing.Color.Lime
        Me.Label3.Location = New System.Drawing.Point(23, 30)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(139, 13)
        Me.Label3.TabIndex = 5
        Me.Label3.Text = "Gridcoin Diagnostic System:"
        '
        'dvDiag
        '
        DataGridViewCellStyle1.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle1.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle1.SelectionBackColor = System.Drawing.Color.Gray
        DataGridViewCellStyle1.SelectionForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.dvDiag.AlternatingRowsDefaultCellStyle = DataGridViewCellStyle1
        Me.dvDiag.BackgroundColor = System.Drawing.Color.Black
        DataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle2.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle2.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle2.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText
        DataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.[True]
        Me.dvDiag.ColumnHeadersDefaultCellStyle = DataGridViewCellStyle2
        Me.dvDiag.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize
        DataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle3.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle3.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle3.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText
        DataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.[False]
        Me.dvDiag.DefaultCellStyle = DataGridViewCellStyle3
        Me.dvDiag.EnableHeadersVisualStyles = False
        Me.dvDiag.GridColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.dvDiag.Location = New System.Drawing.Point(25, 46)
        Me.dvDiag.Name = "dvDiag"
        DataGridViewCellStyle4.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle4.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle4.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle4.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle4.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle4.SelectionForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(0, Byte), Integer))
        DataGridViewCellStyle4.WrapMode = System.Windows.Forms.DataGridViewTriState.[True]
        Me.dvDiag.RowHeadersDefaultCellStyle = DataGridViewCellStyle4
        DataGridViewCellStyle5.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle5.ForeColor = System.Drawing.Color.Lime
        Me.dvDiag.RowsDefaultCellStyle = DataGridViewCellStyle5
        Me.dvDiag.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect
        Me.dvDiag.Size = New System.Drawing.Size(882, 346)
        Me.dvDiag.TabIndex = 65
        '
        'frmDiagnostics
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(937, 666)
        Me.Controls.Add(Me.dvDiag)
        Me.Controls.Add(Me.btnRunDiagnostics)
        Me.Controls.Add(Me.Label3)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmDiagnostics"
        Me.Text = "Gridcoin Diagnostic System"
        CType(Me.dvDiag, System.ComponentModel.ISupportInitialize).EndInit()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents btnRunDiagnostics As System.Windows.Forms.Button
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents dvDiag As System.Windows.Forms.DataGridView
End Class
