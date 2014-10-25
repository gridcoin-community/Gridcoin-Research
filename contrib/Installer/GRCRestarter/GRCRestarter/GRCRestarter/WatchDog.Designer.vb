<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class WatchDog
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
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(WatchDog))
        Me.DoggieBox = New System.Windows.Forms.ListBox()
        Me.Timer1 = New System.Windows.Forms.Timer(Me.components)
        Me.SuspendLayout()
        '
        'DoggieBox
        '
        Me.DoggieBox.BackColor = System.Drawing.Color.Black
        Me.DoggieBox.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(192, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.DoggieBox.FormattingEnabled = True
        Me.DoggieBox.Location = New System.Drawing.Point(2, 1)
        Me.DoggieBox.Name = "DoggieBox"
        Me.DoggieBox.Size = New System.Drawing.Size(574, 511)
        Me.DoggieBox.TabIndex = 0
        '
        'Timer1
        '
        Me.Timer1.Enabled = True
        Me.Timer1.Interval = 90000
        '
        'WatchDog
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.BackgroundImage = CType(resources.GetObject("$this.BackgroundImage"), System.Drawing.Image)
        Me.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch
        Me.ClientSize = New System.Drawing.Size(577, 213)
        Me.Controls.Add(Me.DoggieBox)
        Me.DoubleBuffered = True
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.Name = "WatchDog"
        Me.Text = "WatchDog"
        Me.ResumeLayout(False)

    End Sub
    Friend WithEvents DoggieBox As System.Windows.Forms.ListBox
    Friend WithEvents Timer1 As System.Windows.Forms.Timer
End Class
