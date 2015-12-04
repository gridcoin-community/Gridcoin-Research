<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmChartVotes
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
        Dim ChartArea1 As System.Windows.Forms.DataVisualization.Charting.ChartArea = New System.Windows.Forms.DataVisualization.Charting.ChartArea()
        Dim Legend1 As System.Windows.Forms.DataVisualization.Charting.Legend = New System.Windows.Forms.DataVisualization.Charting.Legend()
        Dim Series1 As System.Windows.Forms.DataVisualization.Charting.Series = New System.Windows.Forms.DataVisualization.Charting.Series()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmChartVotes))
        Me.C = New System.Windows.Forms.DataVisualization.Charting.Chart()
        Me.lblTitle = New System.Windows.Forms.Label()
        Me.lblQuestion = New System.Windows.Forms.Label()
        Me.lblBestAnswer = New System.Windows.Forms.Label()
        Me.Label1 = New System.Windows.Forms.Label()
        Me.lnkURL = New System.Windows.Forms.LinkLabel()
        CType(Me.C, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.SuspendLayout()
        '
        'C
        '
        ChartArea1.Name = "ChartArea1"
        Me.C.ChartAreas.Add(ChartArea1)
        Legend1.Name = "Legend1"
        Me.C.Legends.Add(Legend1)
        Me.C.Location = New System.Drawing.Point(12, 176)
        Me.C.Name = "C"
        Series1.ChartArea = "ChartArea1"
        Series1.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line
        Series1.Legend = "Legend1"
        Series1.Name = "Series1"
        Me.C.Series.Add(Series1)
        Me.C.Size = New System.Drawing.Size(1082, 536)
        Me.C.TabIndex = 0
        Me.C.Text = "Analyzer"
        '
        'lblTitle
        '
        Me.lblTitle.AutoSize = True
        Me.lblTitle.Font = New System.Drawing.Font("Microsoft Sans Serif", 14.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblTitle.ForeColor = System.Drawing.Color.Lime
        Me.lblTitle.Location = New System.Drawing.Point(218, 9)
        Me.lblTitle.Name = "lblTitle"
        Me.lblTitle.Size = New System.Drawing.Size(45, 24)
        Me.lblTitle.TabIndex = 1
        Me.lblTitle.Text = "Title"
        '
        'lblQuestion
        '
        Me.lblQuestion.AutoSize = True
        Me.lblQuestion.Font = New System.Drawing.Font("Microsoft Sans Serif", 14.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblQuestion.ForeColor = System.Drawing.Color.Lime
        Me.lblQuestion.Location = New System.Drawing.Point(12, 55)
        Me.lblQuestion.Name = "lblQuestion"
        Me.lblQuestion.Size = New System.Drawing.Size(91, 24)
        Me.lblQuestion.TabIndex = 2
        Me.lblQuestion.Text = "Question:"
        '
        'lblBestAnswer
        '
        Me.lblBestAnswer.AutoSize = True
        Me.lblBestAnswer.Font = New System.Drawing.Font("Microsoft Sans Serif", 14.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lblBestAnswer.ForeColor = System.Drawing.Color.Lime
        Me.lblBestAnswer.Location = New System.Drawing.Point(12, 734)
        Me.lblBestAnswer.Name = "lblBestAnswer"
        Me.lblBestAnswer.Size = New System.Drawing.Size(120, 24)
        Me.lblBestAnswer.TabIndex = 3
        Me.lblBestAnswer.Text = "Best Answer:"
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.Font = New System.Drawing.Font("Microsoft Sans Serif", 14.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.Label1.ForeColor = System.Drawing.Color.Lime
        Me.Label1.Location = New System.Drawing.Point(12, 100)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(147, 24)
        Me.Label1.TabIndex = 4
        Me.Label1.Text = "Discussion URL:"
        '
        'lnkURL
        '
        Me.lnkURL.AutoSize = True
        Me.lnkURL.Font = New System.Drawing.Font("Microsoft Sans Serif", 12.0!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.lnkURL.LinkColor = System.Drawing.Color.Olive
        Me.lnkURL.Location = New System.Drawing.Point(165, 100)
        Me.lnkURL.Name = "lnkURL"
        Me.lnkURL.Size = New System.Drawing.Size(162, 20)
        Me.lnkURL.TabIndex = 12
        Me.lnkURL.TabStop = True
        Me.lnkURL.Text = "http://www.gridcoin.us"
        '
        'frmChartVotes
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(1106, 799)
        Me.Controls.Add(Me.lnkURL)
        Me.Controls.Add(Me.Label1)
        Me.Controls.Add(Me.lblBestAnswer)
        Me.Controls.Add(Me.lblQuestion)
        Me.Controls.Add(Me.lblTitle)
        Me.Controls.Add(Me.C)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.KeyPreview = True
        Me.Name = "frmChartVotes"
        Me.Text = "Poll Results"
        CType(Me.C, System.ComponentModel.ISupportInitialize).EndInit()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents C As System.Windows.Forms.DataVisualization.Charting.Chart
    Friend WithEvents lblTitle As System.Windows.Forms.Label
    Friend WithEvents lblQuestion As System.Windows.Forms.Label
    Friend WithEvents lblBestAnswer As System.Windows.Forms.Label
    Friend WithEvents Label1 As System.Windows.Forms.Label
    Friend WithEvents lnkURL As System.Windows.Forms.LinkLabel
End Class
