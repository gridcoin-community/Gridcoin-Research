<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> Partial Class frmGRCWireFrameCanvas
    '  Inherits CodeArchitects.VB6Library.VB6Form
    Inherits System.Windows.Forms.Form


#Region "Static constructor"

    ' This static constructor ensures that the VB6 support library be initialized before using current class.
    Shared Sub New()
        EnsureVB6LibraryInitialization()
    End Sub

#End Region

#Region "Windows Form Designer generated code "

    <System.Diagnostics.DebuggerNonUserCode()> Public Sub New()
        MyBase.New()
        'Create all controls and control arrays.
        'InitializeComponent()
    End Sub

    ' This method wraps the call to InitializeComponent, but can be called from base class.
    ' <System.Diagnostics.DebuggerNonUserCode()> Protected Overrides Sub InitializeComponents()
    '     Me.ObjectIsInitializing = True
    '   InitializeComponent()
    ' ' Initialize control arrays.
    '
    ' '    Me.ObjectIsInitializing = False
    ' End Sub

    'Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()> Protected Overloads Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing Then
                If components IsNot Nothing Then components.Dispose()
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
    <System.Diagnostics.DebuggerStepThrough()> Private Sub InitializeComponent()
        '  Me.SuspendLayout()
        '
        'frmGRCWireFrameCanvas
        '
        Me.AutoSize = True
        Me.BackColor = System.Drawing.Color.WhiteSmoke
        Me.ClientSize = New System.Drawing.Size(802, 472)
        Me.Location = New System.Drawing.Point(4, 50)
        Me.MinimizeBox = False
        Me.Name = "frmGRCWireFrameCanvas"

        'Me.ScaleMode = CodeArchitects.VB6Library.VBRUN.ScaleModeConstants.vbPixels

        Me.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen
        Me.Text = "GRC Wire Frame Rendering Subsystem"
        Me.ResumeLayout(False)

    End Sub



#End Region


End Class
