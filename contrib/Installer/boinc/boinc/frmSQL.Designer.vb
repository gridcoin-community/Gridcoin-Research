<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmSQL
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
        Me.components = New System.ComponentModel.Container()
        Dim DataGridViewCellStyle1 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle2 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle3 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle4 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim DataGridViewCellStyle5 As System.Windows.Forms.DataGridViewCellStyle = New System.Windows.Forms.DataGridViewCellStyle()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmSQL))
        Me.gbQueryAnalyzer = New System.Windows.Forms.GroupBox()
        Me.rtbQuery = New System.Windows.Forms.RichTextBox()
        Me.lvTables = New System.Windows.Forms.ListView()
        Me.Table = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.gbResultsPane = New System.Windows.Forms.GroupBox()
        Me.dgv = New System.Windows.Forms.DataGridView()
        Me.btnExec = New System.Windows.Forms.Button()
        Me.pbSync = New System.Windows.Forms.ProgressBar()
        Me.lblSync = New System.Windows.Forms.Label()
        Me.tSync = New System.Windows.Forms.Timer(Me.components)
        Me.gbQueryAnalyzer.SuspendLayout()
        Me.gbResultsPane.SuspendLayout()
        CType(Me.dgv, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.SuspendLayout()
        '
        'gbQueryAnalyzer
        '
        Me.gbQueryAnalyzer.Controls.Add(Me.rtbQuery)
        Me.gbQueryAnalyzer.Controls.Add(Me.lvTables)
        Me.gbQueryAnalyzer.ForeColor = System.Drawing.Color.Lime
        Me.gbQueryAnalyzer.Location = New System.Drawing.Point(28, 22)
        Me.gbQueryAnalyzer.Name = "gbQueryAnalyzer"
        Me.gbQueryAnalyzer.Size = New System.Drawing.Size(938, 334)
        Me.gbQueryAnalyzer.TabIndex = 1
        Me.gbQueryAnalyzer.TabStop = False
        Me.gbQueryAnalyzer.Text = "Query Analyzer"
        '
        'rtbQuery
        '
        Me.rtbQuery.BackColor = System.Drawing.Color.Black
        Me.rtbQuery.Font = New System.Drawing.Font("Courier New", 12.0!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.rtbQuery.ForeColor = System.Drawing.Color.Lime
        Me.rtbQuery.Location = New System.Drawing.Point(170, 15)
        Me.rtbQuery.Name = "rtbQuery"
        Me.rtbQuery.Size = New System.Drawing.Size(762, 313)
        Me.rtbQuery.TabIndex = 2
        Me.rtbQuery.Text = ""
        '
        'lvTables
        '
        Me.lvTables.BackColor = System.Drawing.Color.Black
        Me.lvTables.Columns.AddRange(New System.Windows.Forms.ColumnHeader() {Me.Table})
        Me.lvTables.ForeColor = System.Drawing.Color.Lime
        Me.lvTables.Location = New System.Drawing.Point(6, 15)
        Me.lvTables.Name = "lvTables"
        Me.lvTables.OwnerDraw = True
        Me.lvTables.Size = New System.Drawing.Size(158, 312)
        Me.lvTables.TabIndex = 1
        Me.lvTables.UseCompatibleStateImageBehavior = False
        '
        'gbResultsPane
        '
        Me.gbResultsPane.Controls.Add(Me.dgv)
        Me.gbResultsPane.ForeColor = System.Drawing.Color.Lime
        Me.gbResultsPane.Location = New System.Drawing.Point(28, 362)
        Me.gbResultsPane.Name = "gbResultsPane"
        Me.gbResultsPane.Size = New System.Drawing.Size(938, 339)
        Me.gbResultsPane.TabIndex = 2
        Me.gbResultsPane.TabStop = False
        Me.gbResultsPane.Text = "Results"
        '
        'dgv
        '
        DataGridViewCellStyle1.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle1.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle1.SelectionBackColor = System.Drawing.Color.Gray
        DataGridViewCellStyle1.SelectionForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.dgv.AlternatingRowsDefaultCellStyle = DataGridViewCellStyle1
        Me.dgv.BackgroundColor = System.Drawing.Color.Black
        DataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle2.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle2.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle2.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText
        DataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.[True]
        Me.dgv.ColumnHeadersDefaultCellStyle = DataGridViewCellStyle2
        Me.dgv.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize
        DataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle3.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle3.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle3.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText
        DataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.[False]
        Me.dgv.DefaultCellStyle = DataGridViewCellStyle3
        Me.dgv.EnableHeadersVisualStyles = False
        Me.dgv.GridColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(64, Byte), Integer))
        Me.dgv.Location = New System.Drawing.Point(6, 19)
        Me.dgv.Name = "dgv"
        DataGridViewCellStyle4.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft
        DataGridViewCellStyle4.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle4.Font = New System.Drawing.Font("Microsoft Sans Serif", 8.25!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        DataGridViewCellStyle4.ForeColor = System.Drawing.Color.Lime
        DataGridViewCellStyle4.SelectionBackColor = System.Drawing.SystemColors.Highlight
        DataGridViewCellStyle4.SelectionForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(64, Byte), Integer), CType(CType(0, Byte), Integer))
        DataGridViewCellStyle4.WrapMode = System.Windows.Forms.DataGridViewTriState.[True]
        Me.dgv.RowHeadersDefaultCellStyle = DataGridViewCellStyle4
        DataGridViewCellStyle5.BackColor = System.Drawing.Color.Black
        DataGridViewCellStyle5.ForeColor = System.Drawing.Color.Lime
        Me.dgv.RowsDefaultCellStyle = DataGridViewCellStyle5
        Me.dgv.Size = New System.Drawing.Size(891, 314)
        Me.dgv.TabIndex = 0
        '
        'btnExec
        '
        Me.btnExec.BackgroundImage = Global.BoincStake.My.Resources.Resources.gradient75
        Me.btnExec.ForeColor = System.Drawing.Color.Lime
        Me.btnExec.Location = New System.Drawing.Point(890, 5)
        Me.btnExec.Name = "btnExec"
        Me.btnExec.Size = New System.Drawing.Size(76, 20)
        Me.btnExec.TabIndex = 3
        Me.btnExec.Text = "Execute"
        Me.btnExec.UseVisualStyleBackColor = True
        '
        'pbSync
        '
        Me.pbSync.ForeColor = System.Drawing.Color.Teal
        Me.pbSync.Location = New System.Drawing.Point(74, 7)
        Me.pbSync.Maximum = 40000
        Me.pbSync.Name = "pbSync"
        Me.pbSync.Size = New System.Drawing.Size(794, 12)
        Me.pbSync.Step = 1
        Me.pbSync.Style = System.Windows.Forms.ProgressBarStyle.Continuous
        Me.pbSync.TabIndex = 4
        '
        'lblSync
        '
        Me.lblSync.AutoSize = True
        Me.lblSync.ForeColor = System.Drawing.Color.FromArgb(CType(CType(128, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(128, Byte), Integer))
        Me.lblSync.Location = New System.Drawing.Point(25, 5)
        Me.lblSync.Name = "lblSync"
        Me.lblSync.Size = New System.Drawing.Size(34, 13)
        Me.lblSync.TabIndex = 5
        Me.lblSync.Text = "Sync:"
        '
        'tSync
        '
        Me.tSync.Enabled = True
        Me.tSync.Interval = 20000
        '
        'frmSQL
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackColor = System.Drawing.Color.Black
        Me.ClientSize = New System.Drawing.Size(978, 713)
        Me.Controls.Add(Me.lblSync)
        Me.Controls.Add(Me.pbSync)
        Me.Controls.Add(Me.btnExec)
        Me.Controls.Add(Me.gbResultsPane)
        Me.Controls.Add(Me.gbQueryAnalyzer)
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "frmSQL"
        Me.Text = "Gridcoin Query Analyzer"
        Me.gbQueryAnalyzer.ResumeLayout(False)
        Me.gbResultsPane.ResumeLayout(False)
        CType(Me.dgv, System.ComponentModel.ISupportInitialize).EndInit()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents gbQueryAnalyzer As System.Windows.Forms.GroupBox
    Friend WithEvents rtbQuery As System.Windows.Forms.RichTextBox
    Friend WithEvents lvTables As System.Windows.Forms.ListView
    Friend WithEvents gbResultsPane As System.Windows.Forms.GroupBox
    Friend WithEvents dgv As System.Windows.Forms.DataGridView
    Friend WithEvents btnExec As System.Windows.Forms.Button
    Friend WithEvents Table As System.Windows.Forms.ColumnHeader
    Friend WithEvents pbSync As System.Windows.Forms.ProgressBar
    Friend WithEvents lblSync As System.Windows.Forms.Label
    Friend WithEvents tSync As System.Windows.Forms.Timer
End Class
